#!/bin/bash

config-pin P9_16 pwm
cd /sys/class/pwm/pwmchip4/pwm-4:1


echo 100000000 >  period
cat period
echo 50000000 >  duty_cycle
cat duty_cycle

echo 1 > enable


cd
cd /sys/class/gpio/gpio68
echo rising > edge
cat edge

echo "PWM generation and GPIO configuration completed."
