# to get arduino-cli
# https://arduino.github.io/arduino-cli/latest/getting-started/
#
# to use esp32-s3 boards, the libraries are the next
# https://github.com/espressif/arduino-esp32

FBQN:=espressif:esp32:adafruit_feather_esp32s3
THISDIR=$(shell pwd)
SKETCH=$(notdir $(THISDIR))
CLI:=arduino-cli
LIBDIR:=$(HOME)/Arduino/libraries
ADEV:=/dev/ttyACM0
BDIR:=$(THISDIR)/build
SOURCES:=fifo_queue.cpp twskb.cpp kbdconfig.cpp
HEADERS:=$(patsubst %.cpp,%.hpp,$(SOURCES))

all: $(BDIR)/$(SKETCH).ino.elf

kbdconfig.cpp: gen_header.py ../config.org
	./gen_header.py

$(BDIR)/$(SKETCH).ino.elf: $(SKETCH).ino $(SOURCES) $(HEADERS)
	$(CLI) compile --fqbn $(FBQN) --build-path $(BDIR) $(THISDIR)

install: $(BDIR)/$(SKETCH).ino.elf
	$(CLI) upload -p $(ADEV) --fqbn $(FBQN) --input-dir $(BDIR) $(SKETCH)
