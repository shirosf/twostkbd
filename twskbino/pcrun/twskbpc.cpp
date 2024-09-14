#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <termios.h>
#include <signal.h>
#include <stdlib.h>
#include "twskb.hpp"

static bool app_stop;
static int num_read;
static bool rgbled[3];

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
		LOG_PRINT(LOGL_DEBUGV, "BLACK");
		rgbled[0]=false;rgbled[1]=false;rgbled[2]=false;
		break;
	case RGB_COLOR_RED:
		LOG_PRINT(LOGL_DEBUGV, "RED");
		rgbled[0]=true;rgbled[1]=false;rgbled[2]=false;
		break;
	case RGB_COLOR_GREEN:
		LOG_PRINT(LOGL_DEBUGV, "GREEN");
		rgbled[0]=false;rgbled[1]=true;rgbled[2]=false;
		break;
	case RGB_COLOR_BLUE:
		LOG_PRINT(LOGL_DEBUGV, "BLUE");
		rgbled[0]=false;rgbled[1]=false;rgbled[2]=true;
		break;
	case RGB_COLOR_YELLOW:
		LOG_PRINT(LOGL_DEBUGV, "YELLOW");
		rgbled[0]=true;rgbled[1]=true;rgbled[2]=false;
		break;
	case RGB_COLOR_CYAN:
		LOG_PRINT(LOGL_DEBUGV, "CYAN");
		rgbled[0]=false;rgbled[1]=true;rgbled[2]=true;
		break;
	case RGB_COLOR_MAGENTA:
		LOG_PRINT(LOGL_DEBUGV, "MAGENTA");
		rgbled[0]=true;rgbled[1]=false;rgbled[2]=true;
		break;
	case RGB_COLOR_WHITE:
		LOG_PRINT(LOGL_DEBUGV, "WHITE");
		rgbled[0]=true;rgbled[1]=true;rgbled[2]=true;
		break;
	}
	LOG_PRINT(LOGL_DEBUGV, " %d,%d,%d\n", rgbled[0],rgbled[1],rgbled[2]);
}

void KbdInOut::set_bit_led(rgb_color_index_t ci, bool ledon)
{
	switch(ci){
	case RGB_COLOR_RED:
		rgbled[0]=ledon;
		break;
	case RGB_COLOR_GREEN:
		rgbled[1]=ledon;
		break;
	case RGB_COLOR_BLUE:
		rgbled[2]=ledon;
		break;
	default:
		break;
	}
	LOG_PRINT(LOGL_DEBUG, "RGBLED %d,%d,%d\n", rgbled[0],rgbled[1],rgbled[2]);
}


bool KbdInOut::get_inkey(KeyFifo::key_indexmap_t *ki, bool *pressed)
{
	/*
	  a: KINDEX_K0,     //0
	  b: KINDEX_K1,     //1
	  c: KINDEX_K2,     //2
	  d: KINDEX_K3,     //3
	  e: KINDEX_K4,     //4
	  f: KINDEX_K5,     //5
	  g: KINDEX_F1,     //6
	  h: KINDEX_F2,     //7
	  i: KINDEX_F3,     //8
	  j: KINDEX_ALT,    //9
	  k: KINDEX_CTRL,   //10
	  l: KINDEX_SHIFT,  //11
	  m: KINDEX_EXT,    //12
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

void KbdInOut::key_press(uint8_t x)
{
	if(x>=' ' && x<='~'){
		printf("%c\n", x);
	}else{
		printf("0x%02X\n", x);
	}
}

void KbdInOut::key_release(uint8_t x)
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
