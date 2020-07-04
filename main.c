#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include <util/delay.h>

#include <stdio.h>
#include <string.h>

#include "common.h"
#include "comm.h"
#include "module.h"

unsigned int uptime = 0;

uint8_t timer_tick_counter = 0;

ISR (TIMER1_COMPA_vect) {
	wdt_reset();
	
	timer_tick_counter = (timer_tick_counter + 1) % 50;
	
	if(timer_tick_counter == 0)
		uptime++;
	
	module_tick(timer_tick_counter);
}

static void io_init() {
	/* Set pins as output */
	SET_PIN(DDR, PIN_LED);
	SET_PIN(DDR, PIN_DE);
	
	/* Enable pull-ups on inputs */
	SET_PIN(PORT, PIN_ADDR0);
	SET_PIN(PORT, PIN_ADDR1);
	SET_PIN(PORT, PIN_ADDR2);
	SET_PIN(PORT, PIN_ADDR3);
	SET_PIN(PORT, PIN_ADDR4);
	
	/* Turn LED off and DE pin low */
	CLR_PIN(PORT, PIN_DE);
	CLR_PIN(PORT, PIN_LED);
}

static void timer_init() {
	/* Configure Timer 1 in CTC mode to generate an interruption every 100ms */
	
	/* Set CTC mode with ICR1 as TOP and clock as clkio/256 */
	TCCR1B = _BV(WGM13) | _BV(WGM12) | _BV(CS12);
	
	/* Set OCR1A to obtain an 50Hz interrupt */
	OCR1A = (F_CPU / (256UL * 50UL)) - 1UL;
	
	/* Also set ICR1 to change the resolution of the counter */
	ICR1 = (F_CPU / (256UL * 50UL)) - 1UL;
	
	/* Enable the Output Compare A Match Interrupt */
	TIMSK1 = _BV(OCIE1A);
}

int main() {
	uint8_t addr;
	char content_buffer[32];
	char cmd;
	
	char response_buffer[32];
	
	int aux_channel_id;
	int aux_port_num;
	char * parameter_ptr[2];
	
	wdt_reset();
	wdt_enable(WDTO_1S);
	
	io_init();
	
	for(uint8_t i = 0; i < 4; i++) {
		_delay_ms(300);
		SET_PIN(PORT, PIN_LED);
		_delay_ms(200);
		CLR_PIN(PORT, PIN_LED);
		wdt_reset();
	}
	
	/* Read address value */
	addr = (GET_PIN(PIN, PIN_ADDR0) ? 0 : 1);
	addr |= (GET_PIN(PIN, PIN_ADDR1) ? 0 : 1) << 1;
	addr |= (GET_PIN(PIN, PIN_ADDR2) ? 0 : 1) << 2;
	addr |= (GET_PIN(PIN, PIN_ADDR3) ? 0 : 1) << 3;
	addr |= (GET_PIN(PIN, PIN_ADDR4) ? 0 : 1) << 4;
	
	comm_init(addr);
	
	wdt_reset();
	
	module_init();
	
	timer_init();
	
	/* Enable interruptions */
	sei();
	
	for(;;) {
		wdt_reset();
		
		SET_PIN(PORT, PIN_LED);
		cmd = comm_receive_command(content_buffer, 32);
		CLR_PIN(PORT, PIN_LED);
		
		_delay_ms(1);
		
		switch(cmd) {
			case 'P':
				ATOMIC_BLOCK(ATOMIC_FORCEON) {
					sprintf(response_buffer, "%s:%u:%u", module_type, module_channel_qty, uptime);
				}
				
				comm_send_response(cmd, response_buffer);
				break;
			case 'D':
				if(sscanf(content_buffer, "%u", &aux_channel_id) != 1) {
					comm_send_response(cmd, "\x15""PARAMETER_ERROR");
					break;
				}
				
				if(aux_channel_id >= module_channel_qty) {
					comm_send_response(cmd, "\x15""INVALID_CHANNEL");
					break;
				}
				
				sprintf(response_buffer, "%s:%u:%c:%c:%s:%s", channel_list[aux_channel_id].name, channel_list[aux_channel_id].qty, channel_list[aux_channel_id].type, channel_list[aux_channel_id].wr, channel_list[aux_channel_id].min, channel_list[aux_channel_id].max);
				comm_send_response(cmd, response_buffer);
				break;
			case 'R':
				if(!(parameter_ptr[0] = strchr(content_buffer, ':'))) {
					comm_send_response(cmd, "\x15""PARAMETER_ERROR");
					break;
				}
				
				*parameter_ptr[0] = '\0';
					
				if(sscanf(content_buffer, "%u", &aux_channel_id) != 1 || sscanf(parameter_ptr[0] + 1, "%u", &aux_port_num) != 1) {
					comm_send_response(cmd, "\x15""PARAMETER_ERROR");
					break;
				}
				
				if(aux_channel_id >= module_channel_qty) {
					comm_send_response(cmd, "\x15""INVALID_CHANNEL");
					break;
				}
				
				if(aux_port_num >= channel_list[aux_channel_id].qty) {
					comm_send_response(cmd, "\x15""INVALID_PORT");
					break;
				}
				
				(channel_list[aux_channel_id].r_function)((uint8_t) aux_port_num, response_buffer);
				comm_send_response(cmd, response_buffer);
				break;
			case 'W':
				if(!(parameter_ptr[0] = strchr(content_buffer, ':'))) {
					comm_send_response(cmd, "\x15""PARAMETER_ERROR");
					break;
				}
				
				*parameter_ptr[0] = '\0';
				
				if(!(parameter_ptr[1] = strchr(parameter_ptr[0] + 1, ':'))) {
					comm_send_response(cmd, "\x15""PARAMETER_ERROR");
					break;
				}
				
				*parameter_ptr[1] = '\0';
					
				if(sscanf(content_buffer, "%u", &aux_channel_id) != 1 || sscanf(parameter_ptr[0] + 1, "%u", &aux_port_num) != 1) {
					comm_send_response(cmd, "\x15""PARAMETER_ERROR");
					break;
				}
				
				if(aux_channel_id >= module_channel_qty) {
					comm_send_response(cmd, "\x15""INVALID_CHANNEL");
					break;
				}
				
				if(channel_list[aux_channel_id].wr == 'R') {
					comm_send_response(cmd, "\x15""CHANNEL_READ_ONLY");
					break;
				}
				
				if(aux_port_num >= channel_list[aux_channel_id].qty) {
					comm_send_response(cmd, "\x15""INVALID_PORT");
					break;
				}
				
				(channel_list[aux_channel_id].w_function)((uint8_t) aux_port_num, parameter_ptr[1] + 1);
				comm_send_response(cmd, "\x06");
				break;
			default:
				break;
		}
	}
	
	return 0;
}
