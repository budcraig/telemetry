#include <stdlib.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include "lcd.h"
#include "uart.h"
#define F_CPU 16000000UL

/* 9600 baud */
#define UART_BAUD_RATE      9600  

volatile int time_click = 0;
int secs =0;
char buffer[17]  = "its alive!";


int main(void)
{
		/*
		TCCR0A |= (1 << WGM01); //Timer0 to CTC mode
		OCR0A = 250; //Vale Timer0 counts to
		TIMSK0 |= (1 << OCIE0A);    //Enable COMPA interrupt
		sei();         //enable Global interrupts
		TCCR0B |= (1 << CS02) | (1<<CS00); // set 1/1024 prescale and start the timer
		
		uint8_t x = 0;
		uint8_t line = 0;
		uint8_t time_click2=0;
		
		lcd_init(LCD_DISP_ON); //Initialize LCD
		*/
		uart_init( UART_BAUD_SELECT(UART_BAUD_RATE,F_CPU) );  //Setup UART IO pins and defaults
		
        //lcd_clrscr(); // clear display and home cursor  
        //lcd_puts("Line1 \n"); //Write to LCD
		
		sei();
		
		while (1) //Endless loop
		{
		
			for(int i=0; i<12;i++){
			uart_puts(buffer);
			_delay_ms(500);
			}
		
			/*
			if(time_click>=250){ //Watch for 125 interrupts
			time_click2++;
			time_click=0;
			line++;
			
			if(line==2){printf("AT\r");}	//Check that phone is alive. Response should be 'OK'
			if(line==3){printf("AT+cmgf=1\r");} //Tell phone to expect SMS message. Response 'OK'
			if(line==4){printf("AT+cmgs=\"8143237541\"\r");} //Set destination address
			if(line==5){
				printf("message generated and sent by avr. reply to iphone.\x1A\r"); //Set message. CTRL-Z to finish input
				}
				
			if(line>7){printf("AT\r");} //Repeat 'phone is alive' commands

			}
			*/
			/*
			secs++;
			lcd_gotoxy(0,1); 	
			lcd_puts(itoa(secs, seconds, 10));
			time_click=0;
			x++;
			//printf("Test it! x = %d", x);	
			printf("AT\n");
			*/
			
			
			
		

		
		}
		
		
		
}
		
ISR (TIMER0_COMPA_vect)  // timer0 overflow interrupt
{

  		time_click++;

}

