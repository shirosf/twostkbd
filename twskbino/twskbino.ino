#include "twskbino.h"
#include <soc/gpio_reg.h>
#include "twskb.hpp"

#define LED_RED 46
#define LED_GREEN 0
#define LED_BLUE 45

#ifndef ARDUINO_USB_MODE
#error This ESP32 SoC has no Native USB interface
#elif ARDUINO_USB_MODE == 1
#warning This sketch should be used when USB is in OTG mode
void setup() {}
void loop() {}
#else

#include "USB.h"
#include "USBHIDKeyboard.h"

USBHIDKeyboard Keyboard;


Twskbd *twkd;

struct kbdio_data {
	unsigned char key_gpios[13];
	unsigned char keybit_index[32];
	unsigned long keys_mask;
	unsigned long gpd;
	unsigned long *gpdp;
	unsigned long ugpdp;
};

KbdInOut::KbdInOut(void)
{
	int i;
	unsigned char kbi[13]=KEY_GPIO_LIST;
	kbdiod=(struct kbdio_data*)malloc(sizeof(struct kbdio_data));
	memset(kbdiod, 0, sizeof(struct kbdio_data));
	kbdiod->gpdp=(unsigned long *)GPIO_IN_REG;
	memcpy(kbdiod->key_gpios, kbi, 13);
	memset(kbdiod->keybit_index, 0xff, sizeof(kbdiod->keybit_index));
	for(i=0;i<13;i++){
		pinMode(kbdiod->key_gpios[i], INPUT_PULLUP);
		kbdiod->keys_mask|=(1<<kbdiod->key_gpios[i]);
		kbdiod->keybit_index[kbdiod->key_gpios[i]]=i;
	}
	kbdiod->gpd=*kbdiod->gpdp & kbdiod->keys_mask;
	kbdiod->ugpdp=0;

	pinMode(LED_RED, OUTPUT);
	pinMode(LED_GREEN, OUTPUT);
	pinMode(LED_BLUE, OUTPUT);
	Keyboard.begin();
	USB.begin();
}

KbdInOut::~KbdInOut(void)
{
	free(kbdiod);
}

void KbdInOut::print_kd(KeyFifo::key_fifo_data_t *kd)
{
	char pbuf[4];
	if(kd->pressed){
		sprintf(pbuf, "%d+", kd->ki);
	}else{
		sprintf(pbuf, "%d-", kd->ki);
	}
	Keyboard.print(pbuf);
}

void KbdInOut::set_rgb_led(rgb_color_index_t ci)
{
	digitalWrite(LED_RED, HIGH);
	digitalWrite(LED_GREEN, HIGH);
	digitalWrite(LED_BLUE, HIGH);
	switch(ci){
	case RGB_COLOR_BLACK:
		return;
	case RGB_COLOR_RED:
		digitalWrite(LED_RED, LOW);
		return;
	case RGB_COLOR_GREEN:
		digitalWrite(LED_GREEN, LOW);
		return;
	case RGB_COLOR_BLUE:
		digitalWrite(LED_BLUE, LOW);
		return;
	case RGB_COLOR_YELLOW:
		digitalWrite(LED_RED, LOW);
		digitalWrite(LED_GREEN, LOW);
		return;
	case RGB_COLOR_CYAN:
		digitalWrite(LED_GREEN, LOW);
		digitalWrite(LED_BLUE, LOW);
		return;
	case RGB_COLOR_MAGENTA:
		digitalWrite(LED_RED, LOW);
		digitalWrite(LED_BLUE, LOW);
		return;
	case RGB_COLOR_WHITE:
		digitalWrite(LED_RED, LOW);
		digitalWrite(LED_GREEN, LOW);
		digitalWrite(LED_BLUE, LOW);
		return;
	}
}


bool KbdInOut::get_inkey(KeyFifo::key_indexmap_t *ki, bool *pressed)
{
	unsigned long ngpd;
	unsigned long mb;
	int i;
	if(kbdiod->ugpdp==0){
		ngpd=*kbdiod->gpdp & kbdiod->keys_mask;
		kbdiod->ugpdp=kbdiod->gpd^ngpd; // changed bits in ugpd
		kbdiod->gpd=ngpd;
	}
	if(kbdiod->ugpdp==0){return false;}
	mb=1;
	for(i=0;i<32;i++){
		if((kbdiod->ugpdp & mb) && (kbdiod->keybit_index[i]!=0xff)){
			*pressed=(kbdiod->gpd & mb) != mb;
			*ki=(KeyFifo::key_indexmap_t)kbdiod->keybit_index[i];
			kbdiod->ugpdp&=~mb;
			return true;
		}
		mb=mb<<1;
	}
	return false;
}



void setup()
{
	twkd=new(Twskbd);
}

void loop()
{
	twkd->main_loop(millis());
}

#endif
