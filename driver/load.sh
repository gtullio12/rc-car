#!/bin/bash
sudo rmmod car_gpio 2>/dev/null
sudo insmod car_gpio.ko
MAJOR=$(awk '$2=="car_gpio" {print $1}' /proc/devices)
sudo rm -f /dev/car_gpio
sudo mknod /dev/car_gpio c $MAJOR 0
sudo chmod 666 /dev/car_gpio
echo "car_gpio loaded, major $MAJOR"
