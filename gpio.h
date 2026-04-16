#ifndef GPIO_H
#define GPIO_H

int gpio12_init_output(void);
void gpio12_set(int value);
void gpio12_cleanup(void);

#endif