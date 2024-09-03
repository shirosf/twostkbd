#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <termios.h>
#include <signal.h>
#include <stdlib.h>
#include "twskb.hpp"

static bool app_stop;
static int num_read;

struct kbdio_data {
	bool prev_state[13];
};

KbdInOut::KbdInOut(void)
{
	kbdiod=(struct kbdio_data*)malloc(sizeof(struct kbdio_data));
	memset(kbdiod, 0, sizeof(struct kbdio_data));
}

KbdInOut::~KbdInOut(void)
{
	free(kbdiod);
}

void KbdInOut::print_kd(KeyFifo::key_fifo_data_t *kd)
{
	if(kd->pressed){
		printf("%d+\n", kd->ki);
	}else{
		printf("%d-\n", kd->ki);
	}
}

void KbdInOut::set_rgb_led(rgb_color_index_t ci)
{
	switch(ci){
	case RGB_COLOR_BLACK:
		LOG_PRINT(LOGL_DEBUGV, "BLACK\n");
		return;
	case RGB_COLOR_RED:
		LOG_PRINT(LOGL_DEBUGV, "RED\n");
		return;
	case RGB_COLOR_GREEN:
		LOG_PRINT(LOGL_DEBUGV, "GREEN\n");
		return;
	case RGB_COLOR_BLUE:
		LOG_PRINT(LOGL_DEBUGV, "BLUE\n");
		return;
	case RGB_COLOR_YELLOW:
		LOG_PRINT(LOGL_DEBUGV, "YELLOW\n");
		return;
	case RGB_COLOR_CYAN:
		LOG_PRINT(LOGL_DEBUGV, "CYAN\n");
		return;
	case RGB_COLOR_MAGENTA:
		LOG_PRINT(LOGL_DEBUGV, "MAGENTA\n");
		return;
	case RGB_COLOR_WHITE:
		LOG_PRINT(LOGL_DEBUGV, "WHITE\n");
		return;
	}

}

bool KbdInOut::get_inkey(KeyFifo::key_indexmap_t *ki, bool *pressed)
{
	/*
	  a: KINDEX_K0,
	  b: KINDEX_K1,
	  c: KINDEX_K2,
	  d: KINDEX_K3,
	  e: KINDEX_K4,
	  f: KINDEX_K5,
	  g: KINDEX_F1,
	  h: KINDEX_F2,
	  i: KINDEX_F3,
	  j: KINDEX_ALT,
	  k: KINDEX_CTRL,
	  l: KINDEX_SHIFT,
	  m: KINDEX_EXT,
	*/
	char cb;
	int res;
	if(num_read<1){return false;}
	res=read(STDIN_FILENO, &cb, 1);
	if(res!=1){
		app_stop=true;
		return false;
	}
	num_read--;
	if(cb<'a' || cb>'m'){return false;}
	*ki=(KeyFifo::key_indexmap_t)(cb-'a');
	kbdiod->prev_state[*ki]=!kbdiod->prev_state[*ki];
	*pressed=kbdiod->prev_state[*ki];
	return true;
}

void KbdInOut::key_press(char x)
{
	if(x>=' '){
		printf("%c\n", x);
	}else{
		printf("0x%02X\n", x);
	}
}

void KbdInOut::key_release(char x)
{
}

void KbdInOut::key_releaseAll(void)
{
}

static void sighandler(int signo)
{
	app_stop=true;
}

int main(int argc, char *argv[])
{
	Twskbd *twkd;
	struct timeval tv;
	unsigned long tsms;
	struct termios ts_state, ts_save;
	struct sigaction sigact;
	fd_set rfds;

	/* setup signal handler */
	memset(&sigact, 0, sizeof(sigact));
	sigact.sa_handler=sighandler;
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = 0;
	sigaction(SIGINT,  &sigact, 0);
	sigaction(SIGTERM, &sigact, 0);
	signal(SIGPIPE, SIG_IGN);

	tcgetattr(STDIN_FILENO, &ts_state);
	memcpy(&ts_save, &ts_state, sizeof(ts_save));
	ts_state.c_lflag &= ~ICANON;
	ts_state.c_lflag &= ~ECHO;
	ts_state.c_cc[VMIN] = 1;
	tcsetattr(STDIN_FILENO, TCSANOW, &ts_state);

	twkd=new(Twskbd);
	while(!app_stop){
		tv.tv_sec=0;
		tv.tv_usec=10000;
		FD_ZERO(&rfds);
		FD_SET(STDIN_FILENO, &rfds);
		if(select(STDIN_FILENO+1, &rfds, NULL, NULL, &tv)==1){
			num_read++;
		}
		gettimeofday(&tv, NULL);
		tsms=tv.tv_sec*1000+tv.tv_usec/1000;
		twkd->main_loop(tsms);
	}

	tcsetattr(STDIN_FILENO, TCSANOW, &ts_save);
	return 0;
}
