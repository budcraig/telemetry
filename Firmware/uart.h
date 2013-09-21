#ifndef UART_H
#define UART_H

#include <stdio.h>
#include <avr/io.h>
#include <util/delay.h>

#define FOSC 16000000
#define BAUD 4800
#define MYUBRR FOSC/16/BAUD-1

#define sbi(var, mask)   ((var) |= (uint8_t)(1 << mask))
#define cbi(var, mask)   ((var) &= (uint8_t)~(1 << mask))

#define STATUS_LED 0




void uart_ioinit(void);      // initializes IO

static int uart_putchar(char c, FILE *stream);

uint8_t uart_getchar(void);

static FILE mystdout = FDEV_SETUP_STREAM(uart_putchar, NULL, _FDEV_SETUP_WRITE);

void delay_ms(uint16_t x); // general purpose delay


#endif //UART_H