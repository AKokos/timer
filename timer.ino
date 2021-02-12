#include "Arduino.h"
#include <TM1637TinyDisplay.h>
#include <GyverButton.h>
#include <MusicWithoutDelay.h>
#include <EEPROM.h>
#include <BlinkControl.h>

/**
 * Пока в ожидании кнопками добавляем\убавляем время, при долгом нажатии ускоряем процесс
 * Старт по кнопке
 * Как закончили - моргают нули + звуковой сигнал,
 *    при нажатии на старт - сброс в начало на предустановленное ранее время
 *
 * В процессе отсчета нажали кнопку - пауза, повторное нажатие ресьюм, долгое - сброс
 */

#define CLK_PIN 7
#define DIO_PIN 8
#define BTN_PLUS_PIN 4
#define BTN_MINUS_PIN 5
#define BTN_START_PIN 6
#define PASSIZE_BUZZ_PIN 3
#define ACTIVE_BUZZ_PIN 9

#define DOT 0b01000000
#define MODE_WAIT 0
#define MODE_RUN 1
#define MODE_FINISH 2
#define MODE_CHANGE_VOLUME 3

#define MAX_TIME 99*60 + 50
#define MIN_TIME 10

// Initialize TM1637TinyDisplay
TM1637TinyDisplay display(CLK_PIN, DIO_PIN);
GButton plusBtn(BTN_PLUS_PIN);
GButton minusBtn(BTN_MINUS_PIN);
GButton startBtn(BTN_START_PIN);

unsigned long tmr;
unsigned long speedTmr;

unsigned int time;
unsigned long startTime = 1; //60;
byte mode = MODE_WAIT;
byte loudSignal = 0;

bool show = true;
byte speed = 1;
bool running = true;

const char song[] PROGMEM
		= {
				"Blue:d=8,o=5,b=140:a,a#,d,g,a#,c6,f,a,4a#,g,a#,d6,d#6,g,d6,c6,a#,d,g,a#,c6,f,a,4a#,g,a#,d6,d#6,g,d6,c6,a#,d,g,a#,c6,f,a,4a#,g,a#,d6,d#6,g,d6,c6,a#,d,g,a#,a,c,f,2g" };
MusicWithoutDelay buzzer(song);
BlinkControl loudBuzzer(ACTIVE_BUZZ_PIN);

void setup() {
	display.setBrightness(0x0f);

	plusBtn.setTimeout(500);
	minusBtn.setTimeout(500);
	startBtn.setTimeout(2000);

	buzzer.begin(CHB, SQUARE, ENVELOPE0, 0);
	pinMode(PASSIZE_BUZZ_PIN, INPUT);
	stopSignal();

	loudBuzzer.begin();
	loudBuzzer.blink4(); // Blink four times per second
	loudBuzzer.pause();

	EEPROM.get(0, loudSignal);
	showCurrentSignalLevel();
	delay(1500);
}

void loop() {
	plusBtn.tick();
	minusBtn.tick();
	startBtn.tick();
	buzzer.update();
	loudBuzzer.loop();

	switch (mode) {
		case MODE_WAIT:
			if (plusBtn.isHold() && minusBtn.isHold()) {
				mode = MODE_CHANGE_VOLUME;
			} else {
				changeStartTime(&plusBtn, 10);
				changeStartTime(&minusBtn, -10);
			}

			// start timer
			if (startBtn.isClick()) {
				mode = MODE_RUN;
				time = startTime;
				tmr = millis();
			}
			showTime(startTime);
			break;
		case MODE_RUN:
			// pause/resume
			if (startBtn.isClick()) {
				running = !running;
				if (running) {
					tmr = millis();
					showTime(time);
				}
			}

			// reset on hold
			if (startBtn.isHolded()) {
				mode = MODE_WAIT;
			}

			// running timer
			if (running && millis() - tmr >= 1000) {
				tmr = millis();
				showTime(--time);
				if (time == 0) {
					mode = MODE_FINISH;
					playSignal();
				}
			}

			// timer on pause
			if (!running && millis() - tmr >= 500) {
				tmr = millis();
				show = !show;
				showTime(time, show);
			}

			break;
		case MODE_FINISH:
			if (millis() - tmr >= 500) {
				tmr = millis();
				show = !show;
				if (show) {
					showTime(0);
				} else {
					display.clear();
				}
			}
			if (startBtn.isClick()) {
				stopSignal();
				mode = MODE_WAIT;
			}
			break;
		case MODE_CHANGE_VOLUME:
			if (millis() - tmr >= 500) {
				tmr = millis();
				show = !show;
				if (show) {
					showCurrentSignalLevel();
				} else {
					display.clear();
				}
			}

			if (plusBtn.isClick() || minusBtn.isClick()) {
				loudSignal = loudSignal ? 0 : 1;
			}

			if (startBtn.isClick()) {
				EEPROM.put(0, loudSignal);
				showCurrentSignalLevel();
				delay(1500);
				mode = MODE_WAIT;
			}
			break;
	}
}

void showCurrentSignalLevel() {
	display.showString(loudSignal ? "Loud" : "Soft");
}

void showTime(unsigned int dispTime) {
	showTime(dispTime, true);
}

void showTime(unsigned int dispTime, bool showColon) {
	byte min = dispTime / 60;
	byte sec = dispTime % 60;
	display.showNumberDec(min * 100 + sec, showColon ? DOT : 0, true);
}

void changeStartTime(GButton *btn, int delta) {
	if ((delta < 0 && startTime <= MIN_TIME) || (delta > 0 && startTime >= MAX_TIME)) {
		return;
	}
	if (btn->isClick()) {
		startTime += delta;
	}
	if (btn->isHolded()) {
		speed = 1;
		speedTmr = millis();
		tmr = millis();
	}
	if (btn->isHold() && (millis() - tmr) >= 100 / speed) {
		tmr = millis();
		startTime += delta;
		if (speed < 64 && millis() - speedTmr >= 1000) {
			speedTmr = millis();
			speed = speed << 1;
		}
	}
	if (btn->isClick()) {
		startTime += 10;
	}
}

void playSignal() {
	if (loudSignal) {
		loudBuzzer.resume();
	} else {
		pinMode(PASSIZE_BUZZ_PIN, OUTPUT); // prevent noise
		buzzer.play();
	}
}

void stopSignal() {
	if (loudSignal) {
		loudBuzzer.pause();
	} else {
		pinMode(PASSIZE_BUZZ_PIN, INPUT); // prevent noise
		buzzer.pause(true);
	}
}
