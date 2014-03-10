#include <stdlib.h>
#include <stdio.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include "lcd.h"
#include "uart.h"
#define F_CPU 16000000UL

//DS18B20 stuff
#define LOOP_CYCLES 8
#define us(num) ((5+num)/(LOOP_CYCLES*(1/(F_CPU/1000000.0))))

#define THERM_PORT PORTD
#define THERM_DDR DDRD
#define THERM_PIN PIND
#define THERM_DQ PD7

#define THERM_CMD_CONVERTTEMP 0x44
#define THERM_CMD_RSCRATCHPAD 0xbe
#define THERM_CMD_WSCRATCHPAD 0x4e
#define THERM_CMD_CPYSCRATCHPAD 0x48
#define THERM_CMD_RECEEPROM 0xb8
#define THERM_CMD_RPWRSUPPLY 0xb4
#define THERM_CMD_SEARCHROM 0xf0
#define THERM_CMD_READROM 0x33
#define THERM_CMD_MATCHROM 0x55
#define THERM_CMD_SKIPROM 0xCC
#define THERM_CMD_ALARMSEARCH 0xec

#define THERM_INPUT_MODE() THERM_DDR&=~(1<<THERM_DQ)
#define THERM_OUTPUT_MODE()	THERM_DDR|=(1<<THERM_DQ)
#define THERM_LOW()	THERM_PORT&=~(1<<THERM_DQ)
#define THERM_HIGH()	THERM_PORT|=(1<<THERM_DQ)

#define THERM_DECIMAL_STEPS_12BIT 625 //.0625

inline __attribute__ ((gnu_inline)) void therm_delay(uint16_t delay){while(delay--) asm volatile("nop");}
volatile uint16_t therm_step = 0;
volatile uint8_t therm_flag = 0;

uint8_t therm_reset(void){
	uint8_t i;
	//Pull line low and wait for 480us
	THERM_LOW();
	THERM_OUTPUT_MODE();
	therm_delay(us(480));
	
	//Release line and wait for 60us
	THERM_INPUT_MODE();
	therm_delay(us(60));
	
	//Store line value and wait until 480us over
	i=(THERM_PIN & (1<<THERM_DQ));
	therm_delay(us(420));
	
	//Return. 1 is error, 0 is okay
	return i;
}

void therm_write_bit(uint8_t bit){
	//Pull line low for 1uS
	THERM_LOW();
	THERM_OUTPUT_MODE();
	therm_delay(us(1));
	//If we want to write 1, release the line (if not will keep low)
	if(bit) THERM_INPUT_MODE();
	//Wait for 60uS and release the line
	therm_delay(us(60));
	THERM_INPUT_MODE();
}

uint8_t therm_read_bit(void){
	uint8_t bit=0;
	//Pull line low for 1uS
	THERM_LOW();
	THERM_OUTPUT_MODE();
	therm_delay(us(1));
	//Release line and wait for 14uS
	THERM_INPUT_MODE();
	therm_delay(us(14));
	//Read line value
	if(THERM_PIN&(1<<THERM_DQ)) bit=1;
	//Wait for 45uS to end and return read value
	therm_delay(us(45));
	return bit;
}

void therm_write_byte(uint8_t byte){
	uint8_t i=8;
	while(i--){
	//Write actual bit and shift one position right to make
//	the next bit ready
	therm_write_bit(byte&1);
	byte>>=1;
	}
}

uint8_t therm_read_byte(void){
	uint8_t i=8, n=0;
	while(i--){
	//Shift one position right and store read value
	n>>=1;
	n|=(therm_read_bit()<<7);
	}
	return n;
}

void therm_start_temperature( void ){
//Call this function first, will start temperature conversion then set 700ms timer & release uC	
	//Reset, skip ROM and start temperature conversion
	therm_reset();
	therm_write_byte(THERM_CMD_SKIPROM);
	therm_write_byte(THERM_CMD_CONVERTTEMP);
	therm_step=0;
	therm_flag=1;
	//cell_delay=1;
	//Wait until conversion is complete
}
void therm_read_temperature(char *buffer){
	//while(!therm_read_bit());
//This will be called 700mS after start_temperature to read and return temp
// Buffer length must be at least 12bytes long! ["+XXX.XXXX C"]
	uint8_t temperature[2];
	int8_t digit;
	uint16_t decimal;

	//Reset, skip ROM and send command to read Scratchpad
	therm_reset();
	therm_write_byte(THERM_CMD_SKIPROM);
	therm_write_byte(THERM_CMD_RSCRATCHPAD);
	//Read Scratchpad (only 2 first bytes)
	temperature[0]=therm_read_byte();
	temperature[1]=therm_read_byte();
	therm_reset();
	//Store temperature integer digits and decimal digits
	digit=temperature[0]>>4;
	digit|=(temperature[1]&0x7)<<4;
	//Store decimal digits
	decimal=temperature[0]&0xf;
	decimal*=THERM_DECIMAL_STEPS_12BIT;
	//Format temperature into a string [+XXX.XXXX C]
	//sprintf(buffer, "%+d.%04u C", digit, decimal);
	//Format temperature into string [xxC] with no decimals
	sprintf(buffer, "%d%", digit, decimal);

}	

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
volatile float temperature = 55;
uint8_t error = 0;

//----Cellular varibales---//
void fcell_send(void);
uint8_t cell_delay=0;//Delay counter to prevent flooding the cell phone with serial commands. Poor guy just can't keep up.
uint8_t cell_step=1; //Step counter to track which command is next in the sequence
uint8_t cell_send=0; //State variable, setting to 1 will trigger sending a text message
uint8_t cell_recieve=0;//State variable, setting to 1 will trigger checking the phone for messages.
char response_text[16];
void fcell_recieve(void);
uint8_t i = 0;

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
		
		TCCR0B |= (1 << CS02) | (1<<CS00); // set 1/1024 prescale and start timer0
		TCCR2B |= (1 << CS22) | (1<<CS21); //Start timer2 with 1/256 prescaler (overflow interrupt every 4.09mS)
		
		uint8_t x = 0;
		sei();         //enable Global interrupts
		OCR1A = 0x10; //Number of edges (half rotations) to watch for
		TIMSK0 |= (1<<OCIE0A); // Interrupt at OCR0A match
		TIMSK1 |= (1<<OCIE1A); //interrupt at OCR1A match
		TIMSK2 |= (1<<TOIE2); //Timer2 overflow interrupt
		
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
			
			if(x2==50){
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
			
			if(therm_flag==0){therm_start_temperature();}
			if(therm_flag==1 && therm_step>=171){
				therm_flag=0;
				therm_step=0;
				therm_read_temperature(buffer);
				lcd_gotoxy(6,0);		
				lcd_puts(buffer);
				lcd_puts("C");			
				fcell_recieve();

			}
			
			
			
		
			//temperature = ds1820_read_temp(DS1820_1_pin);
			//dtostrf(temperature,3,1,buffer);
			/*error = ds1820_reset(DS1820_1_pin);
			if(error==0){lcd_puts("RST!");}
			if(error==1){lcd_puts("111");}
			if(error==2){lcd_puts("222");}*/
			//fcell_send();
			
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
	
ISR (PCINT2_vect){//Pin change interrupt fires when anemometer sensor changes states
anem_counts++;//Increment number of times interrupt has fired
}


	
ISR(TIMER1_COMPA_vect){

    if(TIFR1 & (1<<TOV1)){
        TCCR0B &= ~(1<<CS02)|~(1<<CS01)|~(1<<CS00); //Stop timer0
        TCCR1B &= ~(1<<CS12)|~(1<<CS10)|~(1<<CS11); //Stop timer1
        
        calc_windspeed();
        TIFR1=(1<<TOV1);
		}
    }
	
ISR (TIMER0_COMPA_vect){  // timer0 overflow interrupt
    calc_windspeed();
}

ISR (TIMER2_OVF_vect){//System tick, fires every 
	
	cell_delay++;
	if(therm_flag==1){
	//lcd_puts("V");
	therm_step++;
	
	}
	

}
void adc_setup(){

	ADCSRA |= (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); // Set ADC prescalar to 128 - 125KHz sample rate @ 16MHz
	ADMUX |= (1 << REFS0); // Set ADC reference to AVCC
	ADCSRA |= (1 << ADEN);  // Enable ADC
	ADCSRA |= (1 << ADSC);  // Start first A2D Conversion
	ADMUX |= (1<<MUX2) | (1<<MUX1);
	ADMUX |= (1<<ADLAR); 
}

void fcell_send(){

			//cell_delay ticks with timer2 OVF at ~4.09ms. cell_delay>=25 gives ~0.109ms delay.
			if(cell_delay>=25 && cell_step==1){uart_puts("AT\r");cell_delay=0;cell_step=2;fcell_recieve();}
			if(cell_delay>=25 && cell_step==2){uart_puts("AT+cmgf=1\r");cell_delay=0;cell_step=3;}
			if(cell_delay>=25 && cell_step==3){uart_puts("AT+cmgs=\"8143237541\"\r");cell_delay=0;cell_step=4;}
			if(cell_delay>=25 && cell_step==4){uart_puts("message generated and sent by automatically. Do not reply.\x1A\r");cell_delay=0;cell_step=1;cell_send=1;}
			}
			
void fcell_recieve(){

	//UCSR0B = (1<<RXCIE0);//Enable recieve interrupts
	if(cell_delay>=25){uart_puts("AT\r");cell_delay=0;}
	
	i=0;
	//unsigned int letter = 0x0000;
	uint8_t letter = '\n';
	
	while(i<=3){
	letter = (uart_getc() & 0xFF); 
	//lcd_gotoxy((i+10),0);
	if( (letter != '\n') && (letter != '\r')){	//try and sanitize cell phone callback. send cell phone "AT\r" get back "AT OK \r\n 
	//lcd_putc(letter & 0xFF);
	response_text[i] = letter&0xFF;
	i++;}\\if
	}\\while
	
	//letter = uart_getc() & 0xFF;
	lcd_gotoxy(10,0);
	lcd_puts(response_text);
	//i=0;
	
}

