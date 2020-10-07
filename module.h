typedef struct {
	char name[16];
	uint8_t qty;
	char type;
	char min[8];
	char max[8];
	int (*r_function)(uint8_t, char *);
	int (*w_function)(uint8_t, const char *);
} channel_list_t;

extern const channel_list_t channel_list[];
extern const unsigned int module_channel_qty;
extern const char module_type[];

void module_init();
void module_tick(uint8_t counter);
