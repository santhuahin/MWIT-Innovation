/*
***************************************************************************
  ebc_alarmclock - misc functions
***************************************************************************
  last update 20210327 by ericBcreator
***************************************************************************
*/

uint16_t getVersionChecksum() {
  unsigned long checkSum = 0;
  
  #ifdef _VERSION
    String ebcVersion = (String) _VERSION;
    if (ebcVersion.length() != 6)
      DEBUGPRINTLN("Version check: length mismatch");
    else {
      byte checksumChar, counter = 0;
      
      for (int i = 0; i < 6; i++) {
        if (i != 2) {
          checksumChar = ebcVersion[i];
          if (checksumChar == 32)             // convert spaces to 0
            checksumChar = 48;

          //checkSum += (checksumChar - 48) * pow(10, (3 - counter));
          checksumChar = (57 - checksumChar);
          checkSum += checksumChar * pow(10, (4 - counter));
          counter++;
        }
      }

      checkSum %= 65535;
    }
  #endif

  return (uint16_t) checkSum;
}

int delayAndHandle(int delayTime) {
  static unsigned long startTimeHandleEvents = millis();  
  unsigned long startDelayTime = millis();
  int eventResult;
  
  while (millis() - startDelayTime < delayTime) {
    if (millis() - startTimeHandleEvents > handleEventsDelay) { 
      startTimeHandleEvents = millis();    
      
      eventResult = handleEvents();
      if (eventResult)
        return eventResult;        
    }
    
    #ifndef NO_WIFI
      if (UI_enabled) {
        if ((millis() - startUIupdateTime) >= UI_updateDelay) {             // reread the settings after x milliseconds      
          UI_rereadTop();
          startUIupdateTime = millis();
        }
        UI_dnsServer.processNextRequest();
      }
    #endif
    delay(1);         // @EB-todo: needed?
  }

  return 0;
}

int checkAndHandleEvents() {
  readCurrentTime();
  checkAlarms();
  return handleEvents();
}

// returns: 0 no user action, -1 rotary left, 1 rotary right, 11 rotary switch, 12 rotary switch long, 21 PIR motion
int handleEvents() {
  #ifdef ENABLE_PIR
    if (digitalRead(PIN_PIR) != PIR_DEFAULT) {
      digitalWrite(PIN_PIR, PIR_DEFAULT);
      return 21;
    }
  #endif
  
  #ifndef NO_ENCODER
    reEnc = readRotEnc();

    if (reEnc < 0)
      return -1;
    else if (reEnc > 1)
      return 1;

    reSwitch = readRotEncSwitch();

    if (reSwitch == 1)
      return 11;
    else if (reSwitch == 2)
      return 12;
  #endif

  #ifdef OTA_UPDATE                           // @EB-todo
    if ((millis() - OTA_startTime) > OTA_interval) {
      ArduinoOTA.handle();
      OTA_startTime = millis();
    }
  #endif
  
  if (UI_enabled) {                           // @EB-todo
    if ((millis() - startUIupdateTime) >= UI_updateDelay) {             // reread the settings after x milliseconds      
      UI_rereadTop();
      startUIupdateTime = millis();
    }
    UI_dnsServer.processNextRequest();
  }

  return 0;
}

int delayAndCheckEnc(unsigned int delayTime) {
  unsigned long startTime = millis();
  unsigned long startDelayTime;  
  int returnValue = 0;

  if (announceMode) {                         // don't check in announce mode, it will interfere with delayHandleEvents()
    startDelayTime = millis();
    while (millis() - startDelayTime < delayTime) { }
    return returnValue;
  }

  #ifndef NO_WIFI                             // only resync when wifi and timeserver are enabled
    #ifndef DEBUG_NO_TIMESERVER
      if (UI_timeToResync) {
        getTimeFromServer();
        matrix.fillScreen(0);
        UI_timeToResync = false;
      }
    #endif
  #endif
  
  while ((millis() - startTime) < delayTime) {
    if (alarmTriggered)
      checkRecurSkipped();                    // @EB-todo needed?????

    #ifndef NO_WIFI
      if (UI_enabled) {                                                     // handle the UI if the webinterface is enabled
        if ((millis() - startUIupdateTime) >= UI_updateDelay) {             // reread the settings after x milliseconds
//          UI_rereadTab(); // @EB-todo reread
          UI_rereadTop();
          startUIupdateTime = millis();
        }
        UI_dnsServer.processNextRequest();
      }
    #endif

    #ifdef ENABLE_PIR
      if (prevPIRstate == true) {
        if (millis() - PIRstartTime > (PIRdelay * 1000)) {                // wait before checking the PIR to prevent retriggering
          prevPIRstate = false;
        }
      } else {
        if (digitalRead(PIN_PIR) != PIR_DEFAULT) {
          returnValue = 21;
          digitalWrite(PIN_PIR, PIR_DEFAULT);
          prevPIRstate = true;
          PIRstartTime = millis();
          #ifdef DEBUG_PIR
            DEBUGPRINTLN("Motion detected at " + timeString);
          #endif

          if (sleeping)
            endSleepMode(true);
          else      
            endSleepMode(false);
        }
      }
    #endif

    #ifdef ENABLE_LIGHTSENSOR
      checkLightSensor();
    #endif

    #ifndef NO_ENCODER
      reSwitch = readRotEncSwitch();
      reEnc = readRotEnc();
      
      if (reSwitch || reEnc) {
        DEBUGPRINT("Rotary encoder used: ");

        if (reEnc < 0)          returnValue = -1;
        else if (reEnc > 1)     returnValue =  1;
        if (reSwitch == 1)      returnValue = 11;
        else if (reSwitch == 2) returnValue = 12;

        DEBUGPRINTLN(returnValue);

        if (sleeping) {
          endSleepMode(true);
        } else {
          if (alarmTriggered || forceFirstAlarm) {
            if (forceFirstAlarm)
              forceFirstAlarm = false;
            alarmInterrupted();
          } else {
            if (reSwitch == 1) {
              alarmList[0].active = !alarmList[0].active;
              alarmRecTriggered[0] = false;
              eepromChanged();
  
              if (UI_enabled)
                UI_rereadTab(-1);
  
              displayPixelModes();
              matrix.write();
              
              if (alarmList[0].active)
                displayAlarmTime(msgAlarm0Active); 
              else
                displayMessage(msgAlarm0Inactive);
              return 0;
            } else if (reSwitch == 2) {
              //playBuzzer(20);   @EB-todo
              encoderMenu();
            }
          }
        }
      }
    #endif // NO_ENCODER

//    delay(delayAndCheckEncDelay);             // @EB-todo: strange bug: the delay function causes problems when called from the UI webinterface 
                                                //           (the playTone function after saving) so replaced with empty while loop
    
    startDelayTime = millis();
    while (millis() - startDelayTime < delayAndCheckEncDelay) { 
      #ifndef NO_WIFI
        if (UI_enabled)
          UI_dnsServer.processNextRequest();
      #endif
    }

    /*
	nope: don't return early, it will mess up playtone delays...
	if (returnValue)
      return returnValue;                       // event occurred so break the loop and return the value
	*/
  }

  return 0;
}

#ifdef ENABLE_LIGHTSENSOR
  void checkLightSensor() {
    static bool firstCall = true;
    static unsigned long lsStartTime = 0;
    static int prevReadValue = 0;
    int readValue;
    int rangedValue;
    int mappedValue;

    if (ledBrightnessManualSet)                                             // don't use the light sensor when the brightness is set manually
      return;
  
    if (sleeping)
      return;
    
    if (lsStartTime != 0 && ((millis() - lsStartTime) < ls_delay)) {        // delay time before checking the light sensor
      return;
    }

    lsStartTime = millis();
    readValue = analogRead(PIN_LS);

    #ifdef LIGHTSENSOR_AVG                                                  // if defined, average with previous reading
      if (prevReadValue) {
        rangedValue = (readValue + prevReadValue) / 2;
      } else {
        rangedValue = readValue;
      }
    #else
      rangedValue = readValue;
    #endif

    prevReadValue = readValue;

    if (rangedValue < ls_minSensor)                                         // set out of range values to min or max values
      rangedValue = ls_minSensor;
    if (rangedValue > ls_maxSensor)
      rangedValue = ls_maxSensor;
    
    mappedValue = map(rangedValue, ls_minSensor, ls_maxSensor, ls_minValue, ls_maxValue);

    #ifdef LIGHTSENSOR_CURVE                                                // if defined, use slow curve
      mappedValue = int((ls_minMaxRange + .5 - (cos(3.14 / 2 / ls_minMaxRange * mappedValue) * ls_minMaxRange))) + ls_minValue;
    #endif

    #ifdef LIGHTSENSOR_OFFSET
      mappedValue += LIGHTSENSOR_OFFSET;
      mappedValue = constrain(mappedValue, ls_minValue, ls_maxValue);
    #endif

    #ifdef DEBUG_LIGHTSENSOR
      #ifdef LIGHTSENSOR_AVG
        DEBUGPRINT("Brightness read value " + (String) readValue + ", averaged, set in range " + (String) rangedValue + ", mapped to " + (String) mappedValue);
      #else
        DEBUGPRINT("Brightness read value " + (String) readValue + ", set in range " + (String) rangedValue + ", mapped to " + (String) mappedValue);
      #endif
    #endif

    if (firstCall) {
      ledBrightness = mappedValue;
      matrix.setIntensity(ledBrightness);
      #ifdef DEBUG_LIGHTSENSOR
        DEBUGPRINTLN(", set to " + (String) ledBrightness);  
      #endif
      firstCall = false;
      
    } else if (abs(mappedValue - ledBrightness) >= ls_triggerStep) {
      if (mappedValue < ledBrightness)
        ledBrightness--;      
      else
        ledBrightness++;
      matrix.setIntensity(ledBrightness);
      
      #ifdef DEBUG_LIGHTSENSOR
        DEBUGPRINTLN(", set to " + (String) ledBrightness);
      #endif

      if (UI_enabled) {
        UI_rereadLS();
      }
      
    } else {
      #ifdef DEBUG_LIGHTSENSOR
        DEBUGPRINTLN();
      #endif
    }
  }
#endif

void startSleepMode() {
  #ifdef NOPACMAN
    displayMessage(UI_msg_sleepMode);
    matrix.fillScreen(LOW);
  #else
    displayAnimPacMan();
  #endif
  
  sleeping = true;
  DEBUGPRINTLN("Sleep mode started at " + timeString);
  sleepStartedTime = millis();
  displayPixelModes();
  matrix.write();
}
  
void endSleepMode(bool resetInfoTime) {
  if (sleeping) {
    readCurrentTime();
    DEBUGPRINTLN("Sleep mode ended at " + timeString);
    bool prevDST = DST;
    checkDST();
    refreshTimeDisplay = true;
    refreshTempSwapTime = 1;
    sleeping = false;

    #ifndef NO_WIFI
      #ifndef DEBUG_NO_TIMESERVER
        if (DST != prevDST)     // reread the time if the DST changed while sleeping
          getTimeFromServer();
      #endif
    #endif

    #ifndef NOPACMAN
      displayAnimPacMan(true);
    #endif    
  }
  
  sleepStartTime = millis();

  if (resetInfoTime)
    displayInfoStartTime = millis();
}

void setupMatrix() {
  int i = LED_NUM_OF_HOR_DISPLAYS * LED_NUM_OF_VERT_DISPLAYS - 1;
  
  matrix.setIntensity(ledBrightness);
  if (i >= 0) matrix.setRotation(0, LED_MATRIX_ROTATION_0); // set the rotation as defined for each matrix
  if (i >= 1) matrix.setRotation(1, LED_MATRIX_ROTATION_1);
  if (i >= 2) matrix.setRotation(2, LED_MATRIX_ROTATION_2);
  if (i >= 3) matrix.setRotation(3, LED_MATRIX_ROTATION_3);
  if (i >= 4) matrix.setRotation(4, LED_MATRIX_ROTATION_4);
  if (i >= 5) matrix.setRotation(5, LED_MATRIX_ROTATION_5);
  if (i >= 6) matrix.setRotation(6, LED_MATRIX_ROTATION_6);
  if (i >= 7) matrix.setRotation(7, LED_MATRIX_ROTATION_7);
  matrix.fillScreen(LOW);
}

//
// time functions
//

void readCurrentTime() {
  // note: when timeManualSet is true, the time is read from the internal ESP clock, when false from the NTP server
  //       timeManualSet is set true when the time is set with the rotary encoder and false when the time is read with getTimeFromServer()
  
  if (timeManualSet) {
    currentTime = now();
  } else {
    time(&currentTime);     // @EB-todo: connects to timeserver with every call ???
  }
  
  timeData = *localtime(&currentTime);
  
//  timeString = String(ctime(&currentTime));
//  timeString.trim();

  int year = timeData.tm_year;
  if (year < 1900) year += 1900;
    
  timeString = days[timeData.tm_wday] + " "+ fillZero(timeData.tm_mday) + " " + months[timeData.tm_mon]  + " " + (String) year + " ";
  timeString += fillZero(getPMhour()) + ":" + fillZero(timeData.tm_min) + ":" + fillZero(timeData.tm_sec);

  if (ampmMode) {
    if (pm)  timeString += " PM";
    else     timeString += " AM";
  }
}

int getPMhour() {
  int pmHour = timeData.tm_hour;
  pm = false;
  if (pmHour > 11) pm = true;

  if (ampmMode) {         // displaying 12:00-12:59 and 1:00-11:59 AM or PM
    if (pmHour == 0)      pmHour += 12;
    else if (pmHour > 12) pmHour -= 12;    
  }

  return pmHour;
}

bool checkLeapYear (int the_year) {
  if (the_year % 4 != 0) return false;
  if (the_year % 100 == 0 && the_year %400 != 0) return false;
  if (the_year % 400 == 0) return true;
  return true;
}

byte getDaysInMonth (int year, int month) { 
  byte daysInMonth = 31;  
  switch (month) {
    case 2:
      if (checkLeapYear(year))
        daysInMonth = 29;
      else
        daysInMonth = 28;
      break;
    case 4:
    case 6:
    case 9:
    case 11:
      daysInMonth = 30;
      break;
  }
  return daysInMonth;
}

//
// checkDST
//

void checkDST() {
  if (DSTmode == 0) { DST = false; return; }  // DST mode set to off so return false
  if (DSTmode == 1) { DST = true;  return; }  // DST mode set to on so return true

  DST = false;                                // defaults to false

  if (timeManualSet)
    currentTime = now();
  else
    time(&currentTime);
  
  timeData = *localtime(&currentTime);

  int y = timeData.tm_year - 100;             // tm_year returns the year after 1900 so subtract 100 to get the year after 2000
  int x1 = (y + y / 4 - 2) % 7;               // identifies Sunday offset for March
  int x2 = (y + y / 4 + 2) % 7;               // identifies Sunday offset for October
  
  int mon = timeData.tm_mon + 1;              // important: the tm_mon variable ranges from 0 to 11 so add 1 to get 1 to 12 !!

  // DST: begins last Sunday of March 02:00
  if ((mon == 3) && (timeData.tm_mday == (31 - x1)) && (timeData.tm_hour >= 2))
    DST = true;
  if (((mon == 3) && (timeData.tm_mday > (31 - x1))) || (mon  > 3))
    DST = true;

  // DST: ends last Sunday of Oct 02:00
  if ((mon == 10) && (timeData.tm_mday == (31 - x2)) && (timeData.tm_hour >= 2))
    DST = false;
  if (((mon == 10) && (timeData.tm_mday > (31 - x2))) || (mon > 10) || (mon < 3))
    DST = false;
}

//
// div
//

String fillZero(int value) {
  int tmpValue;
  
  if (value < 0)
    return String(value);

  tmpValue = constrain(value, 0, 99);
    
  if (tmpValue < 10)
    return '0' + String(tmpValue);
  else
    return String(tmpValue);
}

String fillSpace(int value, int valLength) {
  String tmpStr = String(value);
  while (tmpStr.length() < valLength)
    tmpStr = " " + tmpStr;
  return tmpStr;
}

//
// buzzer
//

#ifdef NO_BUZZER
  void playBuzzer(int numOfBuzz) {                      // no buzzer so just delay numOfBuzz * 100 ms
    if (numOfBuzz < 11) {
      for (int i = 0; i < numOfBuzz; i++) {
        delayAndCheckEnc(100);
      }
    } else
      delayAndCheckEnc(100);
  }
#else
  void playBuzzer(int numOfBuzz) {
    #ifdef ESP32                                        // ESP32
      ledcSetup(0, 0, 8);
      ledcAttachPin(PIN_BUZZER, 0);
    #endif
    
    if (numOfBuzz < 11) {
      for (int i = 0; i < numOfBuzz; i++) {
        playTone(2000, 50, 50);
      }
    } else {
      switch (numOfBuzz) {
        case 11:                                  // big ben ECDG(-1) G(-1)DEC
          playTone(659, 500); playTone(523, 500); playTone(587, 500); playTone(392, 1000, 500);
          playTone(392, 500); playTone(587, 500); playTone(659, 500); playTone(523, 1000, 500);
          break;

        case 12:                                  // Avicii
          //127 BPM = 1/8 Note: 236.22, 1/16 Note: 118.11, 1/32 Note: 59.055, 1/64 Note: 29.528, 1/128: 14.764 
          //sustain: 1/16+1/32+1/64+1/128 = 221ms //release: 1/128 = 15ms 
          
          playTone(1480, 221, 15); playTone(1319, 221, 15); playTone(1319, 221, (15 + 236)); 
          playTone(1319, 221, 15); playTone(1319, 221, 15); playTone(1319, 221, 15); 
          playTone(1319, 221, 15); playTone(1245, 221, 15); playTone(1245, 221, 15); 
          playTone(1319, 221, 15); playTone(1319, 221, (15 + 236));
          playTone(2217, 221, 15); playTone(1976, 221, 15); playTone(1661, 221, 15);
          playTone(1480, 221, 15); playTone(1319, 221, 15); playTone(1319, 221, (15 + 236));
          playTone(1319, 221, 15); playTone(1319, 221, 15); playTone(1319, 221, 15);
          playTone(1319, 221, 15); playTone(1109, 221, 15); playTone(1109, 221, 15);
          playTone(988, 221, 15);  playTone(988, 221, (15 + 236));
          playTone(2217, 221, 15); playTone(1976, 221, 15); playTone(1661, 221, 15);
          break;
          
        case 20: playTone(2250, 50); break;                                           // UI button clicked
        case 21: for (int i = 0; i < 2; i++) { playTone(2250,  50,  50); }   break;   // save
        case 22: for (int i = 0; i < 2; i++) { playTone(2500,  50,  50); }   break;   // firmware update start
        case 23: for (int i = 0; i < 3; i++) { playTone(2500,  50,  50); }   break;   // firmware update finish
        case 24: for (int i = 0; i < 2; i++) { playTone(1000, 100, 100); }   break;   // firmware update error
        case 31: for (int i = 0; i < 5; i++) { playTone(1750,  50,  50); }   break;   // announcement
      }
    }

    #ifdef ESP32
      ledcDetachPin(PIN_BUZZER);
    #endif
    pinMode(PIN_BUZZER, INPUT);                   // make sure the buzzer is silent ;-)
  }
#endif

void playTone(int freq, int duration, int postDelay) {
  unsigned int startDelayTime;
  
  if (freq > 0) {
    #ifdef ESP32
      ledcWriteTone(0, freq);
    #else
      tone(PIN_BUZZER, freq);
    #endif
  } 

//  startDelayTime = millis();
//  while ((millis() - startDelayTime) < duration) { };
  delayAndCheckEnc(duration);
  
  #ifdef ESP32
    ledcWriteTone(0, 0);
    ledcWrite(0, LOW);
  #else
    noTone(PIN_BUZZER);
  #endif  

  if (postDelay) {
//    startDelayTime = millis();
//    while ((millis() - startDelayTime) < postDelay) { };
    delayAndCheckEnc(postDelay);
  }
}

void testBuzzer() {  
  for (int i = 100; i < 10000; i += 100) {
    DEBUGPRINTLN(i);
    playTone(true, i, 500);
  }
  playTone(false, 0, 0);
}

//
// BME280
//

#ifdef ENABLE_BME280
  void readBME280() {
    BME280_temperature = BME280.readTemperature();                        // get temperature in degree Celsius
    
    if (BME280_fahrenheid) {                                              // convert to Fahrenheid if necessary
      BME280_temperature = (BME280_temperature * 9 / 5 + 32);
    }
      
    BME280_humidity = BME280.readHumidity();                              // get humidity in rH%
    BME280_pressure = BME280.readPressure() / 100;                        // get pressure in Pa   

    #ifdef BME280_SEA_LEVEL_PRESSURE
      BME280_altitude = BME280.readAltitude(BME280_SEA_LEVEL_PRESSURE);   // get altitude in mtr. BME280_SEA_LEVEL_PRESSURE has to be set correctly to get a accurate reading    
    #endif

    BME280_temperature *= BME280_temperatureCalFactor;
    BME280_temperature += BME280_temperatureCalValue;
    BME280_humidity    *= BME280_humidityCalFactor;
    BME280_humidity    += BME280_humidityCalValue;
    BME280_pressure    *= BME280_pressureCalFactor;
    BME280_pressure    += BME280_pressureCalValue;
  }
#endif
