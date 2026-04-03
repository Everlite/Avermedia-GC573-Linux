#!/bin/sh
set -e

# Load sound modules first
modprobe snd
modprobe snd-pcm

# Load videobuf2 core modules in dependency order
modprobe videobuf2-common
modprobe videobuf2-memops
modprobe videobuf2-v4l2

# Load memory allocators (provide vb2_*_memops symbols)
modprobe videobuf2-vmalloc
modprobe videobuf2-dma-contig
modprobe videobuf2-dma-sg

# Video core
modprobe videodev
modprobe media

# Insert our module
insmod cx511h.ko

echo "cx511h module loaded successfully"
