#include <linux/module.h>
#include <linux/io.h>
#include <linux/types.h>
#include "gpio.h"

#define PERIPHERAL_BASE 0xFE000000UL
#define GPIO_OFFSET     0x00200000UL
#define GPIO_BASE_PHYS  (PERIPHERAL_BASE + GPIO_OFFSET)
#define GPIO_BLOCK_SIZE 4096
#define GPIO12          12

static void __iomem *gpio_base;

static inline u32 gpio_read(unsigned int off)
{
    return ioread32(gpio_base + off);
}

static inline void gpio_write(unsigned int off, u32 value)
{
    iowrite32(value, gpio_base + off);
}

int gpio12_init_output(void)
{
    u32 v;
    unsigned int reg = (GPIO12 / 10) * 4;
    unsigned int shift = (GPIO12 % 10) * 3;

    gpio_base = ioremap(GPIO_BASE_PHYS, GPIO_BLOCK_SIZE);
    if (!gpio_base)
        return -ENOMEM;

    v = gpio_read(reg);
    v &= ~(0x7u << shift);
    v |=  (0x1u << shift);   // output
    gpio_write(reg, v);

    gpio12_set(0);
    return 0;
}

void gpio12_set(int value)
{
    unsigned int reg = (GPIO12 / 32);
    unsigned int bit = (GPIO12 % 32);

    if (value)
        gpio_write(0x1C + reg * 4, (1u << bit)); // GPSET
    else
        gpio_write(0x28 + reg * 4, (1u << bit)); // GPCLR
}

void gpio12_cleanup(void)
{
    if (gpio_base) {
        gpio12_set(0);
        iounmap(gpio_base);
        gpio_base = NULL;
    }
}

MODULE_LICENSE("GPL");
