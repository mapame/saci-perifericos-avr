PROGRAM		= controlador
OBJ			= main.o crc16.o comm.o

BAUD_RATE	= 115200
BAUD_TOL	= 3

MCU_TARGET	= atmega328p
AVR_FREQ	= 16000000
OPTIMIZE	= -Os

DEFS		=
LIBS		=

CC			= avr-gcc

CFLAGS		= -g -Wall $(OPTIMIZE) -mmcu=$(MCU_TARGET) -DF_CPU=$(AVR_FREQ) -DBAUD=$(BAUD_RATE) -DBAUD_TOL=$(BAUD_TOL) $(DEFS)
LDFLAGS		= -Wl,-Map,$(PROGRAM).map

OBJCOPY		= avr-objcopy
OBJDUMP		= avr-objdump

ISPCMD		= avrdude -c usbasp -p $(MCU_TARGET)

HFUSE		= C9
LFUSE		= FF
EFUSE		= FF

# -------------------------------------------------------------------------------------------

all:


relay_module.elf: module_relay.o

relay_module: PROGRAM = relay_module
relay_module: relay_module.hex

relay_module_isp: PROGRAM = relay_module
relay_module_isp: relay_module.hex
relay_module_isp: isp_fuses
relay_module_isp: isp_flash


water_level_module.elf: module_water_level.o
water_level_module.elf: DEFS = -DLED_ON_D3

water_level_module: PROGRAM = water_level_module
water_level_module: water_level_module.hex

water_level_module_isp: PROGRAM = water_level_module
water_level_module_isp: water_level_module.hex
water_level_module_isp: isp_fuses
water_level_module_isp: isp_flash

# -------------------------------------------------------------------------------------------

isp_lock:
	$(ISPCMD) -u -U lock:w:0x00:m

isp_fuses:
	$(ISPCMD) -u -U efuse:w:0x$(EFUSE):m -U hfuse:w:0x$(HFUSE):m -U lfuse:w:0x$(LFUSE):m

isp_flash:
	$(ISPCMD) -U flash:w:$(PROGRAM).hex

# -------------------------------------------------------------------------------------------

%.elf: $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)
	avr-size $@

%.lst: %.elf
	$(OBJDUMP) -h -S $< > $@

%.hex: %.elf
	$(OBJCOPY) -j .text -j .data -O ihex $< $@

%.srec: %.elf
	$(OBJCOPY) -j .text -j .data -O srec $< $@

%.bin: %.elf
	$(OBJCOPY) -j .text -j .data -O binary $< $@

clean:
	rm -rf *.o *.elf *.lst *.map *.sym *.lss *.eep *.srec *.bin *.hex
