/**
 * @file LunchboxLogger_v1.ino
 * @author Nigel Hungerford-Symes, Adventures in Silicon Pty Ltd
 * @brief First version of Lunchbox logger with only Pico W internal flash
 * @date 2023-02-09
 * 
 * @copyright Copyright (c) 2023
 * 
 * contribution Bryan Siepert for Adafruit Industries
 * code liberally uses adafruit libraries, please go and support their great products.
  
 * This code is not warranted to be fit for any purpose. You may only use it at your own risk.
 * This generated code may be freely used for both private and commercial use
 * provided this copyright is maintained.
 */


//Required libraries
// Adafruit TMP117
// RV-3028-C7 by Macro Yau
// CAP-1203 by Sparkfun
// Adafruit SSD1306
// SafeString by Matthew Ford

/* TODO
1) Set and read time from RV3028

2) Merge in code to run SSD1306 OLED display (128x64)

3) Log temperature

4) Fix starting routine to report the error but not to block start up if a device cannot be found
*/

/*DONE
9/2/23 - Created a menu option to update time via serial or default to leaving it as the current RTC stored time.

*/


/*
Program flow

- Setup Serial, TWI, SSD1306, RV3028
- Ask if user wants to update the RTC



*/



#include <Wire.h>             //For i2c (piicodev)
#include <Adafruit_TMP117.h>  //Temperature Sensor
#include <Adafruit_Sensor.h>
#include <Adafruit_GFX.h>      // for SSD1306 screen
#include <Adafruit_SSD1306.h>  //for OLED screen
#include <RV3028C7.h>          //RTC
#include <millisDelay.h>       //for non-blocking delays

//Pre-complier Defines (constants)
#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 64  // OLED display height, in pixels
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C 
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

//Constants
//const unsigned long SERIAL_DELAY_TIME = 5000; //5 second timeout on default option

//Global objects
Adafruit_TMP117 tmp117;    //create temperature sensor object
RV3028C7 rtc;              //create RTC object
millisDelay MaxInputTime;  //to allow no input default options after period

void setup(void) {
  //Set up Serial
  Serial.begin(115200);
  while (!Serial) delay(10);  // will pause Zero, Leonardo, etc until serial console opens
  delay(3000);                //time for serial monitor to catch up

  Serial.println("********************************************");
  Serial.println("* Lunchbox Logger version 1 (February 2023) *");
  Serial.println("********************************************");

  //Set up TWI for piicodev
  Wire.setSDA(8);
  Wire.setSCL(9);
  Wire.begin();

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  display.display();
  // Clear the buffer
  display.clearDisplay();
  display.setTextSize(1);   //small but info dense for startup (21 char per line)
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.cp437(true);
  //             ********************* 
  //display.write("Program started...");
  display.println("Logger start.");
  //display.setCursor(0,8);
  //             ********************* 
  display.println("SSD1306 OK.");
  display.display();

  
  //Initialize TMP117
  if (!tmp117.begin()) {
    Serial.println("** Failed to find TMP117 chip");
    display.println("ERR - TMP117 not found");
    display.display();
    while (1) { delay(10); }
  }
  Serial.println("Successfully initialised TMP117");
  display.println("TMP117 OK.");
  display.display();
    

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
    

  // Should I have a time-out and default to a date?
  //Timeout loop
  Serial.println("Do you want to update the time [y/N]? (8 seconds to select)");
  display.print("RTC update");
  display.display();
    
  MaxInputTime.start(8000);  //8 seconds default time
  bool UserInputKnown = false;
  bool UserWantsToSetTime = false;
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
      UserInputKnown = true;  //exits loop
    }

    if ((c == 'N') || (c == 'n')) {
      UserWantsToSetTime = false;
      UserInputKnown = true;  //exits loop
    }

    if (MaxInputTime.justFinished()) {
      Serial.println("Default option selected");
      UserWantsToSetTime = false;
      UserInputKnown = true;  //exits loop
    }

    if ((LastRemainingTime - MaxInputTime.remaining()) >= 1000) {
      Serial.print(".");
      display.print(".");
      display.display();
      LastRemainingTime = MaxInputTime.remaining();
    }
  }

  if (UserWantsToSetTime) {
    Serial.print("Enter current date and time in ISO 8061 format (e.g. 2018-01-01T08:00:00): ");
    while (Serial.available() == false)
      ;
    if (Serial.available() > 0) {
      String dateTime = Serial.readString();  //pull from serial line string
      Serial.println(dateTime);
      rtc.setDateTimeFromISO8601(dateTime);
      rtc.synchronize();  //write the provided time to the rtc
    }

    //ISO 8061 does not include day of the week
    Serial.print("Enter day of the week (0 for Sunday, 1 for Monday...");
    while (Serial.available() == false)
      ;
    if (Serial.available() > 0) {
      int DayOfWeek = Serial.parseInt();
      Serial.println(DayOfWeek);
      rtc.setDateTimeComponent(DATETIME_DAY_OF_WEEK, DayOfWeek);
      rtc.synchronize();
    }

    Serial.println("");  //make space
  }

  // Improvement: Pull time from NTP service with Pico W? (try then prompt?) See https://github.com/raspberrypi/pico-examples/blob/master/pico_w/ntp_client/picow_ntp_client.c
}

void loop() {
  sensors_event_t temp;    // create an empty event to be filled
  tmp117.getEvent(&temp);  //fill the empty event object with the current measurements
  Serial.print(rtc.getCurrentDateTime());
  Serial.print("   Temperature:  ");
  Serial.print(temp.temperature);
  Serial.println(" degrees C");
  //Serial.println("");

  //routine to show temp
  display.clearDisplay();
  display.setCursor(0,0);
  display.setTextSize(2);
  display.print("Temp:");
  display.println(temp.temperature);
  display.display();


  delay(2000);
}
