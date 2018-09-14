#include <avr/io.h>
#include <avr/sleep.h>  
#include <avr/pgmspace.h>
#include <avr/interrupt.h>

#include <util/delay.h>

#include "duplo-train.h"

/* Pin definitions */
#define MOTOR_PIN	PB0
#define SPEAKER_PIN	PB1
#define SENSOR_PIN	PB2
#define BUTTON1_PIN	PB3
#define BUTTON2_PIN	PB4

/* Motor delay values */
#define MOTOR_DELAY_START 300
#define MOTOR_DELAY_STOP  50 

/* Motor max, min and start values */
#define MOTOR_VALUE_MAX   0xFF
#define MOTOR_VALUE_MIN   0x00
#define MOTOR_VALUE_START 0x80

/* Train starting sound delay values */
#define SOUND_TRAIN_STARTING_DELAY_MAX 3200
#define SOUND_TRAIN_STARTING_DELAY_MIN 200
#define SOUND_TRAIN_STARTING_DELAY_INC 500
#define SOUND_TRAIN_STARTED_COUNT      ((SOUND_TRAIN_STARTING_DELAY_MAX - SOUND_TRAIN_STARTING_DELAY_MIN) / \
									   SOUND_TRAIN_STARTING_DELAY_INC)

/* Train stopping sound delay values */	
#define SOUND_TRAIN_STOPPING_DELAY_MAX 3200
#define SOUND_TRAIN_STOPPING_DELAY_MIN 200
#define SOUND_TRAIN_STOPPING_DELAY_INC 1000
#define SOUND_TRAIN_STOPPED_COUNT      ((SOUND_TRAIN_STOPPING_DELAY_MAX - SOUND_TRAIN_STOPPING_DELAY_MIN) / \
									   SOUND_TRAIN_STOPPING_DELAY_INC)

/* Train ringing 1 start and stop count */
#define SOUND_TRAIN_RINGING_1_START_COUNT (SOUND_TRAIN_STARTED_COUNT + 15)
#define SOUND_TRAIN_RINGING_1_STOP_COUNT  (SOUND_TRAIN_STARTED_COUNT + 20)

/* Train whistling start and stop count */
#define SOUND_TRAIN_WHISTLING_START_COUNT (SOUND_TRAIN_STARTED_COUNT + 20)
#define SOUND_TRAIN_WHISTLING_STOP_COUNT  (SOUND_TRAIN_STARTED_COUNT + 25)

/* Train ringing 2 start and stop count */
#define SOUND_TRAIN_RINGING_2_START_COUNT (SOUND_TRAIN_STARTED_COUNT + 25)
#define SOUND_TRAIN_RINGING_2_STOP_COUNT  (SOUND_TRAIN_STARTED_COUNT + 30)

/* Sensor check delay (8000 * 125us = 1sec) */
#define SENSOR_CHECK_DELAY 8000

/* Event definitions */
typedef enum
{
	no_event,
	sensor_missing,
	button1_pressed,
	button1_released,
	button2_pressed,
	button2_released
} events;

/* Sound definitions */
typedef enum
{
	no_sound,
	train_starting,
	train_running,
	train_stopping,
	train_ringing,
	train_whistling,
} sounds;

/* Variables */
volatile int motor_delay = 0;
volatile int sound_count = 0;
volatile int sound_delay = 0;
volatile int sound_index = 0;
volatile int sound_length = 0;
volatile int sensor_count = 0;
volatile int sensor_delay = 0;

volatile unsigned char event = no_event;
volatile unsigned char sound = no_sound;
volatile unsigned char motor_value = 0;
volatile unsigned char sound_value = 0;
volatile unsigned char sound_byte = 0;
volatile unsigned char sound_byte_count = 0;

/* Pin change interrupt routine */
ISR(PCINT0_vect)
{
	if (bit_is_clear(PINB, BUTTON1_PIN))
	{
		_delay_ms(50);

		if (bit_is_clear(PINB, BUTTON1_PIN))
		{
			if (event != button1_pressed)
			{
				if (motor_value == MOTOR_VALUE_MIN)
				{
					/* Set internal pull-up on Sensor pin */
					PORTB |= (1 << SENSOR_PIN);

					/* Set pin change interrupt on Sensor pin */
					PCMSK |= (1 << SENSOR_PIN);

					motor_delay = 0;
					sound_count = 0;
					sound_delay = 0;
					sound_index = 0;
					sensor_delay = 0;
					event = button1_pressed;
				}
			}
			else
			{
				if (sound_count >= SOUND_TRAIN_STOPPED_COUNT)
				{
					/* Clear internal pull-up on Sensor pin to save power */
					PORTB &= ~(1 << SENSOR_PIN);

					/* Clear pin change interrupt on Sensor pin */
					PCMSK &= ~(1 << SENSOR_PIN);

					motor_delay = 0;
					sound_count = 0;
					sound_delay = 0;
					sound_index = 0;
					sensor_delay = 0;
					event = button1_released;
				}
			}
		}
	}
	else if (bit_is_clear(PINB, BUTTON2_PIN))
	{
		_delay_ms(50);

		if (bit_is_clear(PINB, BUTTON2_PIN))
		{
			sound_count = 0;
			event = button2_pressed;
		}
	}
	else if (event == button2_pressed)
	{
		event = button2_released;
	}

	if (bit_is_clear(PINB, SENSOR_PIN))
	{
		sensor_count++;
	}
}

/* Timer0 overflow interrupt routine (executed every 32us = 31.3kHz) */
ISR(TIMER0_OVF_vect)
{
	OCR0A = motor_value;
	OCR0B = sound_value;
}

/* Timer1 overflow interrupt routine (executed every 125us = 8kHz) */
ISR(TIMER1_OVF_vect)
{
	if (sensor_delay == SENSOR_CHECK_DELAY)
	{
		if ((event == button1_pressed) && (sensor_count == 0))
		{
			event = sensor_missing;
		}

		sensor_count = 0;
		sensor_delay = 0;
	}
	else
	{
		sensor_delay++;
	}

	switch (event)
	{
		case button1_pressed:
		{
			if (motor_value < MOTOR_VALUE_MAX)
			{
				if (motor_value == MOTOR_VALUE_MIN)
				{
					motor_value = MOTOR_VALUE_START;
				}

				motor_delay++;

				if (motor_delay == MOTOR_DELAY_START)
				{
					motor_value++;
					motor_delay = 0;
				}			
			}

			if ((sound_count >= SOUND_TRAIN_RINGING_1_START_COUNT) && 
				(sound_count < SOUND_TRAIN_RINGING_1_STOP_COUNT))
			{
				sound = train_ringing;
				sound_length = sizeof(sound_train_bell);
			}
			else if ((sound_count >= SOUND_TRAIN_WHISTLING_START_COUNT) &&
				(sound_count < SOUND_TRAIN_WHISTLING_STOP_COUNT))
			{
				sound = train_whistling;
				sound_length = sizeof(sound_train_whistle);
			}
			else if ((sound_count >= SOUND_TRAIN_RINGING_2_START_COUNT) && 
				(sound_count < SOUND_TRAIN_RINGING_2_STOP_COUNT))
			{
				sound = train_ringing;
				sound_length = sizeof(sound_train_bell);
			}
			else
			{			
				if (sound_count < SOUND_TRAIN_STARTED_COUNT)
				{
					sound = train_starting;
				}
				else
				{
					sound = train_running;
				}					

				if (sound_count == SOUND_TRAIN_RINGING_2_STOP_COUNT)
				{
					sound_count = SOUND_TRAIN_STARTED_COUNT;
				}

				sound_length = sizeof(sound_train_engine);
			}
			break;
		}

		case button1_released:
		{
			if (motor_value > MOTOR_VALUE_MIN)
			{
				motor_delay++;

				if (motor_delay == MOTOR_DELAY_STOP)
				{
					motor_value--;
					motor_delay = 0;
				}			
			}

			if (sound_count < SOUND_TRAIN_STOPPED_COUNT)
			{ 
				sound = train_stopping;
				sound_length = sizeof(sound_train_engine);
			}
			else
			{
				sound = no_sound;
				sound_length = 0;
			}
			break;
		}

		case button2_pressed:
		{
			motor_value = 0;
			sound = train_whistling;
			sound_length = sizeof(sound_train_whistle);
			break;
		}

		case sensor_missing:
		case button2_released:
		{
			motor_value = 0;
			sound = no_sound;
			sound_length = 0;
			break;
		}
	}

	if (sound_delay == 0)
	{
		if (sound_index < sound_length)
		{
			switch (sound)
			{
				case train_starting:
				case train_running:
				case train_stopping:
					sound_byte = pgm_read_byte(&sound_train_engine[sound_index]);
					break;

				case train_ringing:
					sound_byte = pgm_read_byte(&sound_train_bell[sound_index]);
					break;

				case train_whistling:
					sound_byte = pgm_read_byte(&sound_train_whistle[sound_index]);
					break;
			}

			if ((sound_index == 0) && 
				(sound_byte_count < sound_byte) && ((sound <= train_stopping) || 
				(sound_count == SOUND_TRAIN_RINGING_1_START_COUNT) || 
				(sound_count == SOUND_TRAIN_WHISTLING_START_COUNT) ||
				(sound_count == SOUND_TRAIN_RINGING_2_START_COUNT)))
			{
				sound_byte_count++;
				sound_value = sound_byte_count;

				if (sound_byte_count == sound_byte)
				{
					sound_byte_count = 0;
					sound_index++;
				}
			}
			else if ((sound_index == (sound_length - 1)) && 
				(sound_byte_count < sound_byte) && ((sound <= train_stopping) || 
				(sound_count == SOUND_TRAIN_RINGING_1_STOP_COUNT) ||
				(sound_count == SOUND_TRAIN_WHISTLING_STOP_COUNT) ||
				(sound_count == SOUND_TRAIN_RINGING_2_STOP_COUNT)))
			{
				sound_byte_count++;
				sound_value = (sound_byte - sound_byte_count);

				if (sound_byte_count == sound_byte)
				{
					sound_byte_count = 0;
					sound_index++;
				}
			}
			else
			{
				sound_value = sound_byte;
				sound_index++;
			}
		}
		else
		{
			switch (sound)
			{
				case no_sound:
					sound_value = 0;
					break;

				case train_starting:
					sound_delay = (SOUND_TRAIN_STARTING_DELAY_MAX - (sound_count * SOUND_TRAIN_STARTING_DELAY_INC));					 
					break;

				case train_running:
					sound_delay = SOUND_TRAIN_STOPPING_DELAY_MIN;
					break;

				case train_stopping:
					sound_delay = (SOUND_TRAIN_STOPPING_DELAY_MIN + (sound_count * SOUND_TRAIN_STOPPING_DELAY_INC));
					break;

				case train_ringing:
				case train_whistling:
					sound_delay = 0;
					break;
			}

			if (sound != no_sound)
			{
				sound_count++;
			}

			sound_index = 0;
		}
	}
	else
	{
		sound_delay--;
	}
}

int main(void)
{
	/* Set Clock Prescaler Change Enable bit */
	CLKPR = (1 << CLKPCE); 

	/* Set Clock Division Fator to 1 (internal 8MHz Clock) */
	CLKPR = 0;

	/* Set Motor and Speaker pin as output pins */
	DDRB = (1 << MOTOR_PIN) | (1 << SPEAKER_PIN);

	/* Set internal pull-up on Button1 and Button2 pins */ 
	PORTB = (1 << BUTTON1_PIN) | (1 << BUTTON2_PIN);

	/* Set Timer/Counter0 to fast PWM mode and clear OC0A/B on compare match */
	TCCR0A = (1 << COM0A1) | (1 << COM0B1) | (1 << WGM01) | (1 << WGM00);

	/* Set Timer/Counter0 Prescaler to 1 (PWM frequency of 32KHz) */
	TCCR0B = (1 << CS00);

	/* Set Timer/Counter1 Output Compare Register C to the sample rate freq. */
	OCR1C = (((F_CPU / 4) / SAMPLE_RATE) - 1);

	/* Set Timer/Counter1 Prescaler to System Clock/4 and start the timer */
	TCCR1 = (1 << CS11) | (1 << CS10);

	/* Set Timer/Counter0 and Timer/Counter1 Overflow Interrupt Enable bits */
	TIMSK = (1 << TOIE0) | (1 << TOIE1);

	/* Set pin change interrupt on Button1 and Button2 pins */
	PCMSK = (1 << BUTTON1_PIN) | (1 << BUTTON2_PIN);

	/* Set Pin Change Interrupt Enable bit */
	GIMSK = (1 << PCIE);

	/* Set Global Interrupt Enable bit */
	sei();

	/* Run forever */
	while (1)
	{
		/* Check if none of the buttons have been pressed */
		if ((event != button1_pressed) && (event != button2_pressed))
		{
			/* Wait until the train is completely stopped */
			_delay_ms(5000);

			/* Check if none of the buttons have been pressed */
			if ((event != button1_pressed) && (event != button2_pressed))
			{
				/* Enter sleep mode to reduce power consumption */
				set_sleep_mode(SLEEP_MODE_PWR_DOWN);
				sleep_mode();
			}
		}
		else
		{
			/* Wait a bit before checking again */
			_delay_ms(1000);
		}
	}

	return 0;
}
