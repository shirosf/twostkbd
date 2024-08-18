#include "twskbino.h"
#include <soc/gpio_reg.h>

#define LED_RED 46
#define LED_GREEN 0
#define LED_BLUE 45
#define BOUNCE_GURD_MS 10

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

const int buttonPin = 0;         // input pin for pushbutton
int previousButtonState = HIGH;  // for checking the state of a pushButton
int counter = 0;                 // button push counter

static byte key_gpios[13]=KEY_GPIO_LIST;
static unsigned long keytrans_ms[13];
static unsigned long keys_mask;
static byte keybit_index[32];
static unsigned long gpd;
static unsigned long gpd_current;
static unsigned long gpd_trans;
static unsigned long *gpdp=(unsigned long *)GPIO_IN_REG;

void set_rgb_led(int onled)
{
	digitalWrite(LED_RED, HIGH);
	digitalWrite(LED_GREEN, HIGH);
	digitalWrite(LED_BLUE, HIGH);
	if(onled==-1) return;
	digitalWrite(onled, LOW);
}


void setup() {
	int i;

	pinMode(LED_RED, OUTPUT);
	pinMode(LED_GREEN, OUTPUT);
	pinMode(LED_BLUE, OUTPUT);
	set_rgb_led(LED_GREEN);

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

static void on_change(byte ki, bool kon)
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

void loop() {
	// read the pushbutton:
	char pbuf[16];
	unsigned long ugpd, ngpd, mb, tms;
	int i;
	bool non, con;

	ngpd=*gpdp & keys_mask;
	tms=millis();
	ugpd=gpd^ngpd;
	mb=1;
	for(i=0;i<32;i++){
		if(ugpd & mb){
			con=(gpd_current & mb) == 0;
			non=(ngpd & mb) == 0;
			if(con==non){
				gpd_trans &= ~mb;
			}else{
				gpd_trans |= mb;
				keytrans_ms[keybit_index[i]]=tms;
			}
		}else{
			if((gpd_trans & mb) &&
			   (keytrans_ms[keybit_index[i]]+BOUNCE_GURD_MS < tms)){
				gpd_current ^= mb;
				gpd_trans &= ~mb;
				on_change(keybit_index[i], (gpd_current & mb) == 0);
			}
		}
		mb = mb << 1;
	}
	gpd=ngpd;
}
#endif
