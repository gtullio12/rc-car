#!/bin/bash
# /home/rc-car/car_gpio/systemd/boot_start.sh
/home/rc-car/car_gpio/driver/load.sh
exec /home/rc-car/car_gpio/daemon/rc_daemon
