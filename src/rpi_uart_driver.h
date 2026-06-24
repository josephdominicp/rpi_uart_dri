#ifndef RPI_UART_DRIVER_H
#define RPI_UART_DRIVER_H

#include <linux/types.h>

/* Select your active base address (e.g., BCM2711 for Pi 4, BCM2712 for Pi 5) */
#define UART0_PHYS_BASE     0xFE201000  
#define UART_REG_SIZE       0x1000       

/* PL011 Core Register Offsets */
#define UART_DR             0x00   // Data Register (Rx/Tx)
#define UART_FR             0x18   // Flag Register (Hardware Status)
#define UART_CR             0x30   // Control Register
#define UART_IMSC           0x38   // Interrupt Mask Set/Clear Register
#define UART_MIS            0x40   // Masked Interrupt Status Register
#define UART_ICR            0x44   // Interrupt Clear Register

/* PL011 Flag Register (UART_FR) Bitfields */
#define UART_FR_TXFF        (1 << 5) // Bit 5: Transmit FIFO Full flag
#define UART_FR_RXFE        (1 << 4) // Bit 4: Receive FIFO Empty flag

/* PL011 Interrupt Register Bitfields */
#define UART_INT_RX         (1 << 4) // Bit 4: Receive Interrupt

#endif /* RPI_UART_DRIVER_H */