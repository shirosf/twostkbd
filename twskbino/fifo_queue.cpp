#include <string.h>
#include "fifo_queue.hpp"

KeyFifo::KeyFifo(void) : inp(0), outp(0)
{
}

int KeyFifo::ninfifo(void)
{
	return (inp-outp>=0)?inp-outp:inp+FIFO_MAX_DEPTH-outp;
}

void KeyFifo::pushkd(key_fifo_data_t *kd)
{
	memcpy(&kds[inp++], kd, sizeof(key_fifo_data_t));
	inp&=FIFO_MAX_DEPTH_MASK;
}

KeyFifo::key_fifo_data_t *KeyFifo::popkd(unsigned long ctsms, int gapms)
{
	key_fifo_data_t *kd;
	if(outp==inp){return NULL;}
	if((ctsms - kds[(inp-1)&FIFO_MAX_DEPTH_MASK].tsms) < (unsigned long)gapms){
		return NULL;
	}
	kd=&kds[outp++];
	outp&=FIFO_MAX_DEPTH_MASK;
	return kd;
}

KeyFifo::key_fifo_data_t *KeyFifo::peekd(int n)
{
	if(ninfifo()<n){return NULL;}
	return &kds[(outp+n)&FIFO_MAX_DEPTH_MASK];
}
