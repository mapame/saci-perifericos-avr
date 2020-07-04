#include <stdio.h>
#include <avr/io.h>
#include <util/setbaud.h>
#include <util/delay.h>

#include "common.h"
#include "crc16.h"

char device_address[3];

void comm_init(uint8_t addr) {
	/* 
	 * Use USART module in asynchronous mode and 8N1 configuration, since its the default configuration, we
	 * just need to set the baud rate generator register, enable the double rate bit in UCSR0A if needed and
	 * then enable both the transmitter and receiver. 
	 */
	
	/* Set the USART baud rate register to values calculated in setbaud.h */
	UBRR0H = UBRRH_VALUE;
	UBRR0L = UBRRL_VALUE;
	
	/* Enable Double Rate if needed */
	#if USE_2X
	SET_BIT(UCSR0A, U2X0);
	#else
	CLEAR_BIT(UCSR0A, U2X0);
	#endif
	
	/* Convert device address to hex */
	sprintf(device_address, "%02X", addr);
	
	/* Enable UART transmitter and receiver */
	UCSR0B = _BV(TXEN0) | _BV(RXEN0);
}

void comm_send_response(char cmd, const char *content) {
	char tx_buffer[64];
	uint8_t tx_size;
	uint16_t checksum;
	
	tx_size = (uint8_t) sprintf(tx_buffer, "\x01""FF:%c""\x02""%s""\x03", cmd, content);
	
	checksum = crc16_calculate((uint8_t*)tx_buffer, tx_size);
	
	sprintf(&tx_buffer[tx_size], "%04X", checksum);
	
	SET_PIN(PORT, PIN_DE);
	
	_delay_us(50);
	
	for(uint8_t pos = 0; pos < (tx_size + 4); pos++) {
		loop_until_bit_is_set(UCSR0A, UDRE0);
		UDR0 = *(tx_buffer + pos);
	}
	
	loop_until_bit_is_set(UCSR0A, TXC0); /* Wait until transmission is completed by checking TXC0 */
	SET_BIT(UCSR0A, TXC0); /* Clear TXC0 bit by setting it to one */
	
	_delay_us(50);
	
	CLR_PIN(PORT, PIN_DE);
}

char comm_receive_command(char *content_buffer, uint8_t buffer_size) {
	char c;
	uint8_t state = 0;
	
	char rx_buffer[64];
	uint8_t buf_len = 0;
	
	char cmd = '\0';
	
	uint8_t content_len = 0;
	
	char checksum[5];
	
	while(state <= 10) {
		loop_until_bit_is_set(UCSR0A, RXC0);
		
		/* If there was a frame error just discard the byte */
		if(bit_is_set(UCSR0A, FE0)) {
			c = UDR0;
			continue;
		}
		
		c = UDR0;
		
		if(c == '\x01') { /* SOH */
			state = 1;
			
			rx_buffer[0] = c;
			
			buf_len = 1;
			
			continue;
		}
		
		if(state == 0)
			continue;
		
		if(state <= 6)
			rx_buffer[buf_len++] = c;
		
		switch(state) {
			case 1:
			case 2:
				if(c == device_address[state - 1])
					state++;
				else
					state = 0;
				
				break;
			case 3:
				state = (c == ':') ? 4 : 0;
				break;
			case 4:
				if(c >= 'A' && c <= 'Z') {
					state = 5;
					cmd = c;
				} else
					state = 0;
				
				break;
			case 5:
				state = (c == '\x02') ? 6 : 0; /* STX */
				break;
			case 6:
				if(c == '\x03') { /* ETX */
					state = 7;
					sprintf(checksum, "%04X", crc16_calculate((uint8_t*)rx_buffer, buf_len));
					content_buffer[content_len] = '\0';
					break;
				}
				
				/* If the rx buffer is full and the received byte is not ETX discard the command and start over */
				if(buf_len == 64) {
					state = 0;
					continue;
				}
				
				if(content_len < (buffer_size - 1))
					content_buffer[content_len++] = c;
				
				break;
			case 7:
			case 8:
			case 9:
			case 10:
				if(c == checksum[state - 7])
					state++;
				else
					state = 0;
				break;
			default:
				break;
		}
	}
	
	return cmd;
}
