void comm_init(uint8_t addr);
void comm_send_response(char cmd, const char *content);
char comm_receive_command(char *content_buffer, uint8_t buffer_size);
