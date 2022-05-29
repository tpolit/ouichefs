#!/bin/bash
insmod ouichefs.ko
cd mkfs
mount -oloop -t ouichefs ouichefs.img dir

sleep 1
cd dir
sleep 1
echo test1 > file
sleep 1
echo test2 > file
sleep 1
echo test3 > file

cd /share/ouichefs-master/ioctl
insmod ex34.ko
mknod /dev/current_view c 248 0
cat /sys/kernel/debug/ouichefs/loop0
sleep 1
./use_current /share/ouichefs-master/mkfs/dir/file 1
sleep 1
./use_rewind /share/ouichefs-master/mkfs/dir/file
sleep 1
cat /sys/kernel/debug/ouichefs/loop0
cat /share/ouichefs-master/mkfs/dir/file
rm /share/ouichefs-master/mkfs/dir/file # pour eviter d'avoir le fichier dans le prochain test, sinon le prev = 0, et plusieurs erreurs possibles;