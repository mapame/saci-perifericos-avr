#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include <util/delay.h>

#include <stdio.h>

#include "common.h"
#include "module.h"

#ifndef LED_ON_D3
#error "LED must be on pin D3"
#endif

#define PIN_INPUT D, 4
#define PIN_LEVEL_POWER C, 3

typedef struct {
	uint16_t reg_port;
	uint16_t reg_pin;
	uint8_t bitmask;
} level_def_t;

const char module_type[16] = "WATERLEVEL_V1";
const unsigned int module_channel_qty = 6;

int level_read(uint8_t port, char *result_buffer);

int level_pulse_dur_read(uint8_t port, char *result_buffer);
int level_pulse_dur_write(uint8_t port, const char *value_buffer);

int input_value_read(uint8_t port, char *result_buffer);

int input_value_hl_read(uint8_t port, char *result_buffer);
int input_value_hl_write(uint8_t port, const char *value_buffer);

int input_value_ll_read(uint8_t port, char *result_buffer);
int input_value_ll_write(uint8_t port, const char *value_buffer);

int input_freq_read(uint8_t port, char *result_buffer);

const channel_list_t channel_list[] = {
	{"LEVEL_VALUE",		7, 'B', 'R', "0", "1",		&level_read,			NULL},
	{"LEVEL_PULSE_DUR",	1, 'I', 'W', "0", "9",		&level_pulse_dur_read,	&level_pulse_dur_write},
	{"INPUT_VALUE",		1, 'B', 'R', "0", "1",		&input_value_read,		NULL},
	{"INPUT_VALUE_HL",	1, 'B', 'W', "0", "1",		&input_value_hl_read,	&input_value_hl_write},
	{"INPUT_VALUE_LL",	1, 'B', 'W', "0", "1",		&input_value_ll_read,	&input_value_ll_write},
	{"INPUT_FREQ",		1, 'I', 'R', "0", "65535",	&input_freq_read,		NULL},
};

const level_def_t level_inputs[7] = {
	{(uint16_t) &PORTB, (uint16_t) &PINB, _BV(3)},
	{(uint16_t) &PORTC, (uint16_t) &PINC, _BV(1)},
	{(uint16_t) &PORTC, (uint16_t) &PINC, _BV(2)},
	{(uint16_t) &PORTC, (uint16_t) &PINC, _BV(0)},
	{(uint16_t) &PORTB, (uint16_t) &PINB, _BV(5)},
	{(uint16_t) &PORTB, (uint16_t) &PINB, _BV(4)},
	{(uint16_t) &PORTB, (uint16_t) &PINB, _BV(2)},
};

volatile uint8_t level_dbnc[7] = {0};
volatile uint8_t level_value[7] = {0};

volatile uint8_t level_pulse_duration = 1;

volatile uint8_t input_dbnc = 0;
volatile uint8_t input_value = 0;
volatile uint8_t input_value_hl = 0;
volatile uint8_t input_value_ll = 1;

volatile uint16_t freq_counter = 0;
volatile uint16_t input_frequency = 0;

volatile uint8_t *reg;
volatile uint8_t *ireg;

void module_init() {
	SET_PIN(DDR, PIN_LEVEL_POWER);
	
	SET_PIN(PORT, PIN_INPUT);
	
	/* Enable pull-ups */
	for(uint8_t i = 0; i < 7; i++) {
		reg = (volatile uint8_t*) level_inputs[i].reg_port;
		*reg |= level_inputs[i].bitmask;
	}
	
	/* Set pin T0 as the clock source (rising edge) for Counter 0 */
	TCCR0B = _BV(CS00) | _BV(CS01) | _BV(CS02);
}

void module_tick(uint8_t counter) {
	if((counter % 10) == 0 && level_pulse_duration) {
		SET_PIN(PORT, PIN_LEVEL_POWER);
	} else if((counter % 10) == level_pulse_duration) {
		for(uint8_t port = 0; port < 7; port++) {
			ireg = (volatile uint8_t*) level_inputs[port].reg_pin;
			
			level_dbnc[port] <<= 1;
			level_dbnc[port] |= ((*ireg) & level_inputs[port].bitmask) ? 0 : 1;
			
			if((level_dbnc[port] & 0b00011111) == 0b00011111)
				level_value[port] = 1;
			else if((level_dbnc[port] & 0b00011111) == 0b00000000)
				level_value[port] = 0;
		}
		
		CLR_PIN(PORT, PIN_LEVEL_POWER);
	}
	
	
	input_dbnc <<= 1;
	input_dbnc |= GET_PIN(PIN, PIN_INPUT) ? 0 : 1;
	
	if((input_dbnc & 0b00011111) == 0b00011111) {
		input_value = 1;
		input_value_hl = 1;
	} else if((input_dbnc & 0b00011111) == 0b00000000) {
		input_value = 0;
		input_value_ll = 0;
	}
	
	
	freq_counter += TCNT0;
	TCNT0 = 0;
	
	if(counter == 0) {
		input_frequency = freq_counter;
		freq_counter = 0;
	}
	
	return;
}

int level_read(uint8_t port, char *result_buffer) {
	result_buffer[0] = level_value[port] ? '1' : '0';
	
	result_buffer[1] = '\0';
	
	return 1;
}

int level_pulse_dur_read(uint8_t port, char *result_buffer) {
	sprintf(result_buffer, "%u", level_pulse_duration);
	
	return 1;
}

int level_pulse_dur_write(uint8_t port, const char *value_buffer) {
	unsigned int dur_value;
	
	if(sscanf(value_buffer, "%u", &dur_value) != 1)
		return 0;
	
	if(dur_value < 1)
		level_pulse_duration = 1;
	else if(dur_value > 9)
		level_pulse_duration = 9;
	else
		level_pulse_duration = (uint8_t) dur_value;
	
	return 1;
}

int input_value_read(uint8_t port, char *result_buffer) {
	result_buffer[0] = input_value ? '1' : '0';
	
	result_buffer[1] = '\0';
	
	return 1;
}

int input_value_hl_read(uint8_t port, char *result_buffer) {
	result_buffer[0] = input_value_hl ? '1' : '0';
	
	result_buffer[1] = '\0';
	
	return 1;
}

int input_value_hl_write(uint8_t port, const char *value_buffer) {
	if(value_buffer[0] == '1')
		input_value_hl = 1;
	else if(value_buffer[0] == '0')
		input_value_hl = 0;
	else
		return 0;
	
	return 1;
}

int input_value_ll_read(uint8_t port, char *result_buffer) {
	result_buffer[0] = input_value_ll ? '1' : '0';
	
	result_buffer[1] = '\0';
	
	return 1;
}

int input_value_ll_write(uint8_t port, const char *value_buffer) {
	if(value_buffer[0] == '1')
		input_value_ll = 1;
	else if(value_buffer[0] == '0')
		input_value_ll = 0;
	else
		return 0;
	
	return 1;
}

int input_freq_read(uint8_t port, char *result_buffer) {
	ATOMIC_BLOCK(ATOMIC_FORCEON) {
		sprintf(result_buffer, "%u", input_frequency);
	}
	
	return 1;
}
