
include $(src)/version.mak
PLATFORM_CPPFLAGS += -DSDHCI_NEBULA_UBOOT_VERSION='${SDHCI_NEBULA_UBOOT_VERSION}'

PLATFORM_CPPFLAGS += -I$(src)
PLATFORM_CPPFLAGS += -I$(srctree)/drivers/mmc/

obj-$(CONFIG_MMC_SDHCI_NEBULA)	+= sdhci_nebula.o nebula_quick.o nebula_cmd.o
obj-$(CONFIG_MMC_SDHCI_NEBULA)	+= ext_sdhci.o ext_mmc.o
obj-$(CONFIG_MMC_SDHCI_NEBULA)	+= platform/platform_comm.o

obj-$(CONFIG_TARGET_HI3519DV500_FAMILY)	+= platform/sdhci_hi3519dv500.o
obj-$(CONFIG_TARGET_HI3516CV610_FAMILY)	+= platform/sdhci_hi3516cv610.o
obj-$(CONFIG_CPU_WUDANGSTICK)	        += platform/wudangstick_ext.o
