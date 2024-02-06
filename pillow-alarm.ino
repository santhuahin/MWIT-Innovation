#include "BluetoothSerial.h"
#include <ESP32Time.h>
#include <EEPROM.h>

#define EEPROM_SIZE 255

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

BluetoothSerial SerialBT;

char current_time;
const byte numChars = 32;
char receivedChars[numChars];
boolean newData = false;
int dataNumber = 0;  

ESP32Time rtc(25200); //UTC+7

void setup() {
  Serial.begin(115200);
  EEPROM.begin(EEPROM_SIZE);
  rtc.setTime(1609459200);
  SerialBT.setTimeout(5000);
  SerialBT.begin("ESP32alarm"); //Bluetooth device name
  Serial.println("The device started, now you can pair it with bluetooth!");
}

void loop() {
  recvWithEndMarker();
  parseNumber();
  if (dataNumber == 100){
    setTime();
    
  }
  else if (dataNumber == 200){
    stopAlarm();
    
  }
  else if (dataNumber == 300){
    snoozeAlarm();
    
  }
  else if (dataNumber == 400){
    SerialBT.println(rtc.getDateTime());
    
  }
  else if (dataNumber == 500){
    setAlarm();
    
  }
  else if (dataNumber == 600){
    viewAlarms();
    
  }
  else{
    SerialBT.println("Error");
    
  }
  if ((rtc.getHour() == EEPROM.read(1)) && (rtc.getMinute() == EEPROM.read(2))){
    startVibration();
    
  }


} 

void recvWithEndMarker() {
    static byte ndx = 0;
    char endMarker = '\n';
    char rc;

    while (Serial.available() > 0) {
    Serial.read();
}
    
    if (SerialBT.available() > 0) {
        rc = SerialBT.read();

        if (rc != endMarker) {
            receivedChars[ndx] = rc;
            ndx++;
            if (ndx >= numChars) {
                ndx = numChars - 1;
            }
        }
        else {
            receivedChars[ndx] = '\0'; // terminate the string
            ndx = 0;
            newData = true;
        }
    }
}

void parseNumber() {
    if (newData == true) {
        dataNumber = 0;             // new for this version
        dataNumber = atoi(receivedChars);   // new for this version
        newData = false;
    }
}

void setTime() {
  SerialBT.println("Input current Day");
  int day = SerialBT.parseInt();
  SerialBT.println("Input current Month");
  int month = SerialBT.parseInt();
  SerialBT.println("Input current Year");
  int year = SerialBT.parseInt();
  SerialBT.println("Input current Hour (24h format)");
  int setHour = SerialBT.parseInt();
  SerialBT.println("Input current Minute");
  int setMin = SerialBT.parseInt();
  rtc.setTime(0, setMin, setHour, day, month, year);

}

void stopAlarm() {

}

void snoozeAlarm(){

}

void setAlarm(){
  SerialBT.println("Input Hour (24h format)");
  int hour = SerialBT.parseInt(); 
  EEPROM.write(1, hour);
  SerialBT.println("Input Minute ");
  int min = SerialBT.parseInt();
  EEPROM.write(2, min);
  SerialBT.println("Repeat Daily 1. Yes 2. NO");
  int repeat = SerialBT.parseInt();
  EEPROM.write(3, repeat);
}

void startVibration(){

}

void stopVibration(){

}

void viewAlarms(){

}