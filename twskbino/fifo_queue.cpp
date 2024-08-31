#include <string.h>
#include <stdio.h>
#include "fifo_queue.hpp"

#define GET_TYPE_SPP(t) (t=='r')?&readp:&headp
#define GET_NEXT_EP(t) (t=='h')?readp:-1; // next of end point

KeyFifo::KeyFifo(void)
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

int KeyFifo::ninfifo(int8_t t)
{
	int8_t *sp;
	int8_t nep;
	fq_data_t *fqd;
	int i;
	sp=GET_TYPE_SPP(t);
	if(*sp==-1){return 0;}
	nep=GET_NEXT_EP(t);
	fqd=&fqds[*sp];
	for(i=0;i<FIFO_MAX_DEPTH;i++){
		if(fqd->next==nep){return i+1;}
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

int KeyFifo::delkd(int8_t t, int n)
{
	int8_t *sp;
	int8_t np, nep, pp1, pp2;
	int i;
	sp=GET_TYPE_SPP(t);
	nep=GET_NEXT_EP(t);
	np=*sp;
	pp1=-1;
	pp2=-1;
	for(i=0;i<=n;i++){
		if(np==nep){return -1;}
		pp2=pp1;
		pp1=np;
		np=fqds[np].next;
	}
	if(pp2==-1){
		// n==0
		if(headp==readp){headp=np;}
		*sp=np;
	}else{
		fqds[pp2].next=np;
	}
	if(np==-1){
		btmp=pp2;
	}
	fqds[pp1].busy=false;
	nep=GET_NEXT_EP(t);
	return 0;
}

KeyFifo::key_fifo_data_t *KeyFifo::peekd(int8_t t, int n, unsigned long ctsms, int gapms)
{
	fq_data_t *fqd;
	int8_t *sp;
	int8_t nep;
	int i;

	sp=GET_TYPE_SPP(t);
	if(*sp==-1){return NULL;}
	nep=GET_NEXT_EP(t);

	if(btmp==-1){return NULL;}
	if((ctsms - fqds[btmp].kd.tsms) < (unsigned long)gapms){
		return NULL;
	}
	fqd=&fqds[*sp];
	for(i=0;i<n;i++){
		if(fqd->next==nep){return NULL;}
		fqd=&fqds[fqd->next];
	}
	return &fqd->kd;
}
