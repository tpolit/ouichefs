#!/bin/bash
echo "Verfying that the blocks are really freed"
cd ../mkfs/dir
touch rewind
sleep 1
echo one > rewind
sleep 1
echo two > rewind
sleep 1
/share/ouichefs-master/ioctl/use_current /share/ouichefs-master/mkfs/dir/rewind 1
sleep 1
/share/ouichefs-master/ioctl/use_rewind /share/ouichefs-master/mkfs/dir/rewind
sleep 1
echo three > rewind
sleep 1
cat /share/ouichefs-master/mkfs/dir/rewind
rm /share/ouichefs-master/mkfs/dir/rewind
