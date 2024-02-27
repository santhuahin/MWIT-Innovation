/*
*******************************************************************************
  ESP8266/32 programmable WiFi alarm clock with web interface by ericBcreator
*******************************************************************************
  Main file - includes, setup and loop - last update 20210327 by ericBcreator
*******************************************************************************
  My setup:
  - Wemos D1 mini V3 / ESP32 mini (recommended)
    Using an ESP32 mini instead of the D1 (ESP8266) is recommended (because of the ESPUI issues) and a dropin replacement.
    Just add these 2 lines to the definitions
    #define RE_PINA       22  // note: D1 mini to ESP32 mini edit
    #define RE_PINB       21  // note: D1 mini to ESP32 mini edit  
	
  - LED matrix, rotary encoder, light sensor, motion sensor, speaker, neopixels, BME280
  
  - Arduino IDE 1.8.13
    - Flash settings (ESP8266): 4MB (FS: 2MB, OTA 1019KB)
    - lwIP Variant: v2 Lower Memory
*******************************************************************************
  Features:
  - supports ESP8266 and ESP32 boards
  - WiFi web interface setup. if no network is set or found, a web interface
    is started where the SSID and password can be set and saved to EEPROM
  - WiFi connected, syncs to a timeserver for accurate time
  - Web interface for programming the clock and changing settings,
    connect to the access point IP or via the lan IP
  - No Wifi mode
  - (OTA) over the air firmware update support
    read here how to set it up: https://github.com/esp8266/Arduino/blob/master/doc/ota_updates/readme.rst#arduino-ide
  - multiple alarms (default 12)
  - settings saved to EEPROM so they are not lost after a power failure
  - custom message and duration for each alarm
  - an action can be set when the alarm triggers: switching GPIO pins on/off
    so external devices can be controlled, neopixel control
  - recurring alarms: day of the week, week days, weekend days, custom
    setting for seconds, minutes, hours, days, months
  - 5 time display mode with and without seconds, colon, AM/PM
  - AM/PM mode
  - neopixel (wakeup light) feature
  - BME280 support
  - buzzer/speaker (number of beeps can be set) melodies in the works...
  - rotary encoder for switching on/off the alarm, setting default alarm 1,
    manual time and date, resync, display system info and changing
    other settings
  - announcement mode: turns the display off and display a message (predefined
    in code or custom set with the web interface) when the sensor detects motion
  - suspend mode: don't trigger any alarms but update recurring ones
  - light sensor support for adjusting the brightness of the LEDs  
  - sleep mode: display off after a delay
  - sleep times: set custom sleep times when the display will be off
  - motion sensor support: it will end the sleep mode if the sensor detects motion
  - visual feedback with leds:
    - upper left    suspend mode set
    - upper right   alarm 1 active
    - lower left    sleep mode set
    - lower right   sleeping active
  - bright / blinking alarm display
*******************************************************************************
  components:
  - Wemos D1 mini (or other ESP8266 or ESP32 board)
  - 4x8x8 MAX7219 LED matrix (optional 2 of them)
  - rotary encoder
  - buzzer or speaker
  - optional: light sensor
  - optional: motion sensor
  - optional: neopixel strip or ring
*******************************************************************************
  setup notes:
    - check the remarks in the code about the pins when using an ESP8266 
      (Wemos D1 mini) and also a BME280 sensor
    - check and set the @EB-setup settings to get the code running
    - for the web interface, use the SSID set by UI_ssid (note: the code
      adds the chip ID) and the password set by UI_password to connect to
      the network set by the ESP
    - @EB-custom settings can be set to your preference
    - light sensor setup:
      - 1 connection to 5V
      - other connection to analog input pin (default A0) plus a resistor
        (I used a 10K Ohm one with a 5K Ohm photo resistor) to ground. experiment
        with the values to get the best readings for your sensor. Enable DEBUG
        and DEBUG_LIGHTSENSOR to check the readings in the serial monitor
        and set ls_minSensor and ls_maxSensor to the min and max readings.
    - check the 'Notes on alarms' in the code for an explanation of the
      alarm settings
    - important setting to change or not all ESPUI elements will be updated:
      edit c:\Users\{username}\Documents\Arduino\libraries\ESPAsyncWebServer-master\src\AsyncWebSocket.h

      change line
        #define WS_MAX_QUEUED_MESSAGES 8 (or 32)
      into
        #define WS_MAX_QUEUED_MESSAGES 96
        
    - language: you can change the language by editing the messages in the 05_language_eng.h file (mind the correct quotation marks and occasional trailing spaces)
      you can also copy it to a new file (i.e. 05_language_dut.h) and change the include line (below in this file) into the new file name:

      #include "05_language_eng.h"              // @EB-setup language
	  
	  note: when making changes to language definitions and UI_USE_FILESYSTEM is defined, you need to reinitialize the filesystem with #define UI_INIT_FILESYSTEM

    - if you have connection (stability) problems:
	    - check your router WiFi channel. it may default to 6, try setting
	      it to another (0-13)
      - change the default web interface IP 192.168.4.1
	      try increasing the 4th (or 3rd) parameter.
*******************************************************************************
  This code is free for personal use, not for commercial purposes.
  Please leave this header intact.
  
  contact: ericBcreator@gmail.com
*******************************************************************************
*/

//
// type definitions - don't change
//

#define FN_ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))                     // Generic macro for obtaining number of elements of an array
typedef void (*callBackFunctionInt) (int value);                      // callback function definition for the rotary menu

// prototypes with optional parameters
void playTone(int freq, int duration, int postDelay = 0);             
void displayTime(bool updateDisplay = true);
void displayCharAltFont(byte font, int startColumn, unsigned char c, bool continuePrevColumn = false);
void displayChar(int x, unsigned int fontChar[], int lowHigh = HIGH, bool fill = false, bool reverse = false);
void displayAnimPacMan(bool reverse = false);
void displayDaylightInfo(bool displayDaylightInfo = true);
void UI_rereadTab(int tabCode = 0);

// 
// include the definition and setup files
//

#include "00_version_info.h"
#include "01_definitions_libraries.h"
#include "02_variables_presetup.h"
#include "03_announce.h"

#ifdef LANGUAGE_DUT
  #include "05_language_dut.h"              // Dutch language
#elif defined(LANGUAGE_DEU)
  #include "05_language_deu.h"              // German language
#elif defined(LANGUAGE_ITA)
  #include "05_language_ita.h"              // Italian language
#else
  #include "05_language_eng.h"              // English language (default)
#endif

// initialize the led matrix
Max72xxPanel matrix = Max72xxPanel(PIN_CS, LED_NUM_OF_HOR_DISPLAYS, LED_NUM_OF_VERT_DISPLAYS);

//
// setup
//

void setup() {
  matrixWidth = matrix.width();
  
  /// setup check defs
  #ifdef DEBUG
    Serial.begin(115200);
    delay(500); // @EB-todo arduino ide 1.8.13 bug? otherwise first debugprint fail

    DEBUGPRINTLN();
    DEBUGPRINTLN("Starting setup");
    DEBUGPRINTLN("--------------");
    DEBUGPRINTLN("Version " + (String) _VERSION);

    // sanitizing

    #ifndef ENABLE_PIR
      #ifdef ANNOUNCE_MODE
        #undef ANNOUNCE_MODE
        DEBUGPRINTLN("** PIR disabled -> announce mode disabled"); 
      #endif
    #endif

    DEBUGPRINTLN("--------------");

    #ifdef ESP32
      #ifdef DEBUG_ESP32_DISABLE_BROWNOUT
        DEBUGPRINTLN("** Brownout detection disabled");
        WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);      //disable brownout detector
      #endif
    #endif

    DEBUGPRINT("## Pins: PIN_CS:" + (String) PIN_CS + " PIN_BUZZER:" + (String) PIN_BUZZER + " RE_PINA:" + (String) RE_PINA);
    DEBUGPRINT(" RE_PINB:" + (String) RE_PINB + " RE_SWITCH:" + (String) RE_SWITCH + " PIN_LS:" + (String) PIN_LS);
    #ifdef ENABLE_PIR
      DEBUGPRINT(" PIN_PIR:" + (String) PIN_PIR);
    #endif
    #ifdef ENABLE_NEOPIXELS
      DEBUGPRINT(" PIN_NEOPIXELS:" + (String) PIN_NEOPIXELS);
    #endif
    #ifdef ENABLE_BME280
      DEBUGPRINT(" PIN_SCL:" + (String) PIN_SCL + " PIN_SDA:" + (String) PIN_SDA);
    #endif
    
    DEBUGPRINTLN();

  #endif

  pinMode(PIN_BUZZER, INPUT);             // make sure the buzzer is silent ;-)

  if (displayInfoInterval < 30)           // lower limit of 30 seconds
    displayInfoInterval = 30;

  #ifdef ESP32
    setFeatures = "ESP32";
    ledcSetup(0, 0, 8);                   // setup the buzzer for ESP32
  #else
    setFeatures = "ESP8266";
  #endif

  DEBUGPRINT("## " + setFeatures);
  DEBUGPRINT(" - " + (String) LED_NUM_OF_HOR_DISPLAYS + " horizontal and ");
  DEBUGPRINTLN((String) LED_NUM_OF_VERT_DISPLAYS + " vertical led matrix ");
  setFeatures += "-" + String(LED_NUM_OF_HOR_DISPLAYS) + " led matrix";

  #ifdef NO_WIFI
    DEBUGPRINTLN("!! Wifi disabled");
  #else
    DEBUGPRINTLN("## Wifi enabled");
  #endif

  #ifdef NO_BUZZER
    setFeatures += "-no speaker";
  #endif

  if (ls_minValue > ls_maxValue) {
    int tmpLS = ls_minValue;
    ls_minValue = ls_maxValue;
    ls_maxValue = tmpLS;
  }  
  if (ls_minValue < 0)        ls_minValue = 0;
  else if (ls_minValue > 15)  ls_minValue = 15;

  if (ls_maxValue < 0)        ls_maxValue = 0;
  else if (ls_maxValue > 15)  ls_maxValue = 15;

  #ifdef ENABLE_LIGHTSENSOR
    setFeatures += "-light sensor";
  
    pinMode(PIN_LS, INPUT);
    checkLightSensor();
    DEBUGPRINT("## Light sensor enabled");

    #ifdef LIGHTSENSOR_AVG
      DEBUGPRINT(", averaging values");
    #endif
    
    #ifdef LIGHTSENSOR_CURVE
      DEBUGPRINT(", slow rising curve");
    #endif

    ls_minMaxRange = ls_maxValue - ls_minValue;
    DEBUGPRINTLN(", min value " + (String) ls_minValue + " max value " + (String) ls_maxValue);
  #else
    DEBUGPRINTLN("!! Light sensor disabled");
  #endif

  #ifdef ENABLE_PIR
    setFeatures += "-PIR";
  
    pinMode (PIN_PIR, INPUT);
    digitalWrite (PIN_PIR, LOW);
    DEBUGPRINTLN("## Motion sensor enabled, defaults to " + (String) (PIR_DEFAULT ? "HIGH" : "LOW"));
  #else
    DEBUGPRINTLN("!! Motion sensor disabled");
  #endif

  #ifdef NO_ENCODER
    setFeatures += "-no rotary encoder";
    DEBUGPRINTLN("!! Rotary encoder disabled");
  #else
    DEBUGPRINT("## Rotary encoder enabled");

    #ifdef REVERSE_LR_ENCODER
      DEBUGPRINTLN(", left and right reversed");
    #else
      DEBUGPRINTLN("");
    #endif

    pinMode(RE_SWITCH, INPUT_PULLUP);
    pinMode(RE_PINA, INPUT_PULLUP);
    pinMode(RE_PINB, INPUT_PULLUP);  
  #endif

  #ifdef DAYLIGHTINFO
    setFeatures += "-daylight info";
    DEBUGPRINTLN("## Daylight info");
  #endif

  #ifdef ENABLE_NEOPIXELS
    setFeatures += "-neopixel";
    DEBUGPRINTLN("## Neopixel mode, " + (String) NUM_OF_NEOPIXELS + " pixels on pin " + (String) PIN_NEOPIXELS);
    FastLED.addLeds<NEOPIXEL, PIN_NEOPIXELS>(neopixels, NUM_OF_NEOPIXELS);              // GRB ordering is assumed
    delay(100);     // a little delay
    neopixelsOff();

    numOfNeoPalette = FN_ARRAY_SIZE(neoPalette) - 1;
  #endif

  #ifdef ENABLE_BME280
    setFeatures += "-BME";
    if (!BME280.begin(BME280_I2C_ADDRESS))
      DEBUGPRINTLN("** Error setting up BME280 connection");
    else
      readBME280();    
  #endif

  #ifdef ANNOUNCE_MODE
    setFeatures += "-announce";
  #endif

  #ifdef OTA_UPDATE
    setFeatures += "-OTA";
  #endif

  #ifdef UI_USE_FILESYSTEM
    setFeatures += "-UI fs";
  #endif

  /// setup - read eeprom

  setupMatrix();
  eepromRead();

  #ifdef DEBUG_RESET_WIFI
    UI_sw_resetEEPROM();
    DEBUGPRINTLN("WiFi settings reset");
  #endif

  #ifdef DEBUG_RESET_EEPROM
    eepromInitialize(0);
    eepromRead();
  #endif

  /// setup - intro  
  #ifdef ENABLE_NEOPIXELS
    neopixelsPresetOn(neoBacklight);
  #endif
  
  #ifndef DEBUG_FAST_START
    DEBUGPRINTLN("Display intro");  
    displayIntro("");
  #endif

  /// setup - check version
  int tmpVersionChecksum = getVersionChecksum();
  if (tmpVersionChecksum != versionChecksum) {
    DEBUGPRINTLN("New version " + (String) _VERSION + " checksum " + (String) tmpVersionChecksum);
    versionChecksum = tmpVersionChecksum;
    fileSystemInitialized = 'N';
    eepromChanged();
        
    #ifndef DEBUG_FAST_START
      String tmpMsg = msgNewVersion + (String) _VERSION;
      displayMessage(tmpMsg);
    #endif
  }

  /// setup - check and init the filesystem
  if (UI_enabled)
    UI_filesystem();

  /// setup - wifi & time
  
  #ifdef DEBUG_NO_WIFI
    time(&currentTime);
    timeData = *localtime(&currentTime);
  #else
    #ifdef NO_WIFI                        // no wifi so date and time have to be set manually
      timeManualSet = true;
      UI_enabled = false;                 // disable the ui

      timeData.tm_year = 120;             // preset date and time to 01-01-2020 8:00:00
      timeData.tm_mon = 0;
      timeData.tm_mday = 1;
      timeData.tm_hour = 8;
      timeData.tm_min = 0;
      timeData.tm_sec = 0;

      #ifdef NO_ENCODER                   // both wifi and the encoder disabled so no settings can be changed, display warning
        displayMessage(msgNoWifiNoRE);
//        setTime(timeData.tm_hour, timeData.tm_min, timeData.tm_sec, timeData.tm_mday, (timeData.tm_mon + 1), (timeData.tm_year + 1900));  // @EB-todo doesn't work??
        setTime(8, 0, 0, 1, 1, 2020);
        displayMessage("Time set by code");
      #else
        encoderSetDate();
        encoderSetTime();
      #endif      
    #else                                 // try wifi
      addChipidToUI_SSID();

      #ifdef ESP32                        // @EB-todo: eeprom gets erased when not saved before restart??
        eepromWrite();
      #endif

      if (WN_ssid_ca[0] != 0)             // if ssid is set, try to connect
        setupWifiConnection();
    
      if (UI_enabled) {
        UI_name += UI_ssid;
        UI_title = (char *) UI_name.c_str();        
        
        if (!wifiConnected) {             // wifi not connected so read the eeprom for ssid and password settings
          UI_sw_eepromset = UI_sw_readEEPROM();
    
          if (UI_sw_eepromset) {          // data read so try to connect
            DEBUGPRINT("SSID and password read from EEPROM. SSID: ");
            DEBUGPRINT(UI_sw_EEPROM.ssid);
            DEBUGPRINT(" password: ");
            DEBUGPRINT(UI_sw_EEPROM.password);
            DEBUGPRINTLN("");
    
            strcpy(WN_ssid_ca, UI_sw_EEPROM.ssid);
            strcpy(WN_password_ca, UI_sw_EEPROM.password);
            setupWifiConnection();
          } else {
            DEBUGPRINTLN("SSID and password not read from EEPROM");
          }
        }
    
        if (!wifiConnected) {             // wifi not connected so start the wifi setup web interface
          String tmpMsg = msgUIwifiSetup0  + (String) UI_ssid + msgUIwifiSetup1 + UI_apIP.toString();
          #ifdef DEBUG_FAST_START
            DEBUGPRINTLN(tmpMsg);
          #else
            displayMessage(tmpMsg);
          #endif
    
          matrix.drawChar(2,  yOffset, '<', HIGH, LOW, 1);
          matrix.drawChar(8,  yOffset, 'W', HIGH, LOW, 1);
          matrix.drawChar(14, yOffset, 'e', HIGH, LOW, 1);
          matrix.drawChar(20, yOffset, 'b', HIGH, LOW, 1);
          matrix.drawChar(25, yOffset, '>', HIGH, LOW, 1);
          matrix.write();
    
          if (!UI_sw_eepromset) {
            UI_sw_EEPROM.ssid[0] = 0;
            UI_sw_EEPROM.password[0] = 0;
          }
          UI_sw_setupWifi((char *) UI_title);
        }
      }
    
      #ifdef DEBUG_NO_TIMESERVER
        timeManualSet = true;
        setTime(8, 0, 0, 1, 1, 2020); // @EB DEBUG_NO_TIMESERVER
        currentTime = now();     
        timeData = *localtime(&currentTime);      

        DEBUGPRINTLN("DEBUG_NO_TIMESERVER, time set by code");
      #else
        getTimeFromServer();
      #endif
    #endif
  #endif

  /// setup - alarms

  #ifdef DEBUG_SET_TEST_ALARMS
    debugSetAlarms();
  #endif
  
  #ifdef DEBUG
    debugPrintAlarmlist();
  #endif

  for (int i = 0; i < maxAlarms; i++)
    alarmRecTriggered[i] = false;

  initActions();

  /// setup - OTA

  #ifdef OTA_UPDATE                       // if defined, enable OTA updates
    ArduinoOTA.setHostname(UI_ssid_ca);
    
    ArduinoOTA.onStart([]() {
      DEBUGPRINTLN("OTA - Starting update");
      endSleepMode(true);
      if (alarmTriggered)
        alarmEnded();

      playBuzzer(22);
      displayMessage(msgOTAupdateStart);

      #ifdef ENABLE_NEOPIXELS
        if (neoBacklight != 3)
          neopixelsPresetOn(3);           // set neopixels to green
        else
          neopixelsPresetOn(2);           // if backlight is green, set neopixels to red
      #endif

      for (int x = 0; x < matrixWidth; x++) {
        matrix.drawPixel(x, 2, HIGH);
        matrix.drawPixel(x, 5, HIGH);
      }
      matrix.drawPixel(0, 3, HIGH);
      matrix.drawPixel(0, 4, HIGH);      
      matrix.drawPixel((matrixWidth - 1), 3, HIGH);
      matrix.drawPixel((matrixWidth - 1), 4, HIGH);
      matrix.write();
    });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      int x, percFinished = progress / (total / 100);
      static int prevX = -1;
      static int maxX = matrix.width() - 2;
      
      #ifdef DEBUG
        DEBUGPRINTLN("OTA - Progress: " + fillSpace(percFinished, 3) + "%");
      #endif
      
      x = (maxX * percFinished / 100);

      if (x != prevX ) {
        matrix.drawPixel(x, 3, HIGH);
        matrix.drawPixel(x, 4, HIGH);
        matrix.write();
        prevX = x;
      }
    });

    ArduinoOTA.onError([](ota_error_t error) {
      DEBUGPRINTLN("OTA - Error" + (String) error);
      #ifdef ENABLE_NEOPIXELS
        neopixelsOff();
      #endif
      
      playBuzzer(24);
      displayMessage(msgOTAupdateError + (String) error);
    });
    
    ArduinoOTA.onEnd([]() {
      DEBUGPRINTLN("OTA - Update finished");
      #ifdef ENABLE_NEOPIXELS
        neopixelsOff();
      #endif

      playBuzzer(23);
      displayMessage(msgOTAupdateEnd);
    });

    ArduinoOTA.begin();
  #endif  

  /// setup - UI

  #ifdef DAYLIGHTINFO
    displayDaylightInfo(false);           // read the daylight info (without displaying a message) so it can be seen on the web interface
  #endif
  
  #ifndef NO_WIFI                         
    if (UI_enabled) {
      UI_setup();                         // setup web user interface
      UI_rereadTab(-1);                   // all switches are displaying true even if the variable is false so reread all controls
    }
  #endif

  if (!timeManualSet && !timeServerRead)  // time not read 1st time, try again
    getTimeFromServer();

  #ifdef ESP32                            // @EB-todo: eeprom gets erased when not saved before restart??
    eepromWrite();
  #endif

  alarmSuspended = false;                 // ready to start checking

  DEBUGPRINTLN("-------------");
  DEBUGPRINTLN("Starting loop");
  DEBUGPRINTLN("-------------");
  
}

//
// main loop
//

void loop() {
  static bool firstLoop = true;
  static bool prevSleepMode = sleepMode;  
  unsigned long startTime, startDisplayTime = 0;
  bool longPress = false;
  String announcementMsg;
  int userAction;

  if (firstLoop) {
    firstLoop = false;
    readCurrentTime();
    
    #ifndef DEBUG_FAST_START
      displayMessage(timeString);
    #endif

    if (alarmList[0].active)
      displayAlarmTime(msgAlarm0Active);

    displayInfoStartTime = millis();
    sleepStartTime = millis();
    BME280_startTime = millis();
  }

  matrix.fillScreen(LOW);

  if (alarmMessage) {
    readCurrentTime();
    checkAlarms();

  } else {
    if (displayInfoStartTime == 0)
      displayInfoStartTime = millis();
      
    startDisplayTime = 0;
    if (!sleeping)
      displayTime();

    while (((millis() - displayInfoStartTime) / 1000 < displayInfoInterval)) {

      #ifdef ENABLE_BME280
        if ((millis() - BME280_startTime) > BME280_updateDelay) {
          readBME280();
          BME280_startTime= millis();
        }
      #endif
      
      if ((millis() - startDisplayTime) > timeRefreshInterval || refreshTimeDisplay) {
        startDisplayTime = millis();

        if (sleeping) {
          displayPixelModes();
          matrix.write();
          readCurrentTime();
        } else {
          if (!alarmTriggered || (alarmTriggered && alarmList[alarmNum].fx != 1)) {
            displayTime();
          }
        }
      }

      checkAlarms();

      if (alarmTriggered && alarmMessage)       // break out of loop 
        break;

      #ifdef OTA_UPDATE
        if ((millis() - OTA_startTime) > OTA_interval) {
          ArduinoOTA.handle();
          OTA_startTime = millis();
        }
      #endif

      delayAndCheckEnc(mainLoopDelay);          // @EB-todo

      if (announceMode)                         // break out of loop 
        break;

      if (sleepMode != prevSleepMode) {
        prevSleepMode = sleepMode;
        if (sleepMode) {
          sleepStartTime = millis();
        } else {
          displayInfoStartTime = millis();
          displayPixelModes();
          matrix.write();
        }
      }

      if (forceStartSleepMode) {
        forceStartSleepMode = false;
        startSleepMode();

      } else if (forceEndSleepMode) {
        forceEndSleepMode = false;
        endSleepMode(true);
        
      } else if (sleepMode) {
        if (!sleeping) {
          if ((millis() - sleepStartTime) / 1000 > sleepDelay)
            startSleepMode();
        #ifdef SLEEP_HOURS
          else if (timeData.tm_hour == SLEEP_HOUR_ON && timeData.tm_min == SLEEP_MIN_ON)
            #ifdef SLEEP_HOURS_DISABLE_WIFI
              WiFi.mode(WIFI_OFF);
              timeManualSet = true;
            #endif
            DEBUGPRINT("Sleeping hours active, time to go to sleep");
            sleepHoursActive = true;
            startSleepMode();
        #endif          
        }         
      #ifdef SLEEP_HOURS
        else if (timeData.tm_hour == SLEEP_HOUR_OFF && timeData.tm_min == SLEEP_MIN_OFF) {
          DEBUGPRINT("Sleeping hours active, time to wake up");
          sleepHoursActive = false;
          endSleepMode(true);

          #ifdef SLEEP_HOURS_DISABLE_WIFI
            setupWifiConnection();
            #ifndef DEBUG_NO_TIMESERVER
              getTimeFromServer();
            #endif 
          #endif
        }
      #endif
        }

      if (eepromValChanged) {
        if ((millis() - eepromSavedTime) / 1000 > eepromSaveDelay) {
          DEBUGPRINT("Autosaving - ");
          eepromWrite();
        }
      }
    } // loop before display date / daylightinfo
  }

  if (announceMode) {                             // announce mode will display the selected message while detecting motion. end by long pressing the rotary encoder    
    DEBUGPRINTLN("Starting announce mode");    
    alarmSuspended = true;
    matrix.fillScreen(LOW);
    matrix.write();
    playBuzzer(20);

    String tmpMsg;
    if (customAnnounceMsg)
      tmpMsg = customAnnouncement;
    else {
      tmpMsg = announcements[announceItem];
      if (UI_enabled) {                           // @EB-todo
        UI_rereadTab(-1);
        UI_dnsServer.processNextRequest();
      }
    }

    readCurrentTime();
    announcementTime = timeData;
    displayMessage((String) msgMenuAnnouncement1 + " : " + tmpMsg);
    announcementMsg = fillZero(getPMhour()) + ":" + fillZero(timeData.tm_min) + " * " + tmpMsg + " *";
      
    DEBUGPRINTLN("Announcement : " + announcementMsg);
    longPress = false;
    displayAnnounceMode(0);

    startTime = millis();                         // @EB-todo delay before checking for motion. note: the web interface can cancel announcemode!
    while (((millis() - startTime) / 1000) < announceStartDelay) {
      userAction = checkAndHandleEvents();
      if (!announceMode) {
        longPress = true;
        break;
      }
      delay(250);
    }

    #ifdef ENABLE_PIR
      digitalWrite(PIN_PIR, PIR_DEFAULT);
    #endif

    while (!longPress) {
      displayAnnounceMode(1);
      
      while (1) {
        userAction = checkAndHandleEvents();
        if (userAction || !announceMode) break;
        delay(250);
      }

      if (!announceMode || userAction == 12) {      // announceMode cancelled by web interface or long press
        longPress = true;
        break;
      }
      
      for (int i = 0; i < announceRepeat; i++) {    // display the announcement several times (set by announceRepeat)
        playBuzzer(31);
        displayMessage(announcementMsg);
        if (reSwitch || !announceMode ) break;        
      }

      if (!announceMode || reSwitch == 2) {         // announceMode cancelled by web interface or long press
        longPress = true;
      }

      #ifdef NO_ENCODER                             // @EB-todo if there is no encoder, display the message only once and exit the loop
        longPress = true;                           //          otherwise keep looping until there is a long press
      #endif

      if (!longPress) {                             // wait and restart the announcement loop or break the loop after long press
        displayAnnounceMode(1);

        startTime = millis();        
        while (((millis() - startTime) / 1000 < announceWaitDelay) && !longPress) {
          userAction = checkAndHandleEvents();

          if (userAction == 12) {
            longPress = true;
            break;            
          }
          delay(250);
        }
      }
    }

    DEBUGPRINTLN("Exiting announce mode");
    playBuzzer(20);
    announceMode = false;
    alarmSuspended = false;
    refreshTimeDisplay = true;
  	endSleepMode(true);
    displayEndAnnounceMode();

    #ifndef NO_ENCODER
      while (digitalRead(RE_SWITCH) == LOW) { }     // wait until encoder switched is released
    #endif

    if (UI_enabled) {                               // @EB-todo
      UI_rereadTab(-1);
      UI_dnsServer.processNextRequest();
    }
    
  } else if (!sleeping && displayInfo && !alarmTriggered && displayInfoStartTime > 0 && ((millis() - displayInfoStartTime) / 1000 >= displayInfoInterval)) {
    #ifndef DEBUG_FAST_START
      bool items[] = {true,false,false};      
      
      #ifdef DAYLIGHTINFO
        if (displayDaylight) items[1] = true;
      #endif
      #ifdef ENABLE_BME280
        if (BME280_display)  items[2] = true;
      #endif

      while (items[itemToDisplay] == false) {
        itemToDisplay++;
        itemToDisplay = itemToDisplay % 3;
      }

      DEBUGPRINT("Info items to display:");
      for (int i = 0; i < 3; i++) {
        if (items[i]) {
          DEBUGPRINT(" " + (String) i);
        }
      }
      DEBUGPRINTLN(", selected item: " + (String) itemToDisplay);

      switch (itemToDisplay) {
        case 0: displayMessage(timeString); break;        // display long date and time
        #ifdef DAYLIGHTINFO
          case 1: displayDaylightInfo();    break;        // display daylight info
        #endif
        #ifdef ENABLE_BME280
          case 2: displayBME280();          break;        // display BME280 readings
        #endif
      }

      itemToDisplay++;
      itemToDisplay = itemToDisplay % 3;
    #endif

    refreshTimeDisplay = true;
  }
  
  displayInfoStartTime = millis();
    
} // loop
