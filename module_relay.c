#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include <util/delay.h>

#include <stdio.h>

#include "common.h"
#include "module.h"

#define PIN_PWM D, 3

#ifdef LED_ON_D3
#error "Multiple definitions on the same pin!"
#endif

typedef struct {
	uint16_t reg_mode;
	uint16_t reg_output;
	uint8_t bitmask;
} relay_output_def_t;

const char module_type[16] = "RELAY_V1";
const unsigned int module_channel_qty = 4;

int relay_read(uint8_t port, char *result_buffer);
int relay_write(uint8_t port, const char *value_buffer);

int relay_timeout_read(uint8_t port, char *result_buffer);
int relay_timeout_write(uint8_t port, const char *value_buffer);

int pwm_read(uint8_t port, char *result_buffer);
int pwm_write(uint8_t port, const char *value_buffer);

int pwm_freq_read(uint8_t port, char *result_buffer);
int pwm_freq_write(uint8_t port, const char *value_buffer);

const channel_list_t channel_list[] = {
	{"RELAY_VALUE",		8, 'B', 'W', "0", "1",		&relay_read,			&relay_write},
	{"RELAY_TIMEOUT",	8, 'I', 'W', "0", "65535",	&relay_timeout_read,	&relay_timeout_write},
	{"PWM_VALUE",		1, 'I', 'W', "0", "255",	&pwm_read,				&pwm_write},
	{"PWM_FREQ",		1, 'I', 'W', "0", "7",		&pwm_freq_read,			&pwm_freq_write}
};

const relay_output_def_t relay_ouputs[8] = {
	{(uint16_t) &DDRB, (uint16_t) &PORTB, _BV(2)},
	{(uint16_t) &DDRB, (uint16_t) &PORTB, _BV(3)},
	{(uint16_t) &DDRB, (uint16_t) &PORTB, _BV(4)},
	{(uint16_t) &DDRB, (uint16_t) &PORTB, _BV(5)},
	{(uint16_t) &DDRC, (uint16_t) &PORTC, _BV(0)},
	{(uint16_t) &DDRC, (uint16_t) &PORTC, _BV(1)},
	{(uint16_t) &DDRC, (uint16_t) &PORTC, _BV(2)},
	{(uint16_t) &DDRC, (uint16_t) &PORTC, _BV(3)},
};

volatile uint16_t relay_timeout[8] = {0};
volatile uint16_t relay_time_left[8] = {0};

volatile uint8_t *reg;
volatile uint8_t *ireg;

void module_init() {
	SET_PIN(DDR, PIN_PWM);
	CLR_PIN(PORT, PIN_PWM);
	
	for(uint8_t i = 0; i < 8; i++) {
		reg = (volatile uint8_t*) relay_ouputs[i].reg_mode;
		*reg |= relay_ouputs[i].bitmask;
		
		reg = (volatile uint8_t*) relay_ouputs[i].reg_output;
		*reg |= relay_ouputs[i].bitmask;
	}
	
	
	OCR2B = 0x00;
	
	/* Put Timer2 in Fast PWM mode */
	/* We will use OC2B pin in non-inverting mode (set at BOTTOM and clear on compare match) */
	TCCR2A = _BV(WGM21) | _BV(WGM20);
	
	/* Select clock from prescaler with 1024 division */
	TCCR2B = _BV(CS20) | _BV(CS21) | _BV(CS22);
}

void module_tick(uint8_t counter) {
	/* Only execute 1 time per second */
	if(counter != 0)
		return;
	
	/* No need to use an atomic block as interruptions are disabled inside an ISR */
	for(uint8_t port = 0; port < 8; port++)
		if(relay_timeout[port] && relay_time_left[port]) {
			relay_time_left[port]--;
			
			if(relay_time_left[port] == 0) {
				ireg = (volatile uint8_t*) relay_ouputs[port].reg_output;
				
				*ireg |= relay_ouputs[port].bitmask;
			}
		}
	
	return;
}

int relay_read(uint8_t port, char *result_buffer) {
	reg = (volatile uint8_t*) relay_ouputs[port].reg_output;
	
	result_buffer[0] = (*reg & relay_ouputs[port].bitmask) ? '0' : '1';
	result_buffer[1] = '\0';
	
	return 1;
}

int relay_write(uint8_t port, const char *value_buffer) {
	reg = (volatile uint8_t*) relay_ouputs[port].reg_output;
	
	if(value_buffer[0] == '0') {
		ATOMIC_BLOCK(ATOMIC_FORCEON) {
			relay_time_left[port] = 0;
			
			*reg |= relay_ouputs[port].bitmask;
		}
	} else if(value_buffer[0] == '1') {
		ATOMIC_BLOCK(ATOMIC_FORCEON) {
			if(relay_timeout[port])
				relay_time_left[port] = relay_timeout[port];
			
			*reg &= ~(relay_ouputs[port].bitmask);
		}
	} else {
		return 0;
	}
	
	return 1;
}

int relay_timeout_read(uint8_t port, char *result_buffer) {
	sprintf(result_buffer, "%u", relay_timeout[port]);
	
	return 1;
}

int relay_timeout_write(uint8_t port, const char *value_buffer) {
	unsigned int timeout_value;
	
	if(sscanf(value_buffer, "%u", &timeout_value) != 1)
		return 0;
	
	ATOMIC_BLOCK(ATOMIC_FORCEON) {
		if(timeout_value >= 65535)
			relay_timeout[port] = 65535;
		else
			relay_timeout[port] = (uint16_t) timeout_value;
	}
	
	return 1;
}

int pwm_read(uint8_t port, char *result_buffer) {
	sprintf(result_buffer, "%u", (unsigned int) OCR2B);
	
	return 1;
}

int pwm_write(uint8_t port, const char *value_buffer) {
	unsigned int pwm_value;
	
	if(sscanf(value_buffer, "%u", &pwm_value) != 1)
		return 0;
	
	/* Setting OCR2B to zero will not result in a true 0% duty cycle, so we must disable the waveform generation */
	if(pwm_value == 0) {
		CLEAR_BIT(TCCR2A, COM2B1);
	} else {
		if(pwm_value >= 255)
			OCR2B = 255;
		else
			OCR2B = (uint8_t) pwm_value;
		
		SET_BIT(TCCR2A, COM2B1);
	}
	
	return 1;
}

int pwm_freq_read(uint8_t port, char *result_buffer) {
	sprintf(result_buffer, "%u", (unsigned int) (TCCR2B & (_BV(CS20) | _BV(CS21) | _BV(CS22))));
	
	return 1;
}

int pwm_freq_write(uint8_t port, const char *value_buffer) {
	unsigned int div_value;
	
	if(sscanf(value_buffer, "%u", &div_value) != 1)
		return 0;
	
	if(div_value > 7)
		TCCR2B |= _BV(CS20) | _BV(CS21) | _BV(CS22);
	else
		TCCR2B = (TCCR2B & ~(_BV(CS20) | _BV(CS21) | _BV(CS22))) | ((uint8_t) div_value);
	
	SET_BIT(GTCCR, PSRASY);
	
	return 1;
}
