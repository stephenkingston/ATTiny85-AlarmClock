MAKE_HEX = avr-objcopy
CC = avr-gcc

OUTPUT = ATTiny85_DigitalClock

SRCS += main.c

INCLUDE +=

CFLAGS += -Wall -g -O2 -mmcu=attiny85 --std=gnu99
BUILD_DIR = build

C_FILES = $(notdir $(SRCS))
C_PATHS = $(dir $(SRCS))
vpath %.c $(C_PATHS)


HEX = $(BUILD_DIR)/$(OUTPUT).hex
BIN = $(BUILD_DIR)/$(OUTPUT).bin


####### TARGETS ########

all: $(BUILD_DIR) $(BIN) $(HEX)


$(HEX): $(BIN)
	@$(MAKE_HEX) -j .text -j .data -O ihex $(BIN) $(HEX)
	@echo Created HEX file.

$(BUILD_DIR):
	@mkdir "$@"
	@echo Created build directory.

$(BIN): $(C_FILES) $(BUILD_DIR)
	@$(CC) $(CFLAGS) -o $@ $(addprefix -I,$(INCLUDE)) $(C_FILES)
	@echo Created BIN file.

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)
	@echo Removed build directory.	
.PHONY: flashUSBasp
flashUSBasp: $(HEX)
	avrdude -p attiny85 -c usbasp -U flash:w:$(HEX):i -F -P usb
	
.PHONY: flashMicronucleus
flashMicronucleus: $(HEX)
	micronucleus --run --type intel-hex $(HEX)