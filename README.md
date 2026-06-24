# Raspberry Pi UART Kernel Driver

A Linux kernel module for Raspberry Pi PL011 UART with interrupt-driven circular buffering and cross-compilation support.

## Key Features

1. **Character Device Interface** - Standard `/dev/rpi_uart` device node for user-space access
2. **Interrupt-Driven I/O** - Event-driven top-half ISR with minimal latency response
3. **Circular Ring Buffer** - 512-byte power-of-2 sized buffer prevents FIFO overruns
4. **Spinlock Protection** - Thread-safe concurrent access with atomic operations
5. **Non-Blocking Reads** - Returns `-EAGAIN` when buffer empty, prevents application hangs
6. **Full Duplex Support** - Both read and write syscalls with timeout handling
7. **Cross-Compilation** - ARM64 toolchain with automated build via Makefile
8. **Automated Deployment** - SCP-based transfer script for seamless Pi deployment
9. **Configurable IRQ** - Dynamic module parameter for interrupt assignment
10. **Production-Ready** - Proper error handling, resource cleanup, and kernel panic prevention

## Overview

This driver demonstrates advanced kernel programming including interrupt handling, memory safety, concurrency control, and hardware abstraction for the ARM PL011 UART peripheral on Raspberry Pi 4/5.

**Target Hardware**: Raspberry Pi 4/5 (BCM2711/BCM2712)  
**Language**: C (77%), Shell scripts (18.7%), Makefile (4.3%)  
**License**: GPL v2.0
