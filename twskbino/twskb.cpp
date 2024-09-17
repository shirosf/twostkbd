#include <string.h>
#include <stdio.h>
#include "twskb.hpp"
#include "kbdconfig.hpp"

#define BOUNCE_GURD_MS 10
#ifdef PCRUN_MODE
#define KEY_PROC_GAP_MS 1000
#else
#define KEY_PROC_GAP_MS 50
#endif

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
				kbdio.set_bit_led(kbdio.RGB_COLOR_BLUE, true);
			}else{
				kbdio.set_bit_led(kbdio.RGB_COLOR_BLUE, false);
			}
		}
	}
	return res;
}

Twskbd::Twskbd(void)
{
	kbdio.set_rgb_led(kbdio.RGB_COLOR_BLACK);
}

int Twskbd::proc_func(KeyFifo::key_indexmap_t ki)
{
	int i;
	uint8_t d=0;
	int fi=ki-KeyFifo::KINDEX_F1+1;

	LOG_PRINT(LOGL_DEBUG, "%s:fi=%d\n", __func__, fi);
	if(fi>=KeyFifo::KINDEX_F3){return 0;}
	for(i=0;i<MODKEY_END;i++){
		if(modkey_state[i] || modkey_locked[i]){
			d=fkey_table[fi].mks[i];
			if(d!=0){break;}
		}
	}
	if(d!=0){
		kbdio.key_press(d);
	}else{
		d=fkey_table[fi].rk;
		kbdio.key_press(d);
	}
	unlock_modkeys();
	return d;
}

int Twskbd::get_mod_enum(KeyFifo::key_indexmap_t ki)
{
	switch(ki){
	case KeyFifo::KINDEX_ALT:
		return MODKEY_A;
	case KeyFifo::KINDEX_CTRL:
		return MODKEY_C;
	case KeyFifo::KINDEX_SHIFT:
		return MODKEY_S;
	case KeyFifo::KINDEX_EXT:
		return MODKEY_E;
	default:
		return -1;
	}
}

uint8_t Twskbd::get_mod_keycode(modkey_enum_t mi)
{
	switch(mi){
	case MODKEY_A:
		return KEYCODE_LEFT_ALT;
	case MODKEY_C:
		return KEYCODE_LEFT_CTRL;
	case MODKEY_S:
	        return KEYCODE_LEFT_SHIFT;
	case MODKEY_E:
		return KEYCODE_0;
	default:
		return KEYCODE_0;
	}
}

void Twskbd::set_modkeys_led(void)
{
	int i;
	bool alloff=true;
	for(i=0;i<MODKEY_END;i++){
		alloff&=!modkey_state[i] && !modkey_locked[i];
	}
	if(alloff){
		kbdio.set_bit_led(kbdio.RGB_COLOR_GREEN, false);
	}else{
		kbdio.set_bit_led(kbdio.RGB_COLOR_GREEN, true);
	}
}

void Twskbd::unlock_modkeys(void)
{
	int i;
	for(i=0;i<MODKEY_END;i++){
		if(!modkey_state[i]){
			kbdio.key_release(get_mod_keycode((modkey_enum_t)i));
		}
		modkey_locked[i]=false;
	}
	set_modkeys_led();
}

int Twskbd::proc_mod(KeyFifo::key_indexmap_t ki)
{
	int mi;
	mi=get_mod_enum(ki);
	if(mi<0){return -1;}
	if(modkey_locked[mi]){
		LOG_PRINT(LOGL_DEBUG, "%s:unlock ki=%d\n", __func__, ki);
		modkey_locked[mi]=false;
		modkey_state[mi]=false;
		kbdio.key_release(get_mod_keycode((modkey_enum_t)mi));
		set_modkeys_led();
		return 0;
	}
	LOG_PRINT(LOGL_DEBUG, "%s:state on ki=%d\n", __func__, ki);
	modkey_state[mi]=true;
	modkey_locked[mi]=true;
	kbdio.key_press(get_mod_keycode((modkey_enum_t)mi));
	kbdio.set_bit_led(kbdio.RGB_COLOR_GREEN, true);
	return 0;
}

int Twskbd::proc_reg(KeyFifo::key_indexmap_t ki0, KeyFifo::key_indexmap_t ki1)
{
	int i;
	uint8_t d=0;
	LOG_PRINT(LOGL_DEBUG, "%s:ki0=%d, ki1=%d\n", __func__, ki0, ki1);
	if(ki0>KeyFifo::KINDEX_K5 || ki1>KeyFifo::KINDEX_K5){return 0;}
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
	unlock_modkeys();
	return d;
}

