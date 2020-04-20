/*
 * ATTiny85 Clock with calendar, alarm and the ability to change the time, 
 * date and set alarm using 2 push buttons SET and MOD.
 *
 * Created: 16-04-2020 11:04:08
 * Author : Stephen Kingston
 */

#include <avr/io.h>
#include <avr/interrupt.h>

#define F_CPU 8000000UL
#include <util/delay.h>

//Control variables
volatile uint8_t counter = 0;
volatile uint8_t counter1 = 0;
volatile uint8_t mode = 1;
volatile uint8_t blinkingPosition = 2;
volatile uint8_t blinkingMode = 0;
volatile uint8_t PCIntButtonDown = 0;
volatile uint8_t alarmFlag = 0;

//Set initial time
volatile uint8_t hrs = 10;
volatile uint8_t min = 14;
volatile uint8_t secs = 40;
volatile uint8_t screen = 2;
volatile uint8_t day = 19;
volatile uint8_t month = 4;
volatile uint8_t year = 20;
volatile uint8_t alHr = 10;
volatile uint8_t alMin = 15;
volatile uint8_t alEn = 1;

typedef struct
{
	uint8_t position;
	uint8_t number;
} DisplayNumber;

//Function prototypes
void setPosition(uint8_t cursorPosition);
void setDigit(uint8_t digit, uint8_t dot);
void sendToShiftRegister(DisplayNumber* Digit);
void shiftDigit();
void printDigit(uint8_t digit, uint8_t position, uint8_t dot);
void printDisplay(uint8_t hours, uint8_t minutes, uint8_t seconds);
void printAlarmDisplay(uint8_t var1, uint8_t var2, uint8_t var3);
void timer0Init();
void updateTime();
void modeInterruptInit();
void setInterruptInit();
void checkModeHolddown();
void incrementBlinkingItem();
void checkAlarm();

#define LATCH PB0
#define CLK PB3
#define OE PB5
#define MOD PB2
#define SET PB4
#define SET_BIT(x,y)(x |=(1<<y))
#define CLR_BIT(x,y)(x &=~(1<<y))
#define TIMER_START 133

DisplayNumber Display;

//To check is alarm is to be activated
void checkAlarm()
{
	if(alHr == hrs && alMin == min && alEn == 1)
	alarmFlag = 1;
	else
	alarmFlag = 0;
}

//To set position of digit to be written, accepts inputs 0 to 5
void setPosition(uint8_t cursorPosition)
{
	if (cursorPosition <= 5 && cursorPosition >= 0)
		Display.position = (1 << cursorPosition) | (alarmFlag << 7);
}

//To set the digit at given cursor position
void setDigit(uint8_t digit, uint8_t dot)
{
	uint8_t numbers[] = {0xFC,0x60,0xDA,0xF2,0x66,0xB6,0xBE,0xE0,0xFE,0xF6,0x00,0xEE,0x9E,0x7A};
	Display.number = numbers[digit] | dot;

}

//The digit and its position is moved to the shift register
void sendToShiftRegister(DisplayNumber* Digit)
{
	for(int i = 0; i < 2; i++)
	{
		for(int j = 0; j <= 7; j++)
		{
			if (i == 0)
			{
				if((Digit->position >> j) & 1)
					CLR_BIT(PORTB, PB1);
				else
					SET_BIT(PORTB,PB1);
			}
			if (i == 1)
			{
				if((Digit->number >> j) & 1)
					SET_BIT(PORTB,PB1);
				else
					CLR_BIT(PORTB, PB1);
			}
			_delay_us(1);
			shiftDigit();
		}
	}
	CLR_BIT(PORTB,LATCH);
	_delay_us(1);
	SET_BIT(PORTB,LATCH);
	_delay_us(1);
}

//One clock cycle to move one bit on the shift register
void shiftDigit()
{
	CLR_BIT(PORTB,CLK);
	_delay_us(1);
	SET_BIT(PORTB,CLK);
	_delay_us(1);
}

