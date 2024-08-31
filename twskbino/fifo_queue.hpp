#ifndef FIFO_QUEUE_H_
#define FIFO_QUEUE_H_

#include <inttypes.h>

#define FIFO_MAX_DEPTH 16
#define FIFO_MAX_DEPTH_MASK (FIFO_MAX_DEPTH-1)


class KeyFifo
{
public:
	typedef enum {
		KINDEX_K0,
		KINDEX_K1,
		KINDEX_K2,
		KINDEX_K3,
		KINDEX_K4,
		KINDEX_K5,
		KINDEX_F1,
		KINDEX_F2,
		KINDEX_F3,
		KINDEX_ALT,
		KINDEX_CTRL,
		KINDEX_SHIFT,
		KINDEX_EXT,
	} key_indexmap_t;

	typedef struct key_fifo_data {
		key_indexmap_t ki;
		bool pressed;
		unsigned long tsms;
	} key_fifo_data_t;

	KeyFifo(void);
	int ninfifo(int8_t t);
	int increadp(void);
	int pushkd(key_fifo_data_t *kd);
	int delkd(int8_t t, int n);
	key_fifo_data_t *peekd(int8_t t, int n, unsigned long ctsms, int gapms);

private:
	typedef struct fq_data {
		key_fifo_data_t kd;
		int8_t next;
		bool busy;
	} fq_data_t;

	int8_t headp;
	int8_t btmp;
	int8_t readp;
	fq_data_t fqds[FIFO_MAX_DEPTH];

	int8_t getbuf(void);

};
#endif