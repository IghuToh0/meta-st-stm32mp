#@DESCRIPTION: STM32MP machine flashlayout extensible configuration

# Add specific scheme to provide flashlayout that will make rootfs extensible to
# remaining device free space
#   NOTE: this scheme is ONLY compatible with disco board and optee bootscheme
FLASHLAYOUT_BOOTSCHEME_LABELS += "${@bb.utils.contains('BOOTSCHEME_LABELS', 'optee', 'extensible', '', d)}"

# -----------------------------------------------------------------------------
# Define config labels
# -----------------------------------------------------------------------------
FLASHLAYOUT_CONFIG_LABELS:extensible = "${@bb.utils.contains('BOOTDEVICE_LABELS', 'sdcard', 'sdcard', 'none', d)}"

# -----------------------------------------------------------------------------
# Define label types
# -----------------------------------------------------------------------------
FLASHLAYOUT_TYPE_LABELS:extensible = "${@' '.join(dt for dt in '${STM32MP_DT_FILES_SDCARD}'.split() if '-dk' in dt)}"

# -----------------------------------------------------------------------------
# Define partitions to use
#   NOTE: extensible scheme is ONLY compatible with sdcard and optee bootscheme
#   So we only set partition labels for this particular configuration
# -----------------------------------------------------------------------------
FLASHLAYOUT_PARTITION_LABELS:extensible = "\
    ${FLASHLAYOUT_PROGRAMMER_SECTIONS} \
    ${@' '.join(['%s' % l.split(',')[1] for l in d.getVarFlag('PARTITIONS_BOOTLOADER_CONFIG', 'sdcard').split()])} \
    ${@' '.join(['%s' % l.split(',')[1] for l in d.getVarFlag('PARTITIONS_CONFIG', 'sdcard').split()])} \
    "
FLASHLAYOUT_PARTITION_LABELS:extensible:remove = "userfs"

# -----------------------------------------------------------------------------
# The 'extensible' bootscheme is a trick to generate flashlayout files without
# userfs partition for optee bootscheme ONLY. So rootfs partition will be
# extended up to the end of memory device, leaving plenty of space for user to
# install more applications via 'apt-get'
# Make sure to use 'optee' bootscheme for binary naming instead of 'extensible'
# each time it is required
FLASHLAYOUT_PARTITION_REPLACE_PATTERNS:extensible:prepend = "extensible;optee "