//To print one digit onto the 7 segment display
void printDigit(uint8_t digit, uint8_t position, uint8_t dot)
{
	SET_BIT(PORTB,OE);
	_delay_us(1);
	setPosition(position);
	setDigit(digit, dot);
	sendToShiftRegister(&Display);
	CLR_BIT(PORTB,OE);
	_delay_ms(1);
}

//To print two digits at a time, for example: minutes, hours or days
void printTwoDigits(uint8_t number, uint8_t position, uint8_t blinkingDigit)
{
	uint8_t dot = 1;

	uint8_t minorPosition = 2*position;
	uint8_t majorPosition = minorPosition + 1;

	if (!((secs%2 == 1 && number != secs) || screen == 1 || screen == 2))
		dot = 0;

	if (blinkingDigit != 1)
	{
		if(number >= 10)
		{
			printDigit((number/10), majorPosition, 0);
			printDigit(number%10, minorPosition, dot);
		}
		else
		{
			printDigit(0, majorPosition, 0);
			printDigit(number%10, minorPosition, dot);
		}
	}
	else
	{
		printDigit(10, majorPosition, 0);
		printDigit(10, minorPosition, dot);
	}
}

//To print all 6 digits with the given variables
void printDisplay(uint8_t var1, uint8_t var2, uint8_t var3)
{
	uint8_t var[] = {var3, var2, var1};

	for (int z = 0; z < 3; z++)
	{
		if (blinkingMode == 0)
		{
			printTwoDigits(var[z], z, 0);
		}
		else
		{
			if(z == blinkingPosition && secs%2 == 0)
				printTwoDigits(0x00, z, 1);
			else
				printTwoDigits(var[z], z, 0);
		}
	}
}

void printAlarmDisplay(uint8_t var1, uint8_t var2, uint8_t var3)
{
	uint8_t var[] = {var3, var2, var1};

	for (int z = 0; z < 3; z++)
	{
		if (blinkingMode == 0)
		{
			if (z != 0)
			{
				printTwoDigits(var[z], z, 0);
			}
			if (z == 0)
			{
				printDigit(11, 1, 0);
				if (var3 == 1) //Alarm Enabled
				{
					printDigit(12, 0, 0);
				}
				if (var3 == 0) //Alarm Disabled
				{
					printDigit(13, 0, 0);
				}
			}
		}
		else
		{
			if (z == blinkingPosition && secs%2 == 0)
				printTwoDigits(0x00, z, 1);
			else if (z == 0)
			{
				printDigit(11, 1, 0);
				if (var3 == 1) //Alarm Enabled
				{
					printDigit(12, 0, 0);
				}
				if (var3 == 0) //Alarm Disabled
				{
					printDigit(13, 0, 0);
				}
			}
			else
			{
				printTwoDigits(var[z], z, 0);
			}
		}
	}
}
//Initializing Timer0
void timer0Init()
{
	TCNT0 = TIMER_START; //Initializing Timer0's initial value , actual 130
	TCCR0B |= (1 << CS02) | (1<<CS00) | (0<<CS01); //1024 Timer prescaler
	TIFR |= (1 << TOV0); //Clearing pending interrupt flags
	TIMSK |= (1 << TOIE0); //Enabling Timer 1 interrupt
	sei();
}

void updateTime()
{
	if (secs == 60)
	{
		secs = 0;
		min++;
		if (min == 60)
		{
			min = 0;
			hrs++;
			if (hrs == 24)
			{
				hrs = 0;
				day++;
				if (day == 28 && month == 2 && year%4 != 0)
					month++;
				else if (day == 29 && month == 2 && year%4 == 0)
					month++;
				else if (day == 30 && (month == 4 || month == 6 || month == 9 || month == 11))
					month++;
				else if (day == 31)
				{
					month++;
					if (month == 13)
					{
						month = 1;
						year++;
						if (year == 100)
							year = 0;
					}
				}
			}
		}
	}
}

//Initializing Interrupts

void modeInterruptInit()
{
	GIMSK |= (1 << INT0);
	MCUCR |= (0 << ISC00) |(1 << ISC01); // Falling edge trigger
}

void setInterruptInit()
{
	GIMSK |= (1 << PCIE);
	PCMSK |= (1 << PCINT4);
};

//Timer0 interrupt routine
ISR(TIMER0_OVF_vect) //Seconds timer
{
	TCNT0 = TIMER_START; //Actual 130
	counter++;
	if (counter == 41) //62.5
	{
		counter = 0;
		secs +=1;
	}
};

ISR(INT0_vect) //Interrupt for mode button
{
	//Start Timer1 and run for 3 seconds
	TCNT1 = TIMER_START;
	TCCR1 |= (1 << CS10) | (1<<CS11) | (1<<CS13); //1024 Timer prescaler
	TIFR |= (1 << TOV1); //Clearing pending interrupt flags
	TIMSK |= (1 << TOIE1); //Enabling Timer 1 interrupt
	sei();
			
	if (blinkingMode == 0)
	{
		screen++;
		screen %= 3;
	}
	if (blinkingMode == 1)
	{
		blinkingPosition++;
		blinkingPosition %= 3;
	}
};

void checkModeHolddown()
{
	if(PINB & (1 << MOD))
	{
		if (blinkingMode == 1)
			blinkingMode = 0;
		else
			blinkingMode = 1;
	}
}

void incrementBlinkingItem()
{
	if (screen == 0)
	{
		if(blinkingPosition == 2)
		{
			hrs++;
			if (hrs == 24)
				hrs = 0;
		}
		if(blinkingPosition == 1)
		{
			min++;
			if (min == 60)
				min = 0;
		}
		if(blinkingPosition == 0)
		{
			secs++;
			if (secs == 60)
				secs = 0;
		}
	}
	if (screen == 1)
	{
		if(blinkingPosition == 2)
		{
			day++;
			if (day == 32)
				day = 0;
		}
		if(blinkingPosition == 1)
		{
			month++;
			if (month == 13)
				month = 1;
		}
		if(blinkingPosition == 0)
		{
			year++;
			if (year == 100)
				year = 0;
		}
	}
	if (screen == 2)
	{
		if(blinkingPosition == 2)
		{
			alHr++;
			if (alHr == 24)
			alHr = 0;
		}
		if(blinkingPosition == 1)
		{
			alMin++;
			if (alMin == 60)
			alMin = 0;
		}
		if(blinkingPosition == 0)
		{
			if (alEn == 1)
				alEn = 0;
			else alEn = 1;
		}
	}
}

ISR(TIMER1_OVF_vect) //Interrupt for Timer1 overflow
{
	TCNT1 = TIMER_START;
	counter1++;
	if(counter1 == 125) // ~ 3 sec
	{
		counter1 = 0;
		checkModeHolddown();
		TIMSK &= ~(1 << TOIE1); //Disabling timer1 interrupt
	}
};

ISR(PCINT0_vect) //Pin change interrupt for pin 4 (SET)
{
	if (PCIntButtonDown == 0)
	{
		if (blinkingMode == 1)
		{
			incrementBlinkingItem();
			PCIntButtonDown = 1;
		}
	}
	else
	{
		PCIntButtonDown = 0;
	}
};

int main()
{
	DDRB = (1 << DDB0) | (1 << DDB1) | (0 << DDB2) | (1 << DDB3) | (1 << DDB5) | (0 << DDB4);
	timer0Init();
	modeInterruptInit();
	setInterruptInit();
	while(1)
	{
		updateTime();
		if (screen == 0)
		{
			printDisplay(hrs, min, secs);
		}
		if (screen == 1)
		{
			printDisplay(day, month, year);
		}
		if (screen == 2)
		{
			printAlarmDisplay(alHr, alMin, alEn);
		}
		checkAlarm();
	}
}

