#!/bin/sh
set -e
modprobe videobuf2-common
modprobe videobuf2-memops
modprobe videobuf2-v4l2
modprobe videobuf2-vmalloc
modprobe videobuf2-dma-contig
modprobe videobuf2-dma-sg
modprobe videodev
modprobe media
modprobe snd
modprobe snd-pcm
insmod driver/cx511h.ko
echo "cx511h loaded successfully"
