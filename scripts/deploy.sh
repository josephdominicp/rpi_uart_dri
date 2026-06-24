#!/bin/bash

# -----------------------------------------------------------------------------
# Configuration Variables - Update these to match your environment
# -----------------------------------------------------------------------------
RPI_IP="192.168.1.50"       # Replace with your Raspberry Pi's actual IP address
RPI_USER="pi"               # Replace with your Pi's username (e.g., pi or ubuntu)
RPI_DEST="/home/$RPI_USER"  # Target directory on the Raspberry Pi
MODULE_NAME="rpi_uart_driver"

echo "======================================================="
echo " Starting Deployment Pipeline: Host to Raspberry Pi"
echo "======================================================="

# 1. Clean previous build artifacts (optional, but prevents stale cache bugs)
echo "[1/3] Cleaning up old build files..."
make clean > /dev/null 2>&1

# 2. Compile the kernel module using the cross-compilation toolchain
echo "[2/3] Cross-compiling kernel module for ARM64..."
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu-

# Capture the exit status of the 'make' command
if [ $? -ne 0 ]; then
    echo ""
    echo "❌ CRITICAL ERROR: Compilation failed!"
    echo "Fix the syntax errors in your source code before deploying."
    exit 1
fi

# Double-check that the binary actually exists before initiating network transfer
if [ ! -f "src/${MODULE_NAME}.ko" ]; then
    echo ""
    echo "❌ ERROR: Compilation reported success, but src/${MODULE_NAME}.ko was not found."
    exit 1
fi

echo "✅ Compilation successful!"

# 3. Securely transfer the compiled .ko module over the local network
echo "[3/3] Transferring binary to target hardware via SCP..."
echo "Sending src/${MODULE_NAME}.ko ➡️  ${RPI_USER}@${RPI_IP}:${RPI_DEST}"

scp src/${MODULE_NAME}.ko ${RPI_USER}@${RPI_IP}:${RPI_DEST}/

if [ $? -eq 0 ]; then
    echo "======================================================="
    echo " 🎉 SUCCESS: Driver deployed to /home/${RPI_USER}/${MODULE_NAME}.ko"
    echo " Next Steps on your Pi:"
    echo "   1. Unbind native serial: echo \"fe201000.serial\" | sudo tee /sys/bus/amba/drivers/uart-pl011/unbind"
    echo "   2. Insert module:        sudo insmod ${MODULE_NAME}.ko uart_irq=<IRQ>"
    echo "======================================================="
else
    echo ""
    echo "❌ ERROR: Network transfer failed!"
    echo "Check your network connection, Pi IP address, and SSH credentials."
    exit 1
fi