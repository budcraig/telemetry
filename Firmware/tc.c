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
int x2=0;
int secs =0;
char buffer[17]  = "its alive!";
volatile uint16_t adc_value = 0;
volatile uint8_t ADClow = 0;
float volts = 0;
void adc_setup(void);

/*WINDSPEED CALCULATIONS ARE IN ADC.C in ADCPWM FOLDER*/


int main(void)
{
		
		TCCR0A |= (1 << WGM01); //Timer0 to CTC mode
		OCR0A = 250; //Vale Timer0 counts to
		TIMSK0 |= (1 << OCIE0A);    //Enable COMPA interrupt
		sei();         //enable Global interrupts
		TCCR0B |= (1 << CS02) | (1<<CS00); // set 1/1024 prescale and start the timer
		
		uint8_t x = 0;
		uint8_t line = 0;
		uint8_t time_click2=0;
		
		lcd_init(LCD_DISP_ON); //Initialize LCD
		
		uart_init( UART_BAUD_SELECT(UART_BAUD_RATE,F_CPU) );  //Setup UART IO pins and defaults
		
        lcd_clrscr(); // clear display and home cursor  
        //lcd_puts("Starting"); //Write to LCD
		
		adc_setup(); //Set up ADC
		lcd_gotoxy(0,0); 
		sei();	//Enable global interrupts
		
		while (1) //Endless loop
		{
		
			if((ADCSRA & (1<<ADSC))!=1){ //ADC conversion completed
			x++;
			}
			
			if(x==255){x2++;x=0;}
			
			if(x2==255){
			ADClow = ADCL;
			adc_value = ADCH<<2 | ADClow >> 6;
			volts = adc_value * 0.0455;	//0.0455 calculated transfer function from adc to volts: 5 volts / 1024 bit precision * (11.2/1.2) voltage divider ratio 
			dtostrf(volts,3,1,buffer); //Converts volts float to string in buffer. 3 digits, 1 decimal minimum
			lcd_gotoxy(0,0);			//Output to screen
			if(adc_value<100){lcd_puts("0");}
			lcd_puts(buffer);
			lcd_gotoxy(4,0);
			lcd_puts("V");
			x2=0;
			ADCSRA |= (1 << ADSC);	//Go next ADC conversion
			}
		
		
			/*
			for(int i=0; i<12;i++){
			lcd_puts(buffer);
			//lcd_puts("g");
			_delay_ms(500);
			}
			*/
		
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

void adc_setup(){

	ADCSRA |= (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); // Set ADC prescalar to 128 - 125KHz sample rate @ 16MHz
	ADMUX |= (1 << REFS0); // Set ADC reference to AVCC
	ADCSRA |= (1 << ADEN);  // Enable ADC
	ADCSRA |= (1 << ADSC);  // Start first A2D Conversion
	ADMUX |= (1<<MUX2) | (1<<MUX1);
	ADMUX |= (1<<ADLAR); 
}

