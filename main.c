#include <avr/io.h>
#include <avr/interrupt.h>

volatile unsigned char TimerFlag = 0; // TimerISR() sets this to 1. C programmer should clear to 0.

// Internal variables for mapping AVR's ISR to our cleaner TimerISR model.
unsigned long _avr_timer_M = 1; // Start count from here, down to 0. Default 1 ms.
unsigned long _avr_timer_cntcurr = 0; // Current internal count of 1ms ticks

void TimerOn() {
	// AVR timer/counter controller register TCCR1
	TCCR1B = 0x0B;// bit3 = 0: CTC mode (clear timer on compare)
	// bit2bit1bit0=011: pre-scaler /64
	// 00001011: 0x0B
	// SO, 8 MHz clock or 8,000,000 /64 = 125,000 ticks/s
	// Thus, TCNT1 register will count at 125,000 ticks/s

	// AVR output compare register OCR1A.
	OCR1A = 125;	// Timer interrupt will be generated when TCNT1==OCR1A
	// We want a 1 ms tick. 0.001 s * 125,000 ticks/s = 125
	// So when TCNT1 register equals 125,
	// 1 ms has passed. Thus, we compare to 125.
	// AVR timer interrupt mask register
	TIMSK1 = 0x02; // bit1: OCIE1A -- enables compare match interrupt

	//Initialize avr counter
	TCNT1=0;

	_avr_timer_cntcurr = _avr_timer_M;
	// TimerISR will be called every _avr_timer_cntcurr milliseconds

	//Enable global interrupts
	SREG |= 0x80; // 0x80: 1000000
}

void TimerOff() {
	TCCR1B = 0x00; // bit3bit1bit0=000: timer off
}

void TimerISR() {
	TimerFlag = 1;
}

// In our approach, the C programmer does not touch this ISR, but rather TimerISR()
ISR(TIMER1_COMPA_vect) {
	// CPU automatically calls when TCNT1 == OCR1 (every 1 ms per TimerOn settings)
	_avr_timer_cntcurr--; // Count down to 0 rather than up to TOP
	if (_avr_timer_cntcurr == 0) { // results in a more efficient compare
		TimerISR(); // Call the ISR that the user uses
		_avr_timer_cntcurr = _avr_timer_M;
	}
}

// Set TimerISR() to tick every M ms
void TimerSet(unsigned long M) {
	_avr_timer_M = M;
	_avr_timer_cntcurr = _avr_timer_M;
}

void ADC_init() {
	ADCSRA |= (1 << ADEN) | (1 << ADSC) | (1 << ADATE);
	// ADEN: setting this bit enables analog-to-digital conversion.
	// ADSC: setting this bit starts the first conversion.
	// ADATE: setting this bit enables auto-triggering. Since we are
	//        in Free Running Mode, a new conversion will trigger whenever
	//        the previous conversion completes.
}

	static unsigned char column_val = 0x01; // sets the pattern displayed on columns
	static unsigned char column_sel = 0x7F; // grounds column to display pattern
	unsigned short output = 0x00;
	
void transmit_data(unsigned short data);
// ====================
// SM1: DEMO LED matrix
// ====================
enum SM1_States {sm1_display} state;
int SM1_Tick() {
	// === Local Variables ===

	// === Transitions ===
	switch (state) {
		case sm1_display: break;
		default: state = sm1_display;
		break;
	}

	// === Actions ===
	switch (state) {
		case sm1_display: // If illuminated LED in bottom right corner
		if (column_sel == 0xFE && column_val == 0x80) {
			column_sel = 0x7F; // display far left column
			column_val = 0x01; // pattern illuminates top row
		}
		// else if far right column was last to display (grounded)
		else if (column_sel == 0xFE) {
			column_sel = 0x7F; // resets display column to far left column
			column_val = column_val << 1; // shift down illuminated LED one row
		}
		// else Shift displayed column one to the right
		else {
			column_sel = (column_sel >> 1) | 0x80;
		}
		break;
		default: break;
	}
	
	output = (((short)column_val) << 8 | column_sel);
	
	transmit_data(output);
	return state;
};

void transmit_data(unsigned short data) {
	int i;
	for (i = 0; i < 16 ; ++i) {
		// Sets SRCLR to 1 allowing data to be set
		// Also clears SRCLK in preparation of sending data
		PORTD = 0x08;
		// set SER = next bit of data to be sent.
		PORTD |= ((data >> i) & 0x01);
		// set SRCLK = 1. Rising edge shifts next bit of data into the shift register
		PORTD |= 0x02;
	}
	// set RCLK = 1. Rising edge copies data from “Shift” register to “Storage” register
	PORTD |= 0x04;
	// clears all lines in preparation of a new transmission
	PORTD = 0x00;
}

/*unsigned char ctrlReset;
unsigned char ctrlStrobe;
unsigned char i = 0;

unsigned short channel[7] = {0};

enum read_states{Init, read, store, high, low} read_state;
	
void Read_Tick()
{
	switch(read_state)
	{
		case Init:
			read_state = high;
			break;
			
		case high:
			read_state = low;
			break;
		
		case low:
			read_state = read;
			break;
			
		case read:
			read_state = store;
			 break;
			 
		case store:
		if(i < 7)
		{
			read_state = read;
		}
		else
		{
			read_state = Init;
		}
	}
	switch(read_state)	
	{
		case Init:
			ctrlReset = 0x00;
			ctrlStrobe = 0x02;
			i = 0;
			break;
			
		case low:
			ctrlReset = 0x01;
			break;
			
		case high:
			ctrlReset = 0x00;
			break;
		
		case read:
			ctrlStrobe = 0x02;
			break;
		
		case store:
			ctrlStrobe = 0x00;
			channel[i] = ADC;
			++i;
			break;
	}
	PORTB = channel[4]; //FIXME
	PORTC = ctrlStrobe | ctrlReset;
}*/
int main(void)
{	
	DDRB = 0xFF; // Set port B to output
	PORTB = 0x00; // Init port B to 0s
	DDRD = 0xFF;
	PORTD = 0x00;
	DDRC = 0xFF; PORTC = 0x00;
	
	ADC_init();//initializes ADC
	
	TimerSet(50);
	TimerOn();
	
	state = sm1_display;
	//read_state = Init; 
	
	while(1) {
		//Read_Tick();
		SM1_Tick();
	
		while(!TimerFlag);
		TimerFlag = 0;
	}
}