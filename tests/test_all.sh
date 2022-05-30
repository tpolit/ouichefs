#!/bin/bash
cd ../mkfs/dir
echo "Creating a file with 3 version."
touch file
sleep 1
echo test1 > file
sleep 1
echo test2 > file
sleep 1
echo test3 > file

cd /share/ouichefs-master/ioctl
sleep 1
echo "Go back by "$1" versions"
./use_current /share/ouichefs-master/mkfs/dir/file $1
sleep 1
echo "Go back by "$2" versions"
./use_current /share/ouichefs-master/mkfs/dir/file $2
sleep 1
echo "Testing rewind"
./use_rewind /share/ouichefs-master/mkfs/dir/file
sleep 1
# cat /sys/kernel/debug/ouichefs/loop0
cat /share/ouichefs-master/mkfs/dir/file
rm /share/ouichefs-master/mkfs/dir/file # pour eviter d'avoir le fichier dans le prochain test, sinon le prev = 0, et plusieurs erreurs possibles;