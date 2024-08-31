#include <string.h>
#include <stdio.h>
#include "twskb.hpp"

#define BOUNCE_GURD_MS 10
#define KEY_PROC_GAP_MS 50

int Twskbd::keyscan_push(unsigned long tsms)
{
	// read the pushbutton:
	int i;
	KeyFifo::key_fifo_data_t kd;
	int res=0;

	while(kbdio.get_inkey(&kd.ki, &kd.pressed)){
		if(gpd_current[kd.ki]==kd.pressed){
			// on->on, off->off, it must be a glitch
			gpd_trans[kd.ki]=false;
		}else{
			// new change, set a transition bit
			gpd_trans[kd.ki]=true;
			keytrans_ms[kd.ki]=tsms;
		}
	}
	for(i=0;i<13;i++){
		if(!gpd_trans[i]){continue;}
		if(keytrans_ms[i]+BOUNCE_GURD_MS < tsms){
			// transition timer expired, push 'kd' into the fifo
			gpd_current[i]=!gpd_current[i];
			gpd_trans[i]=false;
			kd.ki=(KeyFifo::key_indexmap_t)i;
			kd.tsms=tsms;
			kd.pressed=gpd_current[i];
			kfifo.pushkd(&kd);
			res+=1;
			if(kd.pressed){
				kbdio.set_rgb_led(kbdio.RGB_COLOR_RED);
			}else{
				kbdio.set_rgb_led(kbdio.RGB_COLOR_BLUE);
			}
		}
	}
	return res;
}

Twskbd::Twskbd(void)
{
	memset(gpd_current, 0, sizeof(gpd_current));
	memset(gpd_trans, 0, sizeof(gpd_trans));
	kbdio.set_rgb_led(kbdio.RGB_COLOR_GREEN);
}

void Twskbd::main_loop(unsigned long tsms)
{
	KeyFifo::key_fifo_data_t *kd;
	if(keyscan_push(tsms)>0){return;}
	kd=kfifo.peekd('r', 0, tsms, KEY_PROC_GAP_MS);
	if(kd==NULL){return;}
	kbdio.print_kd(kd);
	kfifo.increadp();
	kfifo.delkd('h', 0);
}