int Twskbd::proc_multi(unsigned int mkb)
{
	unsigned int i,k;
	unsigned int seqkeys[]={KEYCODE_LEFT_CTRL, KEYCODE_0, KEYCODE_0, KEYCODE_0};
	switch(mkb){
	case KEYCODE_CHOME:
		seqkeys[1]=KEYCODE_HOME;
		break;
	case KEYCODE_CEND:
		seqkeys[1]=KEYCODE_END;
		break;
	case KEYCODE_CLEFT:
		seqkeys[1]=KEYCODE_LEFT;
		break;
	case KEYCODE_CRIGHT:
		seqkeys[1]=KEYCODE_RIGHT;
		break;
	case KEYCODE_CTLG:
		seqkeys[1]='g';
		break;
	case KEYCODE_CTLC:
		seqkeys[1]='c';
		break;
	case KEYCODE_CTLX:
		seqkeys[1]='x';
		break;
	case KEYCODE_ALTX:
		seqkeys[0]=KEYCODE_LEFT_ALT;
		seqkeys[1]='x';
		break;
	case KEYCODE_CTL_SLASH:
		seqkeys[1]='/';
		break;
	default:
		kbdio.key_press(mkb);
		unlock_modkeys();
		return mkb;
	}
	for(i=0;;i++){
		if(seqkeys[i]==KEYCODE_0){break;}
		k=seqkeys[i];
		kbdio.key_press(k);
	}
	inseq=i;
	unlock_modkeys();
	return k;
}

unsigned int Twskbd::multikey_bits(KeyFifo::key_fifo_data_t *kds[], int n,
				   unsigned int mb)
{
	int i;
	unsigned int res=0;
	for(i=0;i<n;i++){
		mb|=(1<<kds[i]->ki);
	}
	LOG_PRINT(LOGL_DEBUGV, "%s:n=%d, mb=0x%X\n", __func__, n, mb);
	for(i=0;;i++){
		if(mkey_table[i][0]==KEYCODE_0){break;}
		if(mkey_table[i][1]==mb){
			LOG_PRINT(LOGL_DEBUG, "%s:mkb=%d\n", __func__, mkey_table[i][0]);
			return mkey_table[i][0];
		}
		if((mkey_table[i][1]&mb)==mb){res=1;}
	}
	if(n==2){return res;}
	return 0;
}

void Twskbd::clean_locked_status(void)
{
	int i;
	inproc=0;
	inseq=0;
	for(i=0;i<MODKEY_END;i++){
		modkey_state[i]=false;
		modkey_locked[i]=false;
	}
	kfifo.clearfifo();
	kbdio.key_releaseAll();
	set_modkeys_led();
}

// check processing multikeys in 'h' area of the queue.
// combining them with newly pressed keys, process a new multikey
int Twskbd::proc_hmkb(KeyFifo::key_fifo_data_t *kds[], int n)
{
	unsigned int mkb, hmkb=0;
	int i;
	int hn=kfifo.ninfifo('h', KeyFifo::KEY_PRESSED);
	KeyFifo::key_fifo_data_t *kd;
	for(i=0;i<hn;i++){
		kd=kfifo.peekd('h', i, 0, 0, KeyFifo::KEY_PRESSED);
		hmkb|=(1<<kd->ki);
	}
	if(hmkb==0){return 0;}
	mkb=multikey_bits(kds, n, hmkb);
	if(mkb==0){return 0;}
	inproc=proc_multi(mkb);
	kfifo.increadp(kds[n-1]);
	return 1;
}

