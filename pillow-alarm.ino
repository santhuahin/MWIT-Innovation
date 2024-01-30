#include "BluetoothSerial.h"

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

BluetoothSerial SerialBT;

char current_time;

void setup() {
  Serial.begin(115200);
  SerialBT.begin("ESP32alarm"); //Bluetooth device name
  Serial.println("The device started, now you can pair it with bluetooth!");
}

void loop() {
   if (Serial.available()) {
    SerialBT.write(Serial.read());
    Serial.println("Choose Function: \n 1. Set Time \n 2. Stop alarm \n 3. Snooze alarm");
    int user_select = Serial.parseInt();
    Serial.print("Curent time is");
    Serial.println(current_time);
    if (user_select == 1) {
      set_time();
    }
    else if (user_select == 2) {
      stop_alarm();
    }
    else if (user_select == 3) {
      snooze_alarm();
    }
  }
  if (SerialBT.available()) {
    Serial.write(SerialBT.read());
    SerialBT.println("Choose Function: \n 1. Set Time \n 2. Stop alarm \n 3. Snooze alarm");
    int user_select = SerialBT.parseInt();
    SerialBT.print("Curent time is");
    SerialBT.println(current_time);
    if (user_select == 1) {
      set_time();
    }
    else if (user_select == 2) {
      stop_alarm();
    }
    else if (user_select == 3) {
      snooze_alarm();
    }
  }
  delay(20);
}

void set_time() {

}

void stop_alarm() {

}

void snooze_alarm(){

}
