#!/bin/bash -e

export ARCH="arm"
export CROSS_COMPILE="arm-linux-gnueabi-"
export CHIP="hi3516cv610"
export MEDIUM="spi"
HIGZIP_DST="arch/arm/cpu/armv7/${CHIP}/hw_compressed"

declare -A reginfo
reginfo[608]="Hi3516CV608-DMEB_4L_DDR2_1333M_64MB_16bit-A7_950M_QFN.bin"
reginfo[10b]="Hi3516CV610-DMEB_4L_DDR2_1333M_64MB_16bit-A7_950M_QFN.bin"
reginfo[20s]="Hi3516CV610-DMEB_4L_DDR3_2133M_128MB_16bit-A7_950M_QFN.bin"
reginfo[20g]="Hi3516CV610-DMEB_4L_DDR3_2133M_128MB_16bit-A7_950M_QFN.bin"
reginfo[00s]="Hi3516CV610-DMEB_4L_DDR3_2133M_512MB_16bit-A7_950M_BGA.bin"
reginfo[00g]="Hi3516CV610-DMEB_4L_DDR3_2133M_512MB_16bit-A7_950M_BGA.bin"

BUILD_DIR="output"
mkdir ${BUILD_DIR} &>/dev/null || true

build_cv610(){
    for SOCMODEL in 10b 20s 20g 00s 00g; do
        echo $SOCMODEL ${reginfo[$SOCMODEL]}
        
        make distclean
        make ${CHIP}_openipc_defconfig
        make -j`nproc` KCFLAGS="-DPRODUCT_SOC=${CHIP} -DPRODUCT_SOCMODEL=${SOCMODEL} -DVENDOR_HISILICON"
        
        make u-boot-z.bin
        make u-boot-z.clean
        
        cp u-boot-${CHIP}.bin image_tool/input/u-boot-original.bin
        cp reginfo/${reginfo[$SOCMODEL]} image_tool/input/reg_info.bin
        pushd image_tool/oem;python oem_quick_build.py;popd
        mv image_tool/image/oem/boot_image.bin output/boot-${CHIP}-${SOCMODEL}-nor.bin
        
    done
}

build_cv608(){
    CHIP="hi3516cv608"
    make distclean
    make ${CHIP}_openipc_defconfig
    make -j`nproc` KCFLAGS="-DPRODUCT_SOC=${CHIP} -DPRODUCT_SOCMODEL= -DVENDOR_HISILICON"
    
    make u-boot-z.bin
    make u-boot-z.clean
    
    cp u-boot-${CHIP}.bin image_tool/input/u-boot-original.bin
    cp reginfo/${reginfo[608]} image_tool/input/reg_info.bin
    pushd image_tool/oem;python oem_quick_build.py;popd
    mv image_tool/image/oem/boot_image.bin output/boot-${CHIP}-nor.bin
}

build_gsl(){
    pushd gsl; make clean; make CHIP=${CHIP} ;popd
    cp gsl/pub/gsl.bin cp image_tool/input/
}

build_higzip(){
    if [ ! -f ${HIGZIP_DST}/gzip ]; then
        LOCAL_CFLAGS="-fPIC -DWSIZE=0x2000"
        LOCAL_LDFLAGS="-Wl,-z,relro -Wl,-z,now -Wl,-z,noexecstack,-s"
        
        pushd extras/gzip-1.11
        autoreconf -f -i -s && ./configure CFLAGS="${LOCAL_CFLAGS}" LDFLAGS="${LOCAL_LDFLAGS}"
        make -j`nproc`
        popd
        cp extras/gzip-1.11/gzip ${HIGZIP_DST} -f
    fi
}

build_higzip

build_cv610

build_cv608
