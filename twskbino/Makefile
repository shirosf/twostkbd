# to get arduino-cli
# https://arduino.github.io/arduino-cli/latest/getting-started/
#
# /etc/udev/rules.d/99-usb-serial.rules, the next line makes the symlink
# SUBSYSTEM=="tty" ATTRS{manufacturer}=="Arduino*" SYMLINK+="arduino"
#

FBQN:=espressif:esp32:adafruit_feather_esp32s3
THISDIR=$(shell pwd)
SKETCH=$(notdir $(THISDIR))
CLI:=arduino-cli
LIBDIR:=$(HOME)/Arduino/libraries
ADEV:=/dev/ttyACM0
BDIR:=$(THISDIR)/build

all: $(BDIR)/$(SKETCH).ino.elf

$(BDIR)/$(SKETCH).ino.elf: $(SKETCH).ino fifo_queue.cpp fifo_queue.hpp twskb.cpp \
twskb.hpp
	$(CLI) compile --fqbn $(FBQN) --build-path $(BDIR) $(THISDIR)

install: $(BDIR)/$(SKETCH).ino.elf
	$(CLI) upload -p $(ADEV) --fqbn $(FBQN) --input-dir $(BDIR) $(SKETCH)
