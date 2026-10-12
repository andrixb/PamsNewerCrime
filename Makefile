# Local dev shortcuts for Pam's Newer Crime.
# No git tag needed to test on hardware — just `make flash`.

# Find pio: PATH first, then the PlatformIO installer's persistent venv.
PIO    := $(shell command -v pio 2>/dev/null || echo $(HOME)/.platformio/penv/bin/pio)
ENV    ?= teensy40

.PHONY: build flash monitor clean size $(ENV)

build:            ## compile the default env ($(ENV))
	$(PIO) run -e $(ENV)

flash:            ## compile + flash to a connected Teensy in bootloader mode
	$(PIO) run -e $(ENV) -t upload

monitor:          ## serial console (115200)
	$(PIO) device monitor -e $(ENV)

size:             ## flash/RAM usage report
	$(PIO) run -e $(ENV) -t size

build40:          ## both hardware targets, handy before pushing a tag
	$(PIO) run -e teensy40

build41:
	$(PIO) run -e teensy41

all: build40 build41

clean:
	$(PIO) run -t clean
