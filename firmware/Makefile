# Firefly firmware - ATtiny13A, factory clock (1.2 MHz)
#
#   make            build build/firefly.hex and print its size
#   make check      fail if flash or RAM use is over budget
#   make test       host unit tests + swarm simulation + simulator tests
#   make flash      program with an AVRISP mkII (on the ISP jig, JP2 removed)
#   make fuses      optional: brown-out detector at 1.8 V (see docs/firmware.md)

MCU      = attiny13a
F_CPU    = 1200000UL
PROG     = avrispmkII
PORT     = usb
BITCLK   = 8          # avrdude -B (us); 1.2 MHz chip needs <= 300 kHz SCK

CC       = avr-gcc
OBJCOPY  = avr-objcopy
SIZE     = avr-size
CFLAGS   = -mmcu=$(MCU) -DF_CPU=$(F_CPU) -Os -std=gnu99 -Wall -Wextra -Werror \
           -ffunction-sections -fdata-sections -flto -mrelax
LDFLAGS  = -mmcu=$(MCU) -Wl,--gc-sections -flto -Os

SRC      = src/main.c src/firefly.c
ELF      = build/firefly.elf
HEX      = build/firefly.hex

FLASH_MAX = 1024
RAM_MAX   = 64
RAM_STATIC_BUDGET = 24   # leave >= 40 bytes for the stack

all: $(HEX) size

build:
	mkdir -p build

$(ELF): $(SRC) src/config.h src/firefly.h | build
	$(CC) $(CFLAGS) $(SRC) -o $@ $(LDFLAGS)

$(HEX): $(ELF)
	$(OBJCOPY) -O ihex -R .eeprom $< $@

size: $(ELF)
	@$(SIZE) --format=avr --mcu=$(MCU) $(ELF) | grep -E "Program|Data"

check: $(ELF)
	@text=$$(avr-size -A $(ELF) | awk '/^\.text/{t=$$2} /^\.data/{d=$$2} END{print t+d}'); \
	 ram=$$(avr-size -A $(ELF) | awk '/^\.data/{d=$$2} /^\.bss/{b=$$2} END{print d+b}'); \
	 echo "flash $$text / $(FLASH_MAX) bytes, static RAM $$ram / $(RAM_STATIC_BUDGET) bytes"; \
	 test $$text -le $(FLASH_MAX) && test $$ram -le $(RAM_STATIC_BUDGET)

test: $(ELF)
	$(MAKE) -C test

flash: $(HEX)
	avrdude -c $(PROG) -P $(PORT) -p t13 -B $(BITCLK) -U flash:w:$(HEX):i

fuses:
	avrdude -c $(PROG) -P $(PORT) -p t13 -B $(BITCLK) -U lfuse:w:0x6A:m -U hfuse:w:0xFD:m

clean:
	rm -rf build
	$(MAKE) -C test clean

.PHONY: all size check test flash fuses clean
