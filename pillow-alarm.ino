#include "BluetoothSerial.h"
#include <ESP32Time.h>
#include <EEPROM.h>

#define EEPROM_SIZE 255

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

BluetoothSerial SerialBT;

int motor1 = 22;
int motor2 = 23;

const byte numChars = 32;
char receivedChars[numChars];
boolean newData = false;
int dataNumber = 0;  
long timeSinceSnooze;
boolean snooze = false;

ESP32Time rtc(25200); //UTC+7

void setup() {
  Serial.begin(115200);

  EEPROM.begin(EEPROM_SIZE);

  rtc.setTime(1609459200);

  pinMode(motor1, OUTPUT);
  pinMode(motor2, OUTPUT);

  SerialBT.setTimeout(5000);
  SerialBT.begin("ESP32alarm"); //Bluetooth device name
  Serial.println("The device started, now you can pair it with bluetooth!");
}

void loop() {
  recvWithEndMarker();
  parseNumber();
  while (SerialBT.available > 0){
  else if (dataNumber == 100){
    setTime();
    break;
  }
  else if (dataNumber == 200){
    stopAlarm();
    break;
  }
  else if (dataNumber == 300){
    snoozeAlarm();
    break;
  }
  else if (dataNumber == 400){
    SerialBT.println(rtc.getDateTime());
    break;
  }
  else if (dataNumber == 500){
    setAlarm();
    break;
  }
  else if (dataNumber == 600){
    viewAlarm();
    break;
  }
  else if (dataNumber == 700){
    clearAlarm();
    break;
  }
  if (dataNumber == 999){
    for (int i = 0; i<3; i++){
      digitalWrite(motor1,HIGH);
      delay(3000);
      digitalWrite(motor1,LOW);
      delay(3000);
      digitalWrite(motor2,HIGH);
      delay(3000);
      digitalWrite(motor2,LOW);
      delay(3000);
    }
  }
  }
  if ((rtc.getHour() == EEPROM.read(1)) && (rtc.getMinute() == EEPROM.read(2))){
    startVibration();
  
  }
  if ((snooze == true) && (timeSinceSnooze == (EEPROM.read(3))*60000)){
    startVibration();
  }



} 

void recvWithEndMarker() {
    static byte ndx = 0;
    char endMarker = '\n';
    char rc;

    while (SerialBT.available() > 0) {
    SerialBT.read();
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
  serialFlush();

}

void stopAlarm() {
  stopVibration();
  timeSinceSnooze = 0;
  snooze = false;
  serialFlush();
}

void snoozeAlarm(){
  stopVibration();
  timeSinceSnooze = millis();
  snooze = true;
  serialFlush();
}

void setAlarm(){
  SerialBT.println("Input Hour (24h format)");
  int hour = SerialBT.parseInt(); 
  EEPROM.write(1, hour);
  SerialBT.println("Input Minute ");
  int min = SerialBT.parseInt();
  EEPROM.write(2, min);
  SerialBT.println("Input Snooze duration (0 for no snooze) ");
  int snoozeDuration = SerialBT.parseInt();
  EEPROM.write(3, snoozeDuration);
  serialFlush();
}

void startVibration(){
  digitalWrite(motor1, HIGH);
  digitalWrite(motor2, HIGH);
  timeSinceSnooze = 0;
  serialFlush();
}

void stopVibration(){
  digitalWrite(motor1, LOW);
  digitalWrite(motor2, LOW);
  timeSinceSnooze = 0;
  serialFlush();
}

void viewAlarm(){
  SerialBT.print("Alarm set");
  SerialBT.print(EEPROM.read(1));
  SerialBT.print(":");
  SerialBT.println(EEPROM.read(2));
  SerialBT.print("Snooze interval");
  SerialBT.print(EEPROM.read(3));
  SerialBT.println("min");
  serialFlush();

}

void clearAlarm(){
  EEPROM.write(1, 0);
  EEPROM.write(2, 0);
  EEPROM.write(3, 0);
  SerialBT.println("Alarm Cleared");
  serialFlush();
}

void serialFlush(){
  while(SerialBT.available() > 0) {
    char t = SerialBT.read();
  }
}