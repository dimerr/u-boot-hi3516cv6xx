################################################################################

PWD           = $(shell pwd)
TOPDIR        =

################################################################################
BOOT     := u-boot-$(VENDER_CHIP)
TEXTBASE := 0x48700000
CFLAGS   :=$(KBUILD_CFLAGS) -fno-builtin -ffreestanding \
	-D__KERNEL__ -DTEXT_BASE=$(TEXTBASE) \
	-I$(TOPDIR)/include \
	-I$(TOPDIR)/include/linux \
	-I$(TOPDIR)/arch/arm/include \
	-I$(TOPDIR)/drivers/mmc \
	-fno-pic  -mstrict-align  -ffunction-sections \
	-fdata-sections -fno-common -ffixed-r9    \
	-fno-common -ffixed-x18 -pipe -march=armv8-a \
	-Wall -Wstrict-prototypes -fno-stack-protector \
	-D__LINUX_ARM_ARCH__=8 -D__ARM__ \
	-DCONFIG_HI3519DV500 -DCONFIG_MMC -DDDR_TRAINING_MEM_FUNC \
	$(MKFLAGS) -fno-strict-aliasing

################################################################################

START := start.o
COBJS := uart.o \
	startup.o \
	image_data.o \
	div0.o \
	reset.o

SSRC  := arch/arm/cpu/armv8/$(SOC)/start.S \
	arch/arm/cpu/armv8/$(SOC)/reset.S \
	arch/arm/cpu/armv8/$(SOC)/uart.S \
	arch/arm/lib/div0.c \
	lib/hw_dec/hw_decompress.c \
	lib/hw_dec/hw_decompress_$(SOC).c \
	lib/hw_dec/hw_decompress_v1.c \
	lib/hw_dec/hw_decompress_v1.h

SRC := $(notdir $(SSRC))

################################################################################
.PHONY: $(BOOT).bin
$(BOOT).bin: $(BOOT).tmp
	@cp ./$(BOOT).tmp $(TOPDIR)/$(BOOT).bin
	@echo $(BOOT).bin is Ready.

$(BOOT).tmp: $(BOOT).elf
ifneq ($(LLVM),)
	$(OBJCOPY) -j .text -O binary $< $(BOOT).text
	$(OBJCOPY) -O binary $< $@
else
	$(OBJCOPY) -O srec $< $(BOOT).srec
	$(OBJCOPY) -j .text -O binary $< $(BOOT).text
	$(OBJCOPY) --gap-fill=0xff -O binary $< $@
endif
$(BOOT).elf: image_data.gzip $(SRC) $(START) $(COBJS)
	$(LD) -Bstatic -T u-boot.lds -Ttext $(TEXTBASE) $(START) \
		$(COBJS) -Map $(BOOT).map -o $@
	$(OBJDUMP) -d  $@ > $@.asm

################################################################################
start.o: start.S
	$(CC) -D__ASSEMBLY__ $(CFLAGS) -o $@ $< -c

# -1 : --fast      -9 : --best
image_data.gzip: $(BINIMAGE)
	./gzip -fNqc -7 $< > $@

%.o: %.c
	$(CC) $(CFLAGS) -Wall -Wstrict-prototypes \
		-fno-stack-protector -o $@ $< -c

%.o: %.S
	$(CC) -D__ASSEMBLY__ $(CFLAGS) -o $@ $< -c

image_data.o: image_data.S image_data.gzip
	$(CC) -D__ASSEMBLY__ $(CFLAGS) -o $@ $< -c

#############################################################################

$(SRC):
	ln -sf ../../../../../../$(filter %/$@,$(SSRC)) $@
################################################################################
TMPS := $(COBJS) start.o $(SRC) \
	$(BOOT).map $(BOOT).elf $(BOOT).srec $(BOOT).bin $(BOOT).text $(BOOT).tmp \
	image_data.gzip

distclean: clean

clean:
	-rm -f $(TMPS)
	-rm -f $(find ./ -type l)
	-rm -f u-boot-$(VENDER_CHIP).bin u-boot-$(VENDER_CHIP).elf u-boot-$(VENDER_CHIP).elf.asm \
		u-boot-$(VENDER_CHIP).map u-boot-$(VENDER_CHIP).srec u-boot-$(VENDER_CHIP).text u-boot-$(VENDER_CHIP).tmp

################################################################################
.PHONY: clean
################################################################################
