#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>        
#include <linux/cdev.h>      
#include <linux/device.h>    
#include <linux/io.h>        
#include <linux/uaccess.h>   
#include <linux/interrupt.h> 
#include <linux/spinlock.h>  // Mandatory header for spin_lock architectures

#include "rpi_uart_driver.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Embedded Hardware Engineer");
MODULE_DESCRIPTION("Raspberry Pi PL011 UART Driver - Day 6 Circular Buffering");
MODULE_VERSION("0.6");

#define DEVICE_NAME "rpi_uart"
#define CLASS_NAME  "rpi_uart_class"
#define KERNEL_BUFFER_SIZE 256
#define RING_BUFFER_SIZE   512  // Must be a power of 2 for optimal sizing efficiency

/* Runtime Module Parameter */
static int uart_irq = -1;
module_param(uart_irq, int, 0444);

/* Hardware & Driver Tracking Context */
static void __iomem *uart_base = NULL;
static dev_t dev_num;                  
static struct cdev uart_cdev;          
static struct class *uart_class = NULL; 
static struct device *uart_device = NULL;

/* Day 6 Telemetry: Circular Ring Buffer Infrastructure */
static char rx_ring_buffer[RING_BUFFER_SIZE];
static unsigned int rx_head = 0; // Shared resource: Modified only by the ISR
static unsigned int rx_tail = 0; // Shared resource: Modified only by the Read Syscall

/* Concurrency Control: Spinlock protecting the ring buffer indices */
static DEFINE_SPINLOCK(buffer_lock);

/**
 * rpi_uart_isr - Day 6 Event-Driven Top-Half Handler
 * Pushes incoming streaming data into the circular queue atomically.
 */
static irqreturn_t rpi_uart_isr(int irq, void *dev_id)
{
    u32 interrupt_status;
    irqreturn_t status = IRQ_NONE;
    unsigned long flags;

    interrupt_status = ioread32(uart_base + UART_MIS);

    if (interrupt_status & UART_INT_RX) {
        status = IRQ_HANDLED;

        /* Acquire the spinlock before touching shared head/tail variables.
         * spin_lock_irqsave disables local interrupts on the current CPU core 
         * and prevents concurrent execution race states.
         */
        spin_lock_irqsave(&buffer_lock, flags);

        while (!(ioread32(uart_base + UART_FR) & UART_FR_RXFE)) {
            u32 raw_data = ioread32(uart_base + UART_DR);
            char ch = (char)(raw_data & 0xFF);
            
            /* Calculate the next hypothetical head pointer location */
            unsigned int next_head = (rx_head + 1) % RING_BUFFER_SIZE;

            if (next_head != rx_tail) {
                /* Safe: Space is available in the circular queue */
                rx_ring_buffer[rx_head] = ch;
                rx_head = next_head;
            } else {
                /* Buffer Overflow Condition: Drop incoming byte to prevent memory corruption */
                // Note: Do NOT call pr_err inside this loop; it defeats the purpose of the buffer.
            }
        }

        spin_unlock_irqrestore(&buffer_lock, flags);

        /* Clear the hardware interrupt line */
        iowrite32(UART_INT_RX, uart_base + UART_ICR);
    }

    return status;
}

static int rpi_uart_open(struct inode *inodep, struct file *filep)
{
    return 0;
}

static int rpi_uart_release(struct inode *inodep, struct file *filep)
{
    return 0;
}

/**
 * rpi_uart_read - Day 6 Implemented System Call Interface
 * Safely extracts bytes out of the circular buffer and passes them to user space.
 */
static ssize_t rpi_uart_read(struct file *filep, char __user *buffer, size_t len, loff_t *offset)
{
    unsigned long flags;
    size_t bytes_read = 0;
    char local_ch;
    bool data_available;

    if (len == 0) return 0;

    /* Process characters sequentially up to the length requested by the user */
    while (bytes_read < len) {
        
        /* CRITICAL DESIGN RULE: You cannot hold a spinlock while calling copy_to_user().
         * copy_to_user() can trigger a page fault, causing the process to sleep.
         * Sleeping while holding a spinlock results in a fatal kernel panic.
         * Therefore, we lock briefly, grab exactly ONE character, and unlock immediately.
         */
        spin_lock_irqsave(&buffer_lock, flags);
        
        if (rx_head != rx_tail) {
            /* Ring buffer is NOT empty */
            local_ch = rx_ring_buffer[rx_tail];
            rx_tail = (rx_tail + 1) % RING_BUFFER_SIZE;
            data_available = true;
        } else {
            /* Ring buffer is empty */
            data_available = false;
        }
        
        spin_unlock_irqrestore(&buffer_lock, flags);

        /* Break out of the loop if no more characters are currently cached */
        if (!data_available) {
            break;
        }

        /* Safely push the isolated single byte across the user-space boundary */
        if (copy_to_user(buffer + bytes_read, &local_ch, 1)) {
            return -EFAULT;
        }

        bytes_read++;
    }

    /* Return total count read. If buffer was empty from the start, returns 0 (EOF) */
    return bytes_read;
}

static ssize_t rpi_uart_write(struct file *filep, const char __user *buffer, size_t len, loff_t *offset)
{
    char kbuf[KERNEL_BUFFER_SIZE];
    size_t bytes_to_copy;
    size_t bytes_written = 0;
    size_t i;

    while (bytes_written < len) {
        bytes_to_copy = min(len - bytes_written, (size_t)KERNEL_BUFFER_SIZE);
        if (copy_from_user(kbuf, buffer + bytes_written, bytes_to_copy)) {
            return -EFAULT;
        }
        for (i = 0; i < bytes_to_copy; i++) {
            while (ioread32(uart_base + UART_FR) & UART_FR_TXFF) {
                cpu_relax();
            }
            iowrite32(kbuf[i], uart_base + UART_DR);
        }
        bytes_written += bytes_to_copy;
    }
    return bytes_written; 
}

static const struct file_operations uart_fops = {
    .owner   = THIS_MODULE,
    .open    = rpi_uart_open,
    .release = rpi_uart_release,
    .read    = rpi_uart_read,
    .write   = rpi_uart_write,
};

static int __init rpi_uart_init(void)
{
    int result;
    u32 imsc_reg;

    if (uart_irq < 0) {
        pr_err("RPi UART Driver: Please provide a valid uart_irq module parameter\n");
        return -EINVAL;
    }

    uart_base = ioremap(UART0_PHYS_BASE, UART_REG_SIZE);
    if (!uart_base) return -ENOMEM;

    result = request_irq(uart_irq, rpi_uart_isr, IRQF_SHARED, DEVICE_NAME, &uart_cdev);
    if (result < 0) goto unmap_io;

    imsc_reg = ioread32(uart_base + UART_IMSC);
    imsc_reg |= UART_INT_RX;
    iowrite32(imsc_reg, uart_base + UART_IMSC);

    result = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (result < 0) goto mask_interrupts;

    cdev_init(&uart_cdev, &uart_fops);
    uart_cdev.owner = THIS_MODULE;
    
    result = cdev_add(&uart_cdev, dev_num, 1);
    if (result < 0) goto unregister_chrdev;

    uart_class = class_create(CLASS_NAME);
    if (IS_ERR(uart_class)) {
        result = PTR_ERR(uart_class);
        goto delete_cdev;
    }

    uart_device = device_create(uart_class, NULL, dev_num, NULL, DEVICE_NAME);
    if (IS_ERR(uart_device)) {
        result = PTR_ERR(uart_device);
        goto destroy_class;
    }

    pr_info("RPi UART Driver: Day 6 Circular Buffering Pipeline Ready.\n");
    return 0;

destroy_class:
    class_destroy(uart_class);
delete_cdev:
    cdev_del(&uart_cdev);
unregister_chrdev:
    unregister_chrdev_region(dev_num, 1);
mask_interrupts:
    imsc_reg = ioread32(uart_base + UART_IMSC);
    imsc_reg &= ~UART_INT_RX;
    iowrite32(imsc_reg, uart_base + UART_IMSC);
    free_irq(uart_irq, &uart_cdev);
unmap_io:
    iounmap(uart_base);
    return result;
}

static void __exit rpi_uart_exit(void)
{
    u32 imsc_reg;

    if (uart_base) {
        imsc_reg = ioread32(uart_base + UART_IMSC);
        imsc_reg &= ~UART_INT_RX;
        iowrite32(imsc_reg, uart_base + UART_IMSC);
    }

    free_irq(uart_irq, &uart_cdev);
    device_destroy(uart_class, dev_num);
    class_destroy(uart_class);
    cdev_del(&uart_cdev);
    unregister_chrdev_region(dev_num, 1);
    if (uart_base) iounmap(uart_base);
    pr_info("RPi UART Driver: Day 6 module unloaded safely.\n");
}

module_init(rpi_uart_init);
module_exit(rpi_uart_exit);