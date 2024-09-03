SOURCES:=../fifo_queue.cpp ../twskb.cpp ../kbdconfig.cpp
HEADERS:=$(patsubst %.cpp,%.hpp,$(SOURCES))

all: twskbpc

../kbdconfig.cpp: ../gen_header.py
	cd ..; ./gen_header.py

twskbpc: $(SOURCES) $(HEADERS) twskbpc.cpp
	g++ -Wall -DLOGGING_LEVEL=LOGL_DEBUG -I.. -o $@ $(SOURCES) twskbpc.cpp
