COMPATIBLE_MACHINE = "(stm32mpcommon)"

#for EFI
include ${@bb.utils.contains('MACHINE_FEATURES', 'efi', 'conf/image-uefi.conf', '', d)}
inherit kernel

DEPENDS += "openssl-native util-linux-native libyaml-native"

B = "${WORKDIR}/build"
# Configure build dir for externalsrc class usage through devtool
EXTERNALSRC_BUILD:pn-${PN} = "${WORKDIR}/build"

# To share config fragments between layers
FILESEXTRAPATHS:prepend := "${THISDIR}:"

do_deploy[sstate-outputdirs] = "${DEPLOY_DIR_IMAGE}/kernel"

#---------------------------------------------------------------
# Module kernel signature
KERNEL_SIGN_ENABLE ?= "0"
EXTRA_OEMAKE += "${@oe.utils.ifelse(d.getVar('KERNEL_SIGN_ENABLE') == '1', 'INSTALL_MOD_STRIP=1','')}"

# Deploy kernel config file to deploy folder
KERNEL_CONFIG_DEPLOY ??= "0"

# ---------------------------------------------------------------------
# Defconfig
#
#If the defconfig is contained on the kernel tree (arch/${ARCH}/configs)
#you must use the following line
do_configure:append() {
    unset CFLAGS CPPFLAGS CXXFLAGS LDFLAGS MACHINE
    if [ ! -z ${KERNEL_DEFCONFIG} ];
    then
        bbnote "Kernel customized: configuration of linux STM by using DEFCONFIG: ${KERNEL_DEFCONFIG}"
        oe_runmake ${PARALLEL_MAKE} -C ${S} O=${B} CC="${KERNEL_CC}" LD="${KERNEL_LD}" ${KERNEL_DEFCONFIG}
    else
        if [ ! -z ${KERNEL_EXTERNAL_DEFCONFIG} ];
        then
            bbnote "Kernel customized: configuration of linux STM by using external DEFCONFIG"
            install -m 0644 ${WORKDIR}/${KERNEL_EXTERNAL_DEFCONFIG} ${B}/.config
            yes '' | oe_runmake -C ${S} O=${B} CC="${KERNEL_CC}" LD="${KERNEL_LD}" oldconfig
        else
            bbwarn "You must specify KERNEL_DEFCONFIG or KERNEL_EXTERNAL_DEFCONFIG"
            die "NO DEFCONFIG SPECIFIED"
        fi
    fi

    if [ ! -z "${KERNEL_CONFIG_FRAGMENTS}" ]
    then
        for f in ${KERNEL_CONFIG_FRAGMENTS}
        do
            bbnote "file = $f"
            # Check if the config fragment was copied into the WORKDIR from
            # the OE meta data
            if [ ! -e "$f" ]
            then
                echo "Could not find kernel config fragment $f"
                exit 1
            fi
        done

        bbnote "${S}/scripts/kconfig/merge_config.sh -m -r -O ${B} ${B}/.config ${KERNEL_CONFIG_FRAGMENTS} 1>&2"
        # Now that all the fragments are located merge them.
        (${S}/scripts/kconfig/merge_config.sh -m -r -O ${B} ${B}/.config ${KERNEL_CONFIG_FRAGMENTS} 1>&2 )
    fi

    yes '' | oe_runmake -C ${S} O=${B} CC="${KERNEL_CC}" LD="${KERNEL_LD}" oldconfig
    #oe_runmake -C ${S} O=${B} savedefconfig && cp ${B}/defconfig ${WORKDIR}/defconfig.saved
}

# ---------------------------------------------------------------------
do_install:append() {
    # Install KERNEL_IMAGETYPE for kernel-imagebootfs package
    install -m 0644 ${KERNEL_OUTPUT_DIR}/${KERNEL_IMAGETYPE} ${D}/${KERNEL_IMAGEDEST}

    if ${@bb.utils.contains('MACHINE_FEATURES','gpu','true','false',d)}; then
        # when ACCEPT_EULA are filled
        echo "blacklist etnaviv" > ${D}/${sysconfdir}/modprobe.d/blacklist.conf
    fi
    #snd dependencies
    install -d ${D}/${sysconfdir}/modprobe.d/
    echo "softdep snd-soc-cs42l51 pre: snd-soc-cs42l51-i2c" > ${D}/${sysconfdir}/modprobe.d/stm32mp1-snd.conf

    # efi
    if ${@bb.utils.contains('MACHINE_FEATURES','efi','true','false',d)}; then
        install -d ${D}${EFI_FILES_PATH}
        install -m 0644 ${KERNEL_OUTPUT_DIR}/${KERNEL_IMAGETYPE} ${D}${EFI_FILES_PATH}/${EFI_BOOT_IMAGE}
        install -d ${D}${EFI_PREFIX}/dtb
        install -m 0664 ${D}${EFI_PREFIX}/*.dtb ${D}${EFI_PREFIX}/dtb/
    fi
}
# ---------------------------------------------------------------------
do_deploy:append() {
    if [ ${MODULE_TARBALL_DEPLOY} = "1" ] && (grep -q -i -e '^CONFIG_MODULES=y$' .config); then
        mkdir -p ${D}${root_prefix}/lib
        tar -cvzf $deployDir/modules-stripped-${MODULE_TARBALL_NAME}.tgz --exclude='.debug' -C ${WORKDIR}/package/${root_prefix} lib
        ln -sf modules-stripped-${MODULE_TARBALL_NAME}.tgz $deployDir/modules-stripped-${MODULE_TARBALL_LINK_NAME}.tgz
    fi
    if [ ${KERNEL_CONFIG_DEPLOY} = "1" ] && [ -f "${D}/boot/config-${KERNEL_VERSION}" ]; then
        install -m 0644 ${D}/boot/config-${KERNEL_VERSION} $deployDir
    fi
}
do_deploy[depends] += " virtual/kernel:do_package "

python() {
    fitimage_signed = d.getVar('UBOOT_SIGN_ENABLE')
    devicetree = d.getVar('KERNEL_DEVICETREE').split()
    machine_features = d.getVar('MACHINE_FEATURES')
    if 'fit' in machine_features:
        if fitimage_signed is not None and fitimage_signed == "1":
            if len(devicetree) > 1:
                bb.fatal("The signature of FIT image work only when there is only "
                         "one DEVICE-TREE specified")
}

# ---------------------------------------------------------------------
# Support checking the kernel load address parameter: expecting proper value for ST kernel.
#
python do_loadaddrcheck() {
    if not d.getVar('ST_KERNEL_LOADADDR'):
        bb.fatal('Missing ST_KERNEL_LOADADDR value for ST kernel build: please define it in your machine.')
}

PACKAGES =+ "${KERNEL_PACKAGE_NAME}-headers ${KERNEL_PACKAGE_NAME}-imagebootfs"
FILES:${KERNEL_PACKAGE_NAME}-headers = "${exec_prefix}/src/linux*"
FILES:${KERNEL_PACKAGE_NAME}-image  += "boot/ ${KERNEL_IMAGEDEST}"
FILES:${KERNEL_PACKAGE_NAME}-imagebootfs = "boot/*.dtb boot/${KERNEL_IMAGETYPE}"
FILES:${KERNEL_PACKAGE_NAME}-modules += "${sysconfdir}/modprobe.d"

PACKAGES =+ "${@bb.utils.contains('MACHINE_FEATURES', 'efi', '${KERNEL_PACKAGE_NAME}-imageefi', '', d)} "
FILES:${KERNEL_PACKAGE_NAME}-imageefi = "${EFI_PREFIX}/dtb/ ${EFI_FILES_PATH}"
