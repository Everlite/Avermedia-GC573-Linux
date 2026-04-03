MODULE_NAME := cx511h



# $(src) is set by kbuild to the absolute source directory of this module.
# $(shell pwd) / $(CURDIR) resolve to the *kernel build directory* during
# kbuild and must NOT be used for include paths or obj references.

common-include := -I$(src) \
                  -I$(src)/include \
                  -I$(src)/include/uapi \
                  -I$(src)/utils/alsa \
                  -I$(src)/utils/alsa/include \
                  -I$(src)/utils/gpio \
                  -I$(src)/utils/gpio/include \
                  -I$(src)/utils/i2c \
                  -I$(src)/utils/i2c/include \
                  -I$(src)/utils/mem \
                  -I$(src)/utils/mem/include \
                  -I$(src)/utils/misc \
                  -I$(src)/utils/misc/include \
                  -I$(src)/utils/pci \
                  -I$(src)/utils/pci/include \
                  -I$(src)/utils/thread \
                  -I$(src)/utils/thread/include \
                  -I$(src)/utils/trace \
                  -I$(src)/utils/trace/include \
                  -I$(src)/utils/v4l2 \
                  -I$(src)/utils/v4l2/include \
                  -I$(src)/board/cx511h

basic-objs := entry.o cxt_mgr.o
utils_objs := utils/alsa/alsa_model.o \
              utils/gpio/gpio_model.o \
              utils/i2c/i2c_model.o \
              utils/mem/mem_model.o \
              utils/misc/sys.o \
              utils/misc/g_queue.o \
              utils/pci/pci_model.o \
              utils/thread/task_model.o \
              utils/trace/trace.o \
              utils/trace/debug.o \
              utils/v4l2/v4l2_model_device.o \
              utils/v4l2/v4l2_model_ioctl.o \
              utils/v4l2/v4l2_model_videobuf2.o \
              utils/v4l2/v4l2_model_table.o \
              utils/v4l2/framegrabber.o

board_objs := board/cx511h/board_config.o \
              board/cx511h/board_i2c.o \
              board/cx511h/board_gpio.o \
              board/cx511h/board_v4l2.o \
              board/cx511h/board_alsa.o

$(MODULE_NAME)-objs := $(basic-objs) $(utils_objs) $(board_objs) AverMediaLib_64.o

# ccflags-y is the modern kbuild variable (EXTRA_CFLAGS is deprecated since 5.x)
ccflags-y += $(common-include) -Wno-maybe-uninitialized -Wno-implicit-fallthrough
# Clang-specific: vendor code uses intentional enum cross-casts and callback-only functions
ccflags-y += -Wno-implicit-enum-enum-cast -Wno-missing-prototypes -Wno-unused-variable

# AverMediaLib_64.a is a precompiled blob without CET/ENDBR64 support.
# Disable CET code generation so indirect calls into the blob don't trap
# on kernels with Intel IBT enabled.
KBUILD_CFLAGS += -fcf-protection=none
KBUILD_CFLAGS += -fno-stack-protector

obj-m += $(MODULE_NAME).o

# Note: cx511h depends on videobuf2 modules (common, memops, v4l2, vmalloc, dma-contig, dma-sg)
# and sound modules (snd, snd-pcm). Ensure they are loaded before insmod.
KDIR := /lib/modules/$(shell uname -r)/build

all:
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean
	rm -f AverMediaLib_64.o .AverMediaLib_64.o.cmd
	rm -f *.ko *.mod.c *.mod .tmp_versions

# Explicit kbuild rule: produce AverMediaLib_64.o from the pre-compiled blob.
# $(src) is set by kbuild to the absolute source directory during the
# recursive make invocation.  Without this rule, kbuild tries to compile
# AverMediaLib_64.o from a non-existent .c file.
$(obj)/AverMediaLib_64.o: $(src)/../AverMediaLib_64.a
	cp $< $@
	@echo 'cmd_$@ := cp $< $@' > $(obj)/.AverMediaLib_64.o.cmd
