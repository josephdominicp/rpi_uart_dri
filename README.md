# Raspberry Pi PL011 UART Driver

## Summary
A Day 6 production-ready Linux kernel module implementing circular buffer-based UART communication for Raspberry Pi's PL011 controller. This driver provides event-driven interrupt handling with concurrent read/write operations, featuring robust spinlock synchronization to prevent race conditions and data corruption.

---

## 1. Project Overview
This repository contains a complete kernel device driver for the Raspberry Pi's PL011 UART (Universal Asynchronous Receiver/Transmitter). The driver implements Day 6 of an embedded Linux development progression, focusing on circular buffer management, interrupt service routines (ISR), and thread-safe concurrent access patterns. It exposes a character device interface (`/dev/rpi_uart`) for userspace applications to communicate with the UART hardware.

## 2. Key Features
- **Circular Ring Buffer**: 512-byte power-of-2 sized buffer for efficient data streaming
- **Interrupt-Driven Architecture**: Hardware interrupt handler processes incoming UART data atomically
- **Spinlock Synchronization**: `spin_lock_irqsave/spin_unlock_irqrestore` prevents race conditions between ISR and read syscall
- **Non-Blocking I/O**: Returns `-EAGAIN` when no data available, avoiding premature EOF signals
- **Safe Userspace Boundaries**: Correct `copy_to_user/copy_from_user` usage with minimal lock hold times
- **Configurable IRQ**: Runtime module parameter accepts custom UART IRQ number
- **Cross-Platform Build**: ARM64 cross-compilation support via Makefile toolchain

## 3. Hardware Requirements
- Raspberry Pi with PL011 UART controller (tested on Pi 3/4)
- Linux kernel headers matching your Pi's running kernel version
- ARM64 cross-compilation toolchain (`aarch64-linux-gnu-gcc`, `aarch64-linux-gnu-ld`)
- GPIO/UART pins accessible (typically UART0 on GPIO14/GPIO15)

## 4. Architecture & Design Patterns

### Circular Buffer
```
[0][1][2]...[rx_tail]...[rx_head]...[511]
     ↑                    ↑
   Read pointer        Write pointer
```
- **rx_head**: Modified only by ISR (write position)
- **rx_tail**: Modified only by read syscall (read position)
- **buffer_lock**: Spinlock protecting index transitions

### ISR (Interrupt Service Routine)
- Acquires spinlock with interrupts disabled
- Reads bytes from UART FIFO into ring buffer
- Detects buffer overflow and drops bytes gracefully
- Clears hardware interrupt before releasing lock

### Read Syscall
- Acquires spinlock per-character (critical rule: never call `copy_to_user` while holding spinlock)
- Extracts one byte at a time to avoid sleeping with lock held
- Returns number of bytes copied or `-EAGAIN` if buffer empty

## 5. Building & Installation

### Prerequisites
```bash
# On your build host (Ubuntu/Debian)
sudo apt-get install linux-headers-$(uname -r)
sudo apt-get install gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu
```

### Build
```bash
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu-
```

### Deploy
Use the provided deployment script:
```bash
./scripts/deploy.sh
# Update RPI_IP, RPI_USER variables in the script first
```

Or manually:
```bash
scp src/rpi_uart_driver.ko pi@192.168.1.50:/home/pi/
```

## 6. Loading the Module

### On your Raspberry Pi
```bash
# First, unbind the native PL011 driver
echo "fe201000.serial" | sudo tee /sys/bus/amba/drivers/uart-pl011/unbind

# Insert the module (replace <IRQ> with your UART0 IRQ, typically 147 or 153)
sudo insmod rpi_uart_driver.ko uart_irq=147

# Verify it loaded
sudo dmesg | tail -5
```

### Verification
```bash
# Check device node was created
ls -l /dev/rpi_uart

# Test reading
cat /dev/rpi_uart

# Test writing
echo "Hello UART" > /dev/rpi_uart
```

## 7. Concurrency & Locking Strategy

### Race Condition Prevention
| Component | Protection | Lock Type |
|-----------|------------|-----------|
| rx_head (ISR writes) | Spinlock + ISR context | spin_lock_irqsave |
| rx_tail (read syscall) | Spinlock | spin_lock_irqsave |
| Buffer access | Per-char lock release | Atomic operations |

### Critical Invariants
- **Never hold spinlock across page faults** → `copy_to_user` called outside lock
- **Interrupt safety** → `spin_lock_irqsave` disables local interrupts
- **Atomicity** → Ring buffer indices updated under lock protection
- **Non-blocking reads** → Returns `-EAGAIN` rather than blocking on wait queue

## 8. Troubleshooting & Debug

### Common Issues
| Issue | Symptom | Resolution |
|-------|---------|-----------|
| Module fails to load | `insmod: error` in dmesg | Verify uart_irq parameter; check `cat /proc/interrupts` |
| No data received | `cat /dev/rpi_uart` hangs | Check UART pins connected; verify baud rate; use `sudo systemtap` to trace ISR |
| Kernel panic on read | "BUG: scheduling while atomic" | Don't modify locking logic; `copy_to_user` must be outside lock |
| Buffer overflow | Data loss | Increase RING_BUFFER_SIZE (keep power-of-2); optimize ISR latency |

### Debug Output
```bash
# View driver logs
dmesg | grep "RPi UART"

# Monitor interrupts
watch -n 1 'cat /proc/interrupts | grep uart'

# Unload safely
sudo rmmod rpi_uart_driver
```

---

## License
GPL v2 (as declared in module headers)

## Author
Embedded Hardware Engineer

## Version
0.6 - Day 6 Circular Buffering Implementation
