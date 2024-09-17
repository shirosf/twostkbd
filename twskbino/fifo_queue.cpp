/*
 * one directional link liss
 * ----------------------------
 *  ^       ^        ^
 *  headp   readp    btmp
 *
 *  push kd at 'btmp'
 *  'readp' is a read point, 'increadp' moves 'readp' one point to
 *  'btmp' direction.
 *  Data between 'headp' and 'readp' is hold until removed by 'delkn'.
 */
#include <string.h>
#include <stdio.h>
#include "fifo_queue.hpp"
#include "twskb.hpp"

#define GET_TYPE_SPP(t) (t=='r')?&readp:&headp
#define GET_NEXT_EP(t) (t=='h')?readp:-1; // next of end point

KeyFifo::KeyFifo(void)
{
	clearfifo();
}

void KeyFifo::clearfifo(void)
{
	headp=-1;
	btmp=-1;
	readp=-1;
	memset(fqds, 0, sizeof(fqds));
}

int8_t KeyFifo::getbuf(void)
{
	int i;
	for(i=0;i<FIFO_MAX_DEPTH;i++){
		if(!fqds[i].busy){
			fqds[i].busy=true;
			fqds[i].next=-1;
			return i;
		}
	}
	return -1;
}

int KeyFifo::ninfifo(int8_t t, key_event_type_t evtype)
{
	int8_t *sp;
	int8_t nep;
	fq_data_t *fqd;
	int i;
	int count;
	sp=GET_TYPE_SPP(t);
	nep=GET_NEXT_EP(t);
	if(*sp==nep){return 0;}
	fqd=&fqds[*sp];
	count=0;
	for(i=0;i<FIFO_MAX_DEPTH;i++){
		switch(evtype){
		case KEY_PRESSED:
			if(fqd->kd.pressed){count++;}
			break;
		case KEY_RELEASED:
			if(!fqd->kd.pressed){count++;}
			break;
		case KEY_ANY:
			count++;
			break;
		}
		if(fqd->next==nep){return count;}
		if(fqd->next==-1){
			LOG_PRINT(LOGL_ERROR, "%s:%c,corrupted, hp=%d, rp=%d, bp=%d, nep=%d\n",
				  __func__, t, headp, readp, btmp, nep);
			return 0;
		}
		fqd=&fqds[fqd->next];
	}
	return 0;
}

int KeyFifo::increadp(void)
{
	if(readp==-1){return -1;}
	readp=fqds[readp].next;
	return 0;
}

int KeyFifo::increadp(key_fifo_data_t *kd)
{
	int8_t np=readp;
	while(np!=-1){
		if((fqds[np].kd.ki==kd->ki) && (fqds[np].kd.pressed==kd->pressed)){
			readp=fqds[np].next;
			LOG_PRINT(LOGL_DEBUGV, "%s:readp=%d\n", __func__, readp);
			return 0;
		}
		np=fqds[np].next;
	}
	return -1;
}

int KeyFifo::pushkd(key_fifo_data_t *kd)
{
	int nk;
	nk=getbuf();
	if(nk==-1){return -1;}
	memcpy(&fqds[nk].kd, kd, sizeof(key_fifo_data_t));
	if(btmp!=-1){
		fqds[btmp].next=nk;
	}
	btmp=nk;
	if(headp==-1){headp=nk;}
	if(readp==-1){readp=nk;}
	return 0;
}

int KeyFifo::delkn(int8_t t, int n, key_event_type_t evtype)
{
	int8_t *sp;
	int8_t np, nep, pp1, pp2;
	int count=-1;
	sp=GET_TYPE_SPP(t);
	nep=GET_NEXT_EP(t);
	np=headp;
	pp1=-1;
	pp2=-1;
	while(np!=-1){
		if(np==nep){return -1;}
		if(np==*sp){count=0;}
		pp2=pp1;
		pp1=np;
		np=fqds[np].next;
		if(count>=0){
			if(count==n){break;}
			switch(evtype){
			case KEY_RELEASED:
				if(fqds[pp1].kd.pressed){count++;}
				break;
			case KEY_PRESSED:
				if(!fqds[pp1].kd.pressed){count++;}
				break;
			case KEY_ANY:
				count++;
				break;
			}
		}
	}
	if(count!=n){return -1;}
	if(n==0){
		if(headp==readp){headp=np;}
		*sp=np;
	}
	if(pp2!=-1){
		fqds[pp2].next=np;
	}
	if(pp1==-1){
		LOG_PRINT(LOGL_ERROR, "%s:%c,corrupted, hp=%d, rp=%d, bp=%d, nep=%d\n",
			  __func__, t, headp, readp, btmp, nep);
		return -1;
	}
	if(btmp==pp1){
		btmp=pp2;
	}
	fqds[pp1].busy=false;
	LOG_PRINT(LOGL_DEBUGV, "%s:%c, n=%d, hp=%d, rp=%d, bp=%d deleted=%d\n", __func__,
		  t, n, headp, readp, btmp, pp1);
	return 0;
}

int KeyFifo::delkd(key_fifo_data_t *kd)
{
	int8_t n=0;
	int8_t np=headp;
	int8_t hn=ninfifo('h', KeyFifo::KEY_ANY);
	while(np!=-1){
		if((fqds[np].kd.ki==kd->ki) && (fqds[np].kd.pressed==kd->pressed)){
			if(n<hn){return delkn('h', n, KEY_ANY);}
			return delkn('r', n-hn, KEY_ANY);
		}
		n++;
		np=fqds[np].next;
	}
	return -1;
}

KeyFifo::key_fifo_data_t *KeyFifo::peekd(int8_t t, int n, unsigned long ctsms,
					 int gapms, key_event_type_t evtype)
{
	fq_data_t *fqd;
	int8_t *sp;
	int8_t nep;
	int i;
	int count=0;

	sp=GET_TYPE_SPP(t);
	nep=GET_NEXT_EP(t);
	if(*sp==nep){return NULL;}

	if(btmp==-1){return NULL;}
	if((gapms!=0) &&
	   ((ctsms - fqds[btmp].kd.tsms) < (unsigned long)gapms)){
		return NULL;
	}
	fqd=&fqds[*sp];
	if(evtype==KEY_PRESSED){
		count=fqd->kd.pressed?0:-1;
	}else if(evtype==KEY_RELEASED){
		count=fqd->kd.pressed?-1:0;
	}
	i=0;
	while(true){
		if(evtype!=0){
			if(count==n){break;}
		}else{
			if(i==n){break;}
		}
		if(fqd->next==nep){return NULL;}
		if(fqd->next==-1){
			LOG_PRINT(LOGL_ERROR, "%s:fifo corrupted\n", __func__);
			return NULL;
		}
		fqd=&fqds[fqd->next];
		i++;
		if(evtype==1){
			if(fqd->kd.pressed){count++;}
		}else if(evtype==-1){
			if(!fqd->kd.pressed){count++;}
		}
	}
	return &fqd->kd;
}

int KeyFifo::findkd(int8_t t, key_indexmap_t ki, bool pressed)
{
	int8_t *sp;
	int8_t np, nep;
	int n=0;
	sp=GET_TYPE_SPP(t);
	np=*sp;
	nep=GET_NEXT_EP(t);
	if(*sp==nep){return -1;}
	while(np!=nep){
		if((fqds[np].kd.ki==ki) && (fqds[np].kd.pressed==pressed)){
			return n;
		}
		np=fqds[np].next;
		n++;
	}
	return -1;
}
