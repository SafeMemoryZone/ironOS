#!/bin/sh
set -e

# Sanity checks

# Check if we are running in the correct environment
if [ -z "$BUILD_DIR" ] || [ -z "$OUT" ] || [ -z "$LIMINE_DATADIR" ]; then
    echo "Error: Required environment variables are missing."
    echo "  BUILD_DIR:      '${BUILD_DIR}'"
    echo "  OUT:            '${OUT}'"
    echo "  LIMINE_DATADIR: '${LIMINE_DATADIR}'"
    exit 1
fi

# Check if the kernel binary exists
KERNEL_BIN="${BUILD_DIR}/${OUT}"
if [ ! -f "$KERNEL_BIN" ]; then
    echo "Error: Kernel binary not found at:"
    echo "  $KERNEL_BIN"
    exit 1
fi

# Check if external tools are available
if ! command -v xorriso >/dev/null 2>&1; then
    echo "Error: 'xorriso' is not installed or not in PATH."
    exit 1
fi

# Define paths based on environment variables
ISO_NAME="${ISO_NAME:-ironOS.iso}"
ISO_ROOT="${BUILD_DIR}/iso-root"
KERNEL_BIN="${BUILD_DIR}/${OUT}"

# Create directory structure
mkdir -p "${ISO_ROOT}/boot/limine"
mkdir -p "${ISO_ROOT}/EFI/BOOT"

# Copy the kernel
cp -v "${KERNEL_BIN}" "${ISO_ROOT}/boot/ironOS"

# Copy limine config
if [ -f "limine.conf" ]; then
    cp -v limine.conf "${ISO_ROOT}/boot/limine/"
else
    echo "Error: limine.conf not found in project root."
    exit 1
fi

# Copy limine binaries
cp -v "${LIMINE_DATADIR}/limine-bios.sys" \
      "${LIMINE_DATADIR}/limine-bios-cd.bin" \
      "${LIMINE_DATADIR}/limine-uefi-cd.bin" \
      "${ISO_ROOT}/boot/limine/"

cp -v "${LIMINE_DATADIR}/BOOTX64.EFI" "${ISO_ROOT}/EFI/BOOT/"
cp -v "${LIMINE_DATADIR}/BOOTIA32.EFI" "${ISO_ROOT}/EFI/BOOT/"

# Create ISO file
xorriso -as mkisofs -R -r -J -b boot/limine/limine-bios-cd.bin \
        -no-emul-boot -boot-load-size 4 -boot-info-table -hfsplus \
        -apm-block-size 2048 --efi-boot boot/limine/limine-uefi-cd.bin \
        -efi-boot-part --efi-boot-image --protective-msdos-label \
        "${ISO_ROOT}" -o "${BUILD_DIR}/${ISO_NAME}"

# Install MBR for BIOS booting
"${LIMINE}" bios-install "${BUILD_DIR}/${ISO_NAME}"
