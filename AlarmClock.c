/**
 * Author: JK
 * Date: 10feb2024
 * Libraries: Tiny4kOLED by Datacute, RTC by Makuna.
 */

#define MCU attiny84
#define F_CPU 8000000UL

//Libraries
#include <Tiny4kOLED.h>
#include <RtcDS1302.h>
#include <EEPROM.h>
#include <avr/io.h>
#include <util/delay.h>
#include <stdbool.h> 

//Pins
#define PIN_CLR PA0
#define PIN_RST PA1
#define PIN_DAT PA2
#define PIN_BTN_UP PA7
#define PIN_BTN_DOWN PB2
#define PIN_IR PB1
#define PIN_ALARM PA3

//RTC
ThreeWire myWire(PIN_DAT, PIN_CLR, PIN_RST);
RtcDS1302<ThreeWire> rtc(myWire);
RtcDateTime now;

//Logic
byte alarmHour = 9;
byte alarmMinute = 30;
bool alarmRinging = false;
byte setting = 0; //0-Alarm, 1-Hour, 2-Minute
bool alarmStopped = false;
const byte alarmDuration = 1;
const byte eepromAlarmHourAddress = 0;
const byte eepromAlarmMinuteAddress = 1;

//Pin masking constants
const byte BTN_UP_REG = B10000000; //PA7
const byte BTN_DOWN_REG = B00000100; //PB2

int main(void) {
	//RTC
	rtc.Begin();
	if(rtc.GetIsWriteProtected()){rtc.SetIsWriteProtected(false);} 
	/*now.InitWithDateTimeFormatString("hh:mm", "23:31");
	rtc.SetDateTime(now);*/
	if(!rtc.GetIsRunning()){rtc.SetIsRunning(true);}
	  
	//OLed
	oled.begin(0,0);
	oled.enableChargePump();
	oled.setRotation(1);
	//oled.setInternalIref(true);   //True gives high internal current reference, resulting in a brighter display
	oled.enableZoomIn();            //In order to use double buffering on the 128x64 screen,
	oled.setFont(FONT8X16);

	// Swap which half of RAM is being written to, and which half is being displayed.
	// This is equivalent to calling both switchRenderFrame and switchDisplayFrame.
	#ifndef NO_DOUBLE_BUFFERING
	  oled.switchRenderFrame();
	  updateDisplay();
	  oled.switchFrame();
	#endif
	oled.on();

	//Retain alarm time from EEProm
	if(EEPROM.read(eepromAlarmHourAddress)<24){
	alarmHour = EEPROM.read(eepromAlarmHourAddress);}
	if(EEPROM.read(eepromAlarmMinuteAddress)<=60){
	alarmMinute = EEPROM.read(eepromAlarmMinuteAddress);}

	//Avoid floating unused pins by pullup
	PORTA = 0xFF;
	PORTB = 0xFF;

	//Pin Setup
	DDRA |= (1 << PIN_ALARM);       //Output

	DDRA &= ~(1 << DDA7);           //Input
	PORTA |= (1 << PIN_BTN_UP);     //Pullup
	  
	DDRA &= ~(1 << DDB2);           //Input
	PORTA |= (1 << PIN_BTN_DOWN);   //Pullup

	PORTA &= ~(1 << PIN_ALARM);     //Low


	while(1) {
	  now = rtc.GetDateTime();
	  alarmCheck();
	  updateDisplay();
	  _delay_ms (200);
	  buttonCheck();
	}
	return(1);
	  
}

void updateDisplay() {
  oled.clear();                           //Clear data left in memory.
  
  //Top Row  
  oled.setCursor(36, 0);                  // Y can be 0, 1, 2, or 3. Usage: oled.setCursor(X px, Y (8*px).
  if(now.Hour()<10){oled.print(F("0"));}
  oled.print((now.Hour()));
  oled.print(F(" : "));
  if(now.Minute()<10){oled.print(F("0"));}
  oled.print(now.Minute());
  if(setting == 1){
    oled.setCursor(0, 0);
    oled.print(">");
  }
  if(setting == 2){
    oled.print("  <");
  }
  

  //Bottom Row
  if(alarmRinging){
    oled.setCursor(45, 2);
    oled.print("Wake!");
  }
  else{
    oled.setCursor(36, 2);
    if(alarmHour<10){oled.print(F("0"));}
    oled.print(alarmHour);
    oled.print(F(" : "));
    if(alarmMinute<10){oled.print(F("0"));} 
    oled.print(alarmMinute);
  }
  oled.switchFrame();
}

void buttonCheck(){
  if(!(PINA & BTN_UP_REG)){
    if(alarmRinging){alarmOff();}
    else if(!(PINB & BTN_DOWN_REG)){
      if(setting == 2){setting = 0;}
      else{setting++;}
      return;
    }
    else{
      alarmStopped = false; 
      switch(setting){
        case 0: incAlarmTime();
                break;
        case 1: changeRtcTime(true, 60);
                break;
        case 2: changeRtcTime(true, 1);
                break;
      }
    }
  }
  
  if(!(PINB & BTN_DOWN_REG)){
    if(alarmRinging){alarmOff();}
    else{
      alarmStopped = false; 
      switch(setting){
        case 0: decAlarmTime();
                break;
        case 1: changeRtcTime(false, 60);
                break;
        case 2: changeRtcTime(false, 1);
                break;
      }
    }
  }
}

void changeRtcTime(bool increase, int minutes){
  RtcDateTime modify = rtc.GetDateTime(); // get current date and time
  if(increase){modify += (60*minutes);} //In seconds
  else{modify -= (60*minutes);}
  rtc.SetDateTime(modify); // apply that modified instance to the RTC module
}

void incAlarmTime(){
  if(alarmMinute >= 45){
    alarmMinute = 0;
    if(alarmHour >= 23){alarmHour = 0;}
    else{alarmHour++;}
  }
  else{alarmMinute += 15;}
  EEPROM.write(eepromAlarmHourAddress, alarmHour);
  EEPROM.write(eepromAlarmMinuteAddress, alarmMinute);
}

void decAlarmTime(){
  if(alarmMinute == 15){
    alarmMinute = 0;
  }
  else if(alarmMinute == 0){
    alarmMinute = 45;
    if(alarmHour <= 0){alarmHour = 23;}
    else{alarmHour--;}
  }
  else{alarmMinute -= 15;}
  EEPROM.write(eepromAlarmHourAddress, alarmHour);
  EEPROM.write(eepromAlarmMinuteAddress, alarmMinute);
}

void alarmCheck(){
  byte currentTime = convertToMinutes(now.Hour(), now.Minute());
  byte alarmTime = convertToMinutes(alarmHour, alarmMinute);
  //Trigger alarm start
  if((currentTime == alarmTime) && (!alarmStopped)){
    alarmRinging = true;
    PORTA |= (1 << PIN_ALARM);     //High   
  }
  //Trigger alarm time out
  if(currentTime == (alarmTime+alarmDuration)){
    alarmOff();
    alarmStopped = false;  
  }
}

void alarmOff(){
  alarmRinging = false;
  PORTA &= ~(1 << PIN_ALARM);     //Low
  alarmStopped = true;  
}

int convertToMinutes(int hour, int minutes){
  int allMinutes = (hour*60)+minutes;
  return allMinutes;
}



