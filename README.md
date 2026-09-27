# RC Car

Remote control ride-on car retrofitted with a Raspberry Pi. Wrote a kernel
device driver to expose GPIO to userspace, and a daemon that reads
the RC receiver and drives an H-bridge motor driver off of it. Comes up
automatically at boot via systemd.

Found a donated RC car with no remote control and decided to rip out the electronics and replace myself. 

## Why a kernel driver instead of just doing this in userspace

The point was to write one. this isn't the easiest way to blink a
motor, it's a way to actually touch the kernel/userspace boundary — char
device registration, file_operations, copy_to_user/copy_from_user, the GPIO
descriptor API.

## how it works

```
RC transmitter
      |
      v
RC receiver --(4 channels: fwd, rev, left, right)--> Pi GPIO in
                                                          |
                                                          v
                                              car_gpio.ko (kernel driver)
                                                /dev/car_gpio
                                                          |
                                                          v
                                                  rc_daemon (userspace)
                                          reads input, applies inversion/
                                          policy, writes output
                                                          |
                                                          v
                                              car_gpio.ko (kernel driver)
                                                          |
                                                          v
                                    Pi GPIO out --> MX1508 dual H-bridge
                                                          |
                                                +---------+---------+
                                                v                   v
                                          drive motor         steering motor
```

driver is dumb on purpose. it registers pins as input/output and moves bits
between kernel and userspace, nothing else. All the actual logic — what
"forward" means, inverting signals, whatever — lives in the daemon. First
version had that stuff baked into the driver with named descriptors
(rc_car_forward, rc_car_reverse, etc) and inversion logic in-kernel. Pulled
that out once it was obvious a driver that knows about "forward" isn't
really a driver anymore, it's the daemon wearing a kernel module as a
costume.

## the driver (car_gpio.c)

One char device, `/dev/car_gpio`, backed by two GPIO arrays:

```c
static int input_gpio_numbers[NUM_INPUT_PINS]   = {GPIO5, GPIO6, GPIO13, GPIO19};
static int output_gpio_numbers[NUM_OUTPUT_PINS] = {GPIO20, GPIO21, GPIO26, GPIO16};
```

- read: samples all input pins, returns them as a string of 0s and 1s
- write: takes a string of 0s and 1s, sets the output pins to match
- uses the gpiod descriptor API (gpio_to_desc, gpiod_direction_input/output,
  gpiod_get/set_value).
- on module exit, zeroes every output pin before freeing it, so unloading
  the module can't leave a motor on

major/minor numbers are allocated dynamically (alloc_chrdev_region +
cdev_add) instead of hardcoded, so load.sh pulls the major number out of
/proc/devices to make the device node.

## the daemon (rc_daemon.c)

opens /dev/car_gpio, loops:

1. read the 4 input channels
2. invert forward/reverse (receiver reads backwards from what you'd expect),
   pass left/right straight through
3. write the result back

### the bug that actually mattered

`systemctl stop` sends SIGTERM. if that lands mid-write, the daemon dies and
whatever was last written to the GPIO pins just... stays there. which means
a motor can be left running with nothing alive to stop it. This wasn't
theoretical, it happened.

Fixed with signal handlers that zero everything before exiting:

```c
void handle_shutdown(int sig) {
    char stop[4] = {'0', '0', '0', '0'};
    write(fd, stop, 4);
    close(fd);
    exit(0);
}

// in main():
signal(SIGTERM, handle_shutdown);
signal(SIGINT, handle_shutdown);
```

Driver also zeroes outputs on exit for the same reason — belt and
suspenders, so neither the daemon dying nor the module unloading can leave
something stuck on.

## boot (systemd)

`rc-car.service` runs `boot_start.sh`, which:

1. runs `load.sh` — insmod the driver, grab the major number it got
   assigned, recreate /dev/car_gpio with the right perms
2. execs the daemon directly so systemd is actually tracking the real
   process, not a wrapper script

```ini
[Unit]
Description=RC Car Control Daemon
After=network.target

[Service]
ExecStart=/home/<user>/rc-car/systemd/boot_start.sh
Restart=always
User=root

[Install]
WantedBy=multi-user.target
```

Restart=always means if the daemon dies it comes back on its own, and
because of the SIGTERM fix above, that restart can't leave things in a
half-stuck state.

## Hardware

- Raspberry Pi Zero
- Generic RC transmitter/receiver, 4 channels (fwd, rev, left, right)
- MX1508 dual H-bridge (drive motor + steering motor)
- battery, buck converter for 5V

### GPIO mapping

| what | direction | GPIO |
|---|---|---|
| RC forward | in | GPIO5 |
| RC reverse | in | GPIO6 |
| RC left | in | GPIO13 |
| RC right | in | GPIO19 |
| motor A (drive) IN1/IN2 | out | GPIO20, GPIO21 |
| motor B (steering) IN3/IN4 | out | GPIO26, GPIO16 |

MX1508 per channel: 00 = off/coast, 10 or 01 = the two directions, 11 =
brake (avoid).

## status

**working end to end on real hardware.** RC input reads correctly, driver
and daemon translate it into motor output correctly, forward/left/right all
confirmed live with an actual transmitter, and it survives a reboot with
zero manual steps — module loads and daemon starts on its own. the SIGTERM
bug above was found and fixed on real hardware, not in a simulator.

**Known issues, not fixed:**
- one MX1508's reverse channel is dead — traced with a multimeter all the
  way from GPIO output to the H-bridge output pin, GPIO levels were correct
  the whole way, so it's the chip, not the code or wiring
- power delivery (buck converter, battery) has caused more problems than
  any of the software. more on that below.

## what this actually demonstrates

- writing a linux char driver from scratch — registration, file_operations,
  the copy_to/from_user boundary, gpiod API
- debugging in kernel space, where a bug hangs the module instead of
  crashing it and printk is the only way to see what's happening
- finding and fixing a real race condition caused by signal handling, on a
  system where "stuck in a bad state" means a motor is still spinning
- driver/daemon split as a design choice, including getting it wrong first
  and refactoring
- systemd integration for a driver + daemon pair, dynamic major number
  handling included
