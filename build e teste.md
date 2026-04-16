make
scp blinker-rpi4.ko pi@IP_DO_PI:/home/pi/
ssh pi@IP_DO_PI

sudo insmod blinker-rpi4.ko
lsmod | grep blinker
cat /dev/blinker
echo 1000000000 | sudo tee /dev/blinker
dmesg | tail
sudo rmmod blinker_rpi4