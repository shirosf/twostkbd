#ifndef FIFO_QUEUE_H_
#define FIFO_QUEUE_H_

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
	int ninfifo(void);
	void push_kdata(key_fifo_data_t *kd);
	void pushkd(key_fifo_data_t *kd);
	key_fifo_data_t *popkd(unsigned long ctsms, int gapms);
	key_fifo_data_t *peekd(int n);

private:
	unsigned char inp;
	unsigned char outp;
	key_fifo_data_t kds[FIFO_MAX_DEPTH];

};
#endif
