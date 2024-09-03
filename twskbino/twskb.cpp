#include <string.h>
#include <stdio.h>
#include "twskb.hpp"
#include "kbdconfig.hpp"

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
	kbdio.set_rgb_led(kbdio.RGB_COLOR_GREEN);
}

int Twskbd::proc_func(KeyFifo::key_indexmap_t ki)
{
	LOG_PRINT(LOGL_DEBUG, "%s:ki=%d\n", __func__, ki);
	return 0;
}

int Twskbd::proc_mod(KeyFifo::key_indexmap_t ki)
{
	LOG_PRINT(LOGL_DEBUG, "%s:ki=%d\n", __func__, ki);
	return 0;
}

int Twskbd::proc_reg(KeyFifo::key_indexmap_t ki0, KeyFifo::key_indexmap_t ki1)
{
	int i;
	uint8_t d=0;
	LOG_PRINT(LOGL_DEBUGV, "%s:ki0=%d, ki1=%d\n", __func__, ki0, ki1);
	for(i=0;i<MODKEY_END;i++){
		if(modkey_state[i] || modkey_locked[i]){
			d=skey_table[ki0*6+ki1].mks[i];
			if(d!=0){break;}
		}
	}
	if(d!=0){
		kbdio.key_press(d);
	}else{
		d=skey_table[ki0*6+ki1].rk;
		kbdio.key_press(d);
	}
	return d;
}

int Twskbd::proc_multi(unsigned int mkb)
{
	return 0;
}

unsigned int Twskbd::multikey_bits(KeyFifo::key_fifo_data_t *kds[], int n)
{
	return 0;
}

void Twskbd::clean_locked_status(void)
{
}

int Twskbd::on_pressed(KeyFifo::key_fifo_data_t *kd)
{
	int i, gapms;
	unsigned int mkb;
	KeyFifo::key_fifo_data_t *kds[4];
	int ninf=kfifo.ninfifo('r', true);
	kds[0]=kd;
	if(ninf==0){return 0;} // never happen
	if(ninf==1){
		if(kds[0]->ki <= KeyFifo::KINDEX_K5){
			return 0;
		}else if(kds[0]->ki <= KeyFifo::KINDEX_F3){
			proc_func(kds[0]->ki);
			kfifo.increadp(kds[0]);
			return 0;
		}else{
			proc_mod(kds[0]->ki);
			kfifo.increadp(kds[0]);
			return 0;
		}
	}
	kds[1]=kfifo.peekd('r', 1, 0, 0, 1);
	gapms=kds[1]->tsms-kds[0]->tsms;
	if(ninf==2){
		if(inproc || (gapms>KEY_PROC_GAP_MS) ||
		   (multikey_bits(kds, 2)==0)){
			inproc=proc_reg(kds[0]->ki, kds[1]->ki);
			kfifo.delkd(kds[0]);
			kfifo.increadp(kds[1]);
			return 0;
		}
		return 0;
	}
	gapms=kds[2]->tsms-kds[1]->tsms;
	if(ninf==3){
		mkb=multikey_bits(kds, 3);
		if(inproc || (mkb==0)){
			if(gapms>KEY_PROC_GAP_MS){
				inproc=proc_reg(kds[0]->ki, kds[1]->ki);
				kfifo.delkd(kds[0]);
				kfifo.increadp(kds[1]);
				return 0;
			}
			// 3-multikey unmatch, throw away
			for(i=0;i<3;i++){kfifo.delkd(kds[i]);}
			clean_locked_status();
			return 0;
		}
		inproc=proc_multi(mkb);
		kfifo.increadp(kds[2]);
		return 0;
	}
	// maximum multikey number is 4, thraw away all unmatched combinations
	mkb=multikey_bits(kds, 4);
	if(inproc || (mkb==0) || (ninf>4)){
		for(i=0;i<ninf;i++){kfifo.delkd(kds[i]);}
		clean_locked_status();
		return 0;
	}
	inproc=proc_multi(mkb);
	kfifo.increadp(kds[3]);
	return 0;
}

int Twskbd::on_released(KeyFifo::key_fifo_data_t *kd)
{

	int n=kfifo.findkd('h', kd->ki, true);
	//LOG_PRINT(LOGL_DEBUG, "%s:n=%d\n", __func__, n);
	if(n==-1){
		kfifo.delkd(kd);
		return 0;
	}
	kfifo.delkn('h', n);
	kfifo.delkd(kd);
	if(inproc){
		kbdio.key_release(inproc);
		inproc=0;
	}
	LOG_PRINT(LOGL_DEBUG, "%s:head=%d, read=%d\n", __func__,
		  kfifo.ninfifo('h', false),  kfifo.ninfifo('r', false));
	return 0;
}

void Twskbd::main_loop(unsigned long tsms)
{
	int res=0;
	KeyFifo::key_fifo_data_t *kd;
	if(keyscan_push(tsms)>0){return;}
	kd=kfifo.peekd('r', 0, tsms, KEY_PROC_GAP_MS, 1);
	if(kd!=NULL){
		res=on_pressed(kd);
	}
	kd=kfifo.peekd('r', 0, tsms, KEY_PROC_GAP_MS, -1);
	if(kd!=NULL){
		res|=on_released(kd);
	}
	if(res!=0){
		kbdio.set_rgb_led(kbdio.RGB_COLOR_RED);
		LOG_PRINT(LOGL_ERROR, "%s:error\n", __func__);
	}
}
