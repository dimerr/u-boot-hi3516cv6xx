#define MTDIDS "sfc"
#define VENDOR "hisilicon"

#define CONFIG_SYS_CBSIZE 1024

#define CONFIG_IPADDR 192.168.1.108
#define CONFIG_NETMASK 255.255.255.0
#define CONFIG_GATEWAYIP 192.168.1.1
#define CONFIG_SERVERIP 192.168.1.107

#ifdef CONFIG_MMC
#define CONFIG_CMD_MMC
#endif

#define CONFIG_BOOTARGS "mem=\\${osmem} console=ttyAMA0,115200 panic=20 root=/dev/mtdblock3 rootfstype=squashfs init=/init mtdparts=\\${mtdids}:\\${mtdparts} \\${extras}"
#define CONFIG_BOOTCOMMAND "setenv bootcmd ${bootnor}; sf probe 0; saveenv; run bootcmd"
#define MTDPARTS "256k(boot),64k(env),2048k(kernel),5120k(rootfs),7168k@0x50000(firmware),-(rootfs_data)"

#define CONFIG_EXTRA_ENV_SETTINGS \
	"baseaddr=" __stringify(CONFIG_SYS_LOAD_ADDR) "\0" \
	"kernaddr=0x50000\0" \
	"kernsize=0x200000\0" \
	"rootaddr=0x250000\0" \
	"rootsize=0x500000\0" \
	"bootnor=run fwrecovery; sf probe 0; setenv setargs setenv bootargs ${bootargs}; run setargs; sf read ${baseaddr} ${kernaddr} ${kernsize}; bootm ${baseaddr}\0" \
	"ubnor=${fetchcmd} ${baseaddr} u-boot-${soc}-nor.bin && run ubwrite\0" \
	"ubwrite=sf probe 0; sf erase 0x0 ${kernaddr}; sf write ${baseaddr} 0x0 ${kernaddr}\0" \
	"fwupd=${fetchcmd} ${baseaddr} firmware.${soc} && run fwwrite\0" \
	"fwrecovery=if env exists bootfail; then run fwupd; sleep 5; reset; fi\0" \
	"fwwrite=sf probe 0; sf erase ${kernaddr} ${filesize}; sf write ${baseaddr} ${kernaddr} ${filesize}\0" \
	"nfsroot=/srv/nfs/" __stringify(PRODUCT_SOC) "\0" \
	"bootargsnfs=mem=\${osmem} console=ttyAMA0,115200 panic=20 root=/dev/nfs rootfstype=nfs ip=${ipaddr}:::255.255.255.0::eth0 nfsroot=${serverip}:${nfsroot},v3,nolock rw \${extras}\0" \
	"bootnfs=setenv setargs setenv bootargs ${bootargsnfs}; run setargs; tftpboot ${baseaddr} uImage.${soc}; bootm ${baseaddr}\0" \
	"sdload=setenv fetchcmd fatload mmc 0\0" \
	"norload=sf probe 0; sf read\0" \
	"fetchcmd=tftpboot\0" \
	"osmem=32M\0" \
	"mtdids=" MTDIDS "\0" \
	"mtdparts=\${mtdids}:" MTDPARTS "\0" \
	"bootargs=" CONFIG_BOOTARGS "\0" \
	"board_name\0" \
	"board=" __stringify(PRODUCT_SOC) "\0" \
	"vendor=" VENDOR "\0" \
	"soc=" __stringify(PRODUCT_SOC) "\0" \
	"socmodel=" __stringify(PRODUCT_SOCMODEL)

#ifdef CONFIG_FMC_SPI_NAND1

#define MTDIDS "nand"

#define CONFIG_BOOTARGS "mem=\\${osmem} console=ttyAMA0,115200 panic=20 init=/init root=/dev/ubiblock0_1 ubi.mtd=2,2048 ubi.block=0,1 mtdparts=\\${mtdids}:\\${mtdparts} \\${extras}"
#define CONFIG_BOOTCOMMAND "setenv setargs setenv bootargs ${bootargs}; run setargs; ubi part ubi; ubi read ${baseaddr} kernel; bootm ${baseaddr}; reset"

#define CONFIG_EXTRA_ENV_SETTINGS \
	"baseaddr=" __stringify(CONFIG_SYS_LOAD_ADDR) "\0" \
	"fwupd=${fetchcmd} ${baseaddr} firmware.ubi.${soc} && nand erase 0x100000 0x7f00000; nand write ${baseaddr} 0x100000 ${filesize}\0" \
	"mtdparts=mtdparts="MTDIDS":768k(boot),256k(env),-(ubi)\0" \
	"nfsroot=/srv/nfs/" __stringify(PRODUCT_SOC) "\0" \
	"bootargsnfs=mem=\${osmem} console=ttyAMA0,115200 panic=20 root=/dev/nfs rootfstype=nfs ip=${ipaddr}:::255.255.255.0::eth0 nfsroot=${serverip}:${nfsroot},v3,nolock rw \${extras}\0" \
	"bootargs="CONFIG_BOOTARGS"\0" \
	"bootnfs=setenv setargs setenv bootargs ${bootargsnfs}; run setargs; tftpboot ${baseaddr} uImage.${soc}; bootm ${baseaddr}\0" \
	"osmem=32M\0" \
	"mtdids=nand0="MTDIDS"\0" \
	"mtdparts=\${mtdids}:" MTDPARTS "\0" \
	"board_name\0" \
	"board=" __stringify(PRODUCT_SOC) "\0" \
	"vendor=" VENDOR "\0" \
	"soc=" __stringify(PRODUCT_SOC) "\0" \
	"socmodel=" __stringify(PRODUCT_SOCMODEL)

#define CONFIG_SYS_MALLOC_LEN (32 * SZ_128K)
#endif
