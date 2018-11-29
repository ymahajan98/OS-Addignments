#!/bin/bash
fusermount -u mnt
rmdir mnt
mkdir mnt
make clean
make
./objfs mnt -o use_ino
cd ./test
make clean
make
#mount | grep objfs
