#ifndef TWSKB_HPP_
#define TWSKB_HPP_

#include <inttypes.h>
#include "fifo_queue.hpp"
#include "kbdconfig.hpp"

enum {
	LOGL_ERROR,
	LOGL_WARN,
	LOGL_INFO,
	LOGL_DEBUG,
	LOGL_DEBUGV,
};

#ifdef LOGGING_LEVEL
#define LOG_PRINT(level, ...) if(level<=LOGGING_LEVEL){printf(__VA_ARGS__);}
#else
#define LOG_PRINT(level, ...)
#endif

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
	void key_press(char x);
	void key_release(char x);
	void key_releaseAll(void);
};

class Twskbd
{
private:
	unsigned long keytrans_ms[13];
	bool gpd_current[13]={false};
	bool gpd_trans[13]={false};
	KeyFifo kfifo;
	KbdInOut kbdio;
	int inproc;
	bool modkey_state[MODKEY_END]={false};
	bool modkey_locked[MODKEY_END]={false};

	int keyscan_push(unsigned long tsms);
	int on_pressed(KeyFifo::key_fifo_data_t *kd);
	int on_released(KeyFifo::key_fifo_data_t *kd);
	int proc_func(KeyFifo::key_indexmap_t ki);
	int proc_mod(KeyFifo::key_indexmap_t ki);
	int proc_reg(KeyFifo::key_indexmap_t ki0, KeyFifo::key_indexmap_t ki1);
	int proc_multi(unsigned int mkb);
	unsigned int multikey_bits(KeyFifo::key_fifo_data_t *kds[], int n);
	void clean_locked_status(void);

public:
	Twskbd(void);
	void main_loop(unsigned long tsms);

};

#endif
