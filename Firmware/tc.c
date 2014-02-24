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
int anem_counts = 0;

//-----Windspeed variables ----//
void start_windspeed( void); 		//Function to request an anemometer reading
volatile float freq = 0; 	    //
volatile char aws[6] = " kts";
volatile uint8_t flag = 0;      //Flag so computer doesn't try to initate a windspeed reading in the middle of another one
volatile float time = 65535;
volatile uint8_t edges = 10;     //Number of edges T0 will be looking for
volatile char revstring[16] = "255";	//char array to put #of revolutions as ascii
void calc_windspeed( void );
//-----------------------------//

void calc_windspeed(){
	TCCR0B &= ~(1<<CS02)|~(1<<CS01)|~(1<<CS00); //Stop timer0
	TCCR1B &= ~(1<<CS12)|~(1<<CS10)|~(1<<CS11); //Stop timer1

	time = TCNT1; //Number of counts that timer1 passed during measurement period
	   
	//Add 16 bits if timer1 ran over, and lower number of req'd counts
	if(TIFR1 & (1<<TOV1)){
    time=time+65535.0;
    if((time>15625) && (edges>2)){edges--;}
	TIFR1 = (1<<TOV1);
    }
    
    if(time<46875 && edges<150){edges++;}
	
	time = time/15625.0; //time into seconds
  
    freq = TCNT0/time;

    //Factory stated transfer function is 0.765 m/s/Hz
	freq = freq*1.48;	//Convert from Hz to kts
	
    dtostrf(freq,3,1,revstring);
    
    lcd_gotoxy(5,1);
    lcd_puts(revstring);
    lcd_puts(aws);

    
	TCNT0=0;
	flag=0; //Unlock windspeed request
    
    }

int main(void)
{
		sei();         //enable Global interrupts
		TCCR0B |= (1 << CS02) | (1<<CS00); // set 1/1024 prescale and start the timer
		
		uint8_t x = 0;
		
		OCR1A = 0x10; //Number of edges (half rotations) to watch for
		TIMSK0 |= (1<<OCIE0A); // Interrupt at OCR0A match
		TIMSK1 |= (1<<OCIE1A); //interrupt at OCR1A match
		
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
			
			
			if(flag==0){start_windspeed();}
			}
	
		}
		
		
		
}

void start_windspeed(){ //read anemometer
	
    OCR0A = edges;
	lcd_gotoxy(0,1);
	lcd_puts("AWS:");
	
	/*
	printf("windspeed requested\n");
    printf("looking for ");
    printf(itoa(edges, buffer, 10));
    printf(" edges\n");
	*/
	
	TCNT0 = 0; //Clear timer0
	TCNT1 = 0; //Clear timer1

	TCCR0B |= (1<<CS02)|(1<<CS01)|(1<<CS00); //Start timer0 on external source, rising edge, T0 pin
	TCCR1B |= (1<<CS12)|(1<<CS10); //Start timer1 on systemclock/1024
	flag=1;//Lock windspeed request

	}
	
ISR (PCINT2_vect){
anem_counts++;
}


	
ISR(TIMER1_COMPA_vect){

    if(TIFR1 & (1<<TOV1)){
        TCCR0B &= ~(1<<CS02)|~(1<<CS01)|~(1<<CS00); //Stop timer0
        TCCR1B &= ~(1<<CS12)|~(1<<CS10)|~(1<<CS11); //Stop timer1
        
        calc_windspeed();
        
        TIFR1=(1<<TOV1);
		}
    }
	
ISR (TIMER0_COMPA_vect)  // timer0 overflow interrupt
{
    calc_windspeed();
}

void adc_setup(){

	ADCSRA |= (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); // Set ADC prescalar to 128 - 125KHz sample rate @ 16MHz
	ADMUX |= (1 << REFS0); // Set ADC reference to AVCC
	ADCSRA |= (1 << ADEN);  // Enable ADC
	ADCSRA |= (1 << ADSC);  // Start first A2D Conversion
	ADMUX |= (1<<MUX2) | (1<<MUX1);
	ADMUX |= (1<<ADLAR); 
}

