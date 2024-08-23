#include "twskbino.h"
#include <soc/gpio_reg.h>

#define LED_RED 46
#define LED_GREEN 0
#define LED_BLUE 45
#define BOUNCE_GURD_MS 10
#define KEY_PROC_GAP_MS 100

#ifndef ARDUINO_USB_MODE
#error This ESP32 SoC has no Native USB interface
#elif ARDUINO_USB_MODE == 1
#warning This sketch should be used when USB is in OTG mode
void setup() {}
void loop() {}
#else

#include "USB.h"
#include "USBHIDKeyboard.h"
#include "fifo_queue.h"

USBHIDKeyboard Keyboard;

static void set_rgb_led(int onled)
{
	digitalWrite(LED_RED, HIGH);
	digitalWrite(LED_GREEN, HIGH);
	digitalWrite(LED_BLUE, HIGH);
	if(onled==-1) return;
	digitalWrite(onled, LOW);
}

class Twskbd
{
private:
	byte key_gpios[13]=KEY_GPIO_LIST;
	unsigned long keytrans_ms[13];
	unsigned long keys_mask;
	byte keybit_index[32];
	unsigned long gpd;
	unsigned long gpd_current;
	unsigned long gpd_trans;
	unsigned long *gpdp=(unsigned long *)GPIO_IN_REG;
	KeyFifo kfifo;

	void on_change(byte ki, bool kon)
	{
		char pbuf[32];
		if(kon){
			set_rgb_led(LED_BLUE);
			sprintf(pbuf, "%d ON\n", ki);
		}else{
			set_rgb_led(LED_RED);
			sprintf(pbuf, "%d OFF\n", ki);
		}
		Keyboard.print(pbuf);
	}

	int keyscan_push(unsigned long tsms)
	{
		// read the pushbutton:
		unsigned long ugpd, ngpd, mb;
		int i;
		bool non, con;
		key_fifo_data_t kd;
		int res=0;

		ngpd=*gpdp & keys_mask;
		ugpd=gpd^ngpd; // changed bits in ugpd
		mb=1;
		for(i=0;i<32;i++){
			if(keybit_index[i]==0xff){continue;}
			kd.ki=keybit_index[i];
			if(ugpd & mb){
				con=(gpd_current & mb) == 0; // current on state
				non=(ngpd & mb) == 0; // new on state
				if(con==non){
					// on->on, off->off, it must be a glitch
					gpd_trans &= ~mb;
				}else{
					// new change, set a transition bit
					gpd_trans |= mb;
					keytrans_ms[kd.ki]=tsms;
				}
			}else{
				// the bit has no change, check the transition timer
				if((gpd_trans & mb) &&
				   (keytrans_ms[kd.ki]+BOUNCE_GURD_MS < tsms)){
					// transition timer expired, push 'kd' into the fifo
					gpd_current ^= mb;
					gpd_trans &= ~mb;
					kd.tsms=tsms;
					kd.pressed=(gpd_current & mb) == 0;
					kfifo.pushkd(&kd);
					res+=1;
				}
			}
			mb = mb << 1;
		}
		gpd=ngpd;
		return res;
	}

public:
	Twskbd(void)
	{
		int i;
		memset(keybit_index, 0xff, sizeof(keybit_index));
		for(i=0;i<sizeof(key_gpios);i++){
			pinMode(key_gpios[i], INPUT_PULLUP);
			keys_mask|=(1<<key_gpios[i]);
			keybit_index[key_gpios[i]]=i;
		}
		gpd=*gpdp & keys_mask;
		gpd_current=gpd;

		Keyboard.begin();
		USB.begin();
	}

	void main_loop(void)
	{
		key_fifo_data_t *kd;
		unsigned long tsms=millis();
		if(keyscan_push(tsms)>0){return;}
		kd=kfifo.popkd(tsms, KEY_PROC_GAP_MS);
		if(kd==NULL){return;}
		on_change(kd->ki, kd->pressed);
	}
};

Twskbd *twkd;

void setup()
{
	int i;
	pinMode(LED_RED, OUTPUT);
	pinMode(LED_GREEN, OUTPUT);
	pinMode(LED_BLUE, OUTPUT);
	set_rgb_led(LED_GREEN);
	twkd=new(Twskbd);
}

void loop()
{
	twkd->main_loop();
}

#endif