int Twskbd::on_pressed(KeyFifo::key_fifo_data_t *kd, bool onrel)
{
	int gapms;
	unsigned int mkb;
	KeyFifo::key_fifo_data_t *kds[4];
	int ninf=kfifo.ninfifo('r', KeyFifo::KEY_PRESSED);
	kds[0]=kd;
	if(ninf==0){return 0;} // never happen
	if(ninf==1){
		if(proc_hmkb(kds, 1)){return 0;}
		if(kds[0]->ki <= KeyFifo::KINDEX_K5){
			return 0;
		}else if(kds[0]->ki <= KeyFifo::KINDEX_F3){
			inproc=proc_func(kds[0]->ki);
			if(inproc>0){
				kfifo.increadp(kds[0]);
			}else{
				kfifo.delkd(kds[0]);
			}
			return 0;
		}else{
			proc_mod(kds[0]->ki);
			kfifo.increadp(kds[0]);
			return 0;
		}
	}
	kds[1]=kfifo.peekd('r', 1, 0, 0, KeyFifo::KEY_PRESSED);
	gapms=kds[1]->tsms-kds[0]->tsms;
	if(ninf==2){
		if(proc_hmkb(kds, 2)){return 0;}
		if(onrel || inproc || (gapms>KEY_PROC_GAP_MS) ||
		   (multikey_bits(kds, 2, 0)==0)){
			if(proc_mod(kds[0]->ki)==0){
				kfifo.increadp(kds[0]);
				return 0;
			}
			inproc=proc_reg(kds[0]->ki, kds[1]->ki);
			kfifo.delkd(kds[0]);
			if(inproc>0){
				kfifo.increadp(kds[1]);
			}else{
				kfifo.delkd(kds[1]);
			}
			return 0;
		}
		return 0;
	}
	kds[2]=kfifo.peekd('r', 2, 0, 0, KeyFifo::KEY_PRESSED);
	gapms=kds[2]->tsms-kds[1]->tsms;
	if(ninf==3){
		mkb=multikey_bits(kds, 3, 0);
		if(onrel || inproc || (mkb==0)){
			if(proc_mod(kds[0]->ki)==0){
				kfifo.increadp(kds[0]);
				return 0;
			}
			if(gapms>KEY_PROC_GAP_MS){
				inproc=proc_reg(kds[0]->ki, kds[1]->ki);
				kfifo.delkd(kds[0]);
				if(inproc>0){
					kfifo.increadp(kds[1]);
				}else{
					kfifo.delkd(kds[1]);
				}
				return 0;
			}
			// 3-multikey unmatch, throw away
			clean_locked_status();
			return 0;
		}
		inproc=proc_multi(mkb);
		kfifo.increadp(kds[2]);
		return 0;
	}
	// maximum multikey number is 4, thraw away all unmatched combinations
	kds[3]=kfifo.peekd('r', 3, 0, 0, KeyFifo::KEY_PRESSED);
	mkb=multikey_bits(kds, 4, 0);
	if(inproc || (mkb==0) || (ninf>4)){
		clean_locked_status();
		return 0;
	}
	inproc=proc_multi(mkb);
	kfifo.increadp(kds[3]);
	return 0;
}

int Twskbd::on_released(KeyFifo::key_fifo_data_t *kd)
{
	int mi;
	int n=kfifo.findkd('h', kd->ki, true);
	//LOG_PRINT(LOGL_DEBUG, "%s:n=%d\n", __func__, n);
	if(n==-1){
		kfifo.delkd(kd);
		return 0;
	}
	mi=get_mod_enum(kd->ki);
	if(mi>=0){
		modkey_state[mi]=false;
		LOG_PRINT(LOGL_DEBUG, "%s:mod state off ki=%d\n", __func__, kd->ki);
		if(!modkey_locked[mi]){
			kbdio.key_release(get_mod_keycode((modkey_enum_t)mi));
			set_modkeys_led();
		}
	}
	kfifo.delkn('h', n, KeyFifo::KEY_ANY);
	kfifo.delkd(kd);
	if(inseq){
		if(--inseq==0){
			clean_locked_status();
		}else{
			kbdio.key_releaseAll();
		}
	}else if(inproc){
		kbdio.key_release(inproc);
		inproc=0;
	}
	LOG_PRINT(LOGL_DEBUG, "%s:head=%d, read=%d\n", __func__,
		  kfifo.ninfifo('h', KeyFifo::KEY_ANY),
		  kfifo.ninfifo('r', KeyFifo::KEY_ANY));
	return 0;
}

void Twskbd::main_loop(unsigned long tsms)
{
	int res=0;
	bool onrel=false;
	KeyFifo::key_fifo_data_t *kd, *pkd=NULL;
	if(keyscan_push(tsms)>0){return;}
	while(true){
		kd=kfifo.peekd('r', 0, tsms, KEY_PROC_GAP_MS, KeyFifo::KEY_ANY);
		if(kd==NULL){break;}
		if(kd==pkd){
			// when the first "pressed" event is pending in the queue,
			// "release" event must be checked in deeper places in the queue
			kd=kfifo.peekd('r', 0, tsms, KEY_PROC_GAP_MS, KeyFifo::KEY_RELEASED);
			if(kd==NULL){break;}
			res|=on_released(kd);
			continue;
		}
		// when release events in the queue, the process shouldn't be on hold.
		// "onrel" is set for that purpose.
		onrel=kfifo.ninfifo('r', KeyFifo::KEY_RELEASED)>0;
		if(kd->pressed){
			res|=on_pressed(kd, onrel);
		}else{
			res|=on_released(kd);
		}
		pkd=kd;
	}
	if(res!=0){
		kbdio.set_rgb_led(kbdio.RGB_COLOR_RED);
		LOG_PRINT(LOGL_ERROR, "%s:error\n", __func__);
	}
}
