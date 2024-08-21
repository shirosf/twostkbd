#ifndef FIFO_QUEUE_H_
#define FIFO_QUEUE_H_

#include "Arduino.h"

#define FIFO_MAX_DEPTH 16
#define FIFO_MAX_DEPTH_MASK (FIFO_MAX_DEPTH-1)

typedef struct key_fifo_data {
	byte ki;
	bool pressed;
	unsigned long tsms;
} key_fifo_data_t;

class KeyFifo
{
private:
	byte inp;
	byte outp;
	key_fifo_data_t kds[FIFO_MAX_DEPTH];
public:
	KeyFifo(void);
	int ninfifo(void);
	void push_kdata(key_fifo_data_t *kd);
	void pushkd(key_fifo_data_t *kd);
	key_fifo_data_t *popkd(unsigned long ctsms, int gapms);
	key_fifo_data_t *peekd(int n);
};
#endif
