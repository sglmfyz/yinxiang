#ifndef __GPIO_H__
#define __GPIO_H__


#define GPIO_Write_Low(port, pin) gpio_bits_reset(port, pin)
#define GPIO_Write_Hi(port, pin) gpio_bits_set(port, pin)
#define GPIO_Toggle(port, pin) gpio_port_write(port, port->odt ^ pin)

#define GPIO_Read(port, pin) gpio_input_data_bit_read(port, pin)


#endif
