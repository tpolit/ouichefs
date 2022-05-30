#!/bin/bash
cd ..
insmod ouichefs.ko
cd mkfs
mount -oloop -t ouichefs ouichefs.img dir
sleep 2
cd /share/ouichefs-master/ioctl
insmod ex34.ko
mknod /dev/current_view c 248 0