#ifndef TWSKB_HPP_
#define TWSKB_HPP_

#include "fifo_queue.hpp"

struct kbdio_data;

class KbdInOut
{
private:
	struct kbdio_data *kbdiod;
public:
	typedef enum {
		RGB_COLOR_BLACK,
		RGB_COLOR_RED,
		RGB_COLOR_GREEN,
		RGB_COLOR_BLUE,
		RGB_COLOR_YELLOW,
		RGB_COLOR_CYAN,
		RGB_COLOR_MAGENTA,
		RGB_COLOR_WHITE,
	} rgb_color_index_t;

	KbdInOut(void);
	~KbdInOut(void);
	bool get_inkey(KeyFifo::key_indexmap_t *ki, bool *pressed);
	void print_kd(KeyFifo::key_fifo_data_t *kd);
	void set_rgb_led(rgb_color_index_t ci);
};

class Twskbd
{
private:
	unsigned long keytrans_ms[13];
	bool gpd_current[13];
	bool gpd_trans[13];
	KeyFifo kfifo;
	KbdInOut kbdio;

	int keyscan_push(unsigned long tsms);

public:
	Twskbd(void);
	void main_loop(unsigned long tsms);

};

#endif
