SOURCES:=../fifo_queue.cpp ../twskb.cpp
HEADERS:=$(patsubst %.cpp,%.hpp,$(SOURCES))

all: twskbpc

twskbpc: $(SOURCES) $(HEADERS) twskbpc.cpp
	g++ -Wall -DLOGGING_LEVEL=LOGL_DEBUG -I.. -o $@ $(SOURCES) twskbpc.cpp
