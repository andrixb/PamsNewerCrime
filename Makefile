# Local dev shortcuts for Pam's Newer Crime.
# No git tag needed to test on hardware — just `make flash`.

# Find pio: PATH first, then the PlatformIO installer's persistent venv.
PIO    := $(shell command -v pio 2>/dev/null || echo $(HOME)/.platformio/penv/bin/pio)
ENV    ?= teensy40

CXX      ?= g++
TEST_BIN := build/test_pnc
TEST_SRC := test/test_main.cpp test/mock_hw.cpp \
            src/euclid.cpp src/clock.cpp src/channel.cpp src/storage.cpp
TESTFLAGS := -std=gnu++17 -Wall -Wextra -Wno-unused-function \
             -Itest/mock -Isrc -DNUM_OUTPUTS=4

.PHONY: build flash monitor test clean size dac-selftest $(ENV)

build:            ## compile the default env ($(ENV))
	$(PIO) run -e $(ENV)

flash:            ## compile + flash to a connected Teensy in bootloader mode
	$(PIO) run -e $(ENV) -t upload

monitor:          ## serial console (115200)
	$(PIO) device monitor -e $(ENV)

test:             ## native unit tests (mocked Arduino/EEPROM/DAC)
	@mkdir -p build
	$(CXX) $(TESTFLAGS) $(TEST_SRC) -o $(TEST_BIN)
	@./$(TEST_BIN)

dac-selftest:     ## build+flash a boot-time CV bring-up sweep (0->5->10->0 V per out)
	PLATFORMIO_BUILD_FLAGS="-DPNC_DAC_SELFTEST=1" $(PIO) run -e $(ENV) -t upload
	@echo "now: make monitor  -> watch 'DAC self-test', probe CV jacks with a meter"

size:             ## flash/RAM usage report
	$(PIO) run -e $(ENV) -t size

build40:          ## both hardware targets, handy before pushing a tag
	$(PIO) run -e teensy40

build41:
	$(PIO) run -e teensy41

all: build40 build41

clean:
	$(PIO) run -t clean
