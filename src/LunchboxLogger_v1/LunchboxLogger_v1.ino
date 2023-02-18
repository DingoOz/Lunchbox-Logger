/**
 * @file LunchboxLogger_v1.ino
 * @author Nigel Hungerford-Symes, Adventures in Silicon Pty Ltd
 * @brief First version of Lunchbox logger with only Pico W internal flash
 * @date 2023-02-09
 * 
 * @copyright Copyright (c) 2023
 * 
 * 
 * code liberally uses adafruit libraries (including contribution 
 * by Bryan Siepert for Adafruit Industries), please go and support their great products!
 * SD card code attributed to David A. Mellis and Tom Igoe.
  
 * This code is not warranted to be fit for any purpose. You may only use it at your own risk.
 * This generated code may be freely used for both private and commercial use
 * provided this copyright is maintained.
 */


//Required libraries
// Adafruit TMP117
// RV-3028-C7 by Macro Yau
// CAP-1203 by Sparkfun  - remove?
// Adafruit SSD1306
// SafeString by Matthew Ford

/* TODO
1) Set and read time from RV3028
2) Done
3) Done

4) Fix starting routine to report the error but not to block start up if a device cannot be found

5) Use files on the SD card to set up time or wifi network

6) Set up user input via a webbrowser and not only via UART

7) Add logic for when there is no Internet - include not trying NTP if no Internet connection. See https://arduino-pico.readthedocs.io/en/latest/wifintp.html#

8) Physical enclosure

9) format the log string for easier data analysis
*/

/*DONE
9/2/23 - Created a menu option to update time via serial or default to leaving it as the current RTC stored time.
14/2/23 - Writting data to the SD Card
*/


/*
Program flow

- Setup Serial, TWI, SSD1306, RV3028, SD Card file
- [Look for SD Card file with time data, update RTC if necessary]
- [Look for SD Card file with Wifi Network data, update Wifi & RTC if found]
- initialise the WIFI
- poll NTP servers, save time, applying TZ if sucessful
- If NTP failed, ask if user wants to update the RTC (with 8 second time out)

-- Display temp
-- log time temp


*/

/*
SD card attached to SPI bus as follows:
   ** MISO      - GPIO 12
   ** CS        - GPIO 13
   ** SCK/CLK   - GPIO 14
   ** MOSI      - GPIO 15
*/


#include <Wire.h>             //For i2c (piicodev)
#include <Adafruit_TMP117.h>  //Temperature Sensor
#include <Adafruit_Sensor.h>
#include <Adafruit_GFX.h>      // for SSD1306 screen
#include <Adafruit_SSD1306.h>  //for OLED screen
#include <RV3028C7.h>          //RTC
#include <millisDelay.h>       //for non-blocking delays

#include <SPI.h>
#include <SD.h>  //SD Card functions

//NTP clock sync
#include <WiFi.h>
#include <time.h>
#include <sys/time.h>

#include "LunchboxLogger.h"



//Pre-complier Defines (constants)
#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 64  // OLED display height, in pixels
#define OLED_RESET -1     // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#ifndef STASSID
//#define STASSID "YOUR_SSID"
//#define STAPSK "YOUR_WIFI_PASSWORD"
#define STASSID ""
#define STAPSK ""
#endif

//Global objects
Adafruit_TMP117 tmp117;    //create temperature sensor object
RV3028C7 rtc;              //create RTC object
millisDelay MaxInputTime;  //to allow no input default options after period
const char* ssid = STASSID;
const char* password = STAPSK;
WiFiMulti multi;
bool NTPTimeSetWasSuccessful = false;

//Pulls the NTP UTC time and prints the local timezone time to serial
int setClock() {
  setenv("TZ", "AEST-10", 1);  //Brisbane AU
  tzset();

  NTP.begin("pool.ntp.org", "time.nist.gov");

  LLPrintln(&display, "Waiting for NTP");

  time_t now = time(nullptr);  //this stores calendar time (UTC)
  while (now < 8 * 3600 * 2) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println("");
  struct tm timeinfo;  //this is for storing localtime
  struct tm* local = localtime(&now);

  Serial.print(asctime(local));

  //Set RV3028
  //rtc.setDateTime(uint16_t year, uint8_t month, uint8_t dayOfMonth, DayOfWeek_t dayOfWeek, uint8_t hour, uint8_t minute)
  rtc.setDateTime(local->tm_year + 1900,
                  local->tm_mon + 1,
                  local->tm_mday,
                  local->tm_wday,
                  local->tm_hour,
                  local->tm_min);
  rtc.synchronize();  //write the provided time to the rtc

  return 0;  //success
}

void setup(void) {
  //Set up Serial
  Serial.begin(115200);
  while (!Serial) delay(10);  // will pause Zero, Leonardo, etc until serial console opens
  delay(3000);                //time for serial monitor to catch up

  LLPrintHeader();

  //Set up TWI for piicodev
  Wire.setSDA(8);
  Wire.setSCL(9);
  Wire.begin();

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;  // Don't proceed, loop forever
  }

  display.display();
  // Clear the buffer
  display.clearDisplay();
  display.setTextSize(1);  //small but info dense for startup (21 char per line)
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.cp437(true);
  LLPrintln(&display, "Logger started.");
  display.println("SSD1306 OK.");
  display.display();

  //Initialize TMP117
  if (!tmp117.begin()) {
    LLPrintln(&display, "ERR-TMR117 not found");
    while (1) { delay(10); }  // loop forever
  }
  LLPrintln(&display, "TMR117 OK.");

  //Set up for RTC RV3028C7
  while (rtc.begin() == false) {
    Serial.print("*** Failed to detect RV-3028-C7");
    display.println("ERR - RV3028C7 not found");
    display.display();
    delay(5000);
  }
  Serial.println("Successfully initialised RV3028C7");
  display.println("RTC OK.");
  display.display();

  //Initialise WIFI
  Serial.print("Connecting to ");
  Serial.println(ssid);
  multi.addAP(ssid, password);
  //connect to WIFI OR REBOOT  - WILL NEED TO CHANGE THIS IN FUTURE!*!*!*!*!*!*!*!*!*!
  if (multi.run() != WL_CONNECTED) {
    LLPrintln(&display, "WiFi ERROR");
    Serial.println("Unable to connect to network 10 seconds...");
    delay(10000);  //Wait 10 seconds so message can be seen
    //rp2040.reboot();
  }
  Serial.println("");
  LLPrintln(&display, "WiFi OK");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  Serial.println("Trying to set RTC from NTP...");
  if (setClock() == 0) {
    NTPTimeSetWasSuccessful = true;
  } else {
    NTPTimeSetWasSuccessful = false;
  }

  //Initialise SD card
  if (!SD.begin(13, SPI1))  //TODO: Pull out pin to a global variable
  {
    Serial.println("Card failed, or not present");
    display.println("ERR - No SD Card");
    display.display();
  }
  //Serial.println("SD Card initialized.");
  display.println("SD Card OK.");
  display.display();

  //Timeout loop for user input if the NTP sync failed
  if (!NTPTimeSetWasSuccessful) {
    LLPrintln(&display, "User RTC update?");
    Serial.print("Do you want to update the time [y/N]? (8s timeout):");

    MaxInputTime.start(8000);  //8 seconds default time
    bool UserInputKnown = false;
    bool UserWantsToSetTime = false;  //Does the user want manually set the time via UART?
    long LastRemainingTime = MaxInputTime.remaining();
    while (!UserInputKnown) {
      char c = 0;
      if (Serial.available()) {
        c = Serial.read();
        while (Serial.available()) {
          Serial.read();  //clear the rest of any input
        }
      }

      if ((c == 'Y') || (c == 'y')) {
        UserWantsToSetTime = true;
        Serial.println("Yes");
        UserInputKnown = true;  //exits loop
      }

      if ((c == 'N') || (c == 'n')) {
        UserWantsToSetTime = false;
        Serial.println("No");
        UserInputKnown = true;  //exits loop
      }

      if (MaxInputTime.justFinished()) {
        Serial.println("No (default)");
        UserWantsToSetTime = false;
        UserInputKnown = true;  //exits loop
      }

      if ((LastRemainingTime - MaxInputTime.remaining()) >= 1000) {
        LLPrint(&display, ".");
        //Serial.print(".");
        //display.print(".");
        //display.display();
        LastRemainingTime = MaxInputTime.remaining();
      }
    }

    if (UserWantsToSetTime) {
      Serial.println("Enter current date and time in ISO 8061 format (e.g. 2018-01-01T08:00:00): ");
      while (Serial.available() == false)
        ;
      if (Serial.available() > 0) {
        String dateTime = Serial.readString();  //pull from serial line string
        Serial.println(dateTime);
        rtc.setDateTimeFromISO8601(dateTime);
        rtc.synchronize();  //write the provided time to the rtc

        //ISO 8061 does not include day of the week
        Serial.println("Enter day of the week (0 for Sunday, 1 for Monday):");
        while (Serial.available() == false)
          ;
        if (Serial.available() > 0) {
          int DayOfWeek = Serial.parseInt();
          Serial.println(DayOfWeek);
          rtc.setDateTimeComponent(DATETIME_DAY_OF_WEEK, DayOfWeek);
          rtc.synchronize();
        }
      }
    }
    Serial.println("");  //make space
  }
 
}

void loop() {
  sensors_event_t temp;    // create an empty event to be filled
  tmp117.getEvent(&temp);  //fill the empty event object with the current measurements
  
  //routine to show temp
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  //show time
  //extract h:m:s
  //String timestring = rtc.getCurrentDateTime();
  display.println(rtc.getCurrentDateTime());
  display.println("");
  display.setTextSize(2);
  display.print("Temp:");
  display.println(temp.temperature);
  display.display();

  //DEBUG open 'datalog.txt' file and add temperature to it.
  File dataFile = SD.open("datalog.csv", FILE_WRITE);

  if (dataFile) {
    //Serial.println("datalog.txt opened, writing the following:");
    dataFile.print(rtc.getCurrentDateTime());
    dataFile.print(" , ");
    dataFile.println(temp.temperature);
    //dataFile.println(" degrees C");

    Serial.print(rtc.getCurrentDateTime());
    Serial.print(" , ");
    Serial.println(temp.temperature);
    //Serial.println(" degrees C");

    //Close file
    dataFile.close();
  }

  delay(2500);
  //Serial.println("");
  delay(2500);
}
