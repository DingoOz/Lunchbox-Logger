/*
 * LunchboxLogger_v1.h
 * 
 * Helper functions for LunchboxLogger.
 */

#include <Adafruit_GFX.h>       // for SSD1306 screen
#include <Adafruit_SSD1306.h>  

//
void LLPrintHeader()
{
  Serial.println("********************************************");
  Serial.println("* Lunchbox Logger version 1 (February 2023)*");
  Serial.println("*                                          *");
  Serial.println("*     -- Ensuring safer lunchboxes --      *");
  Serial.println("*                                          *");
  Serial.println("********************************************");

}

//Sends the text to the UART and the SSD1306 at the same time.
int LLPrint(Adafruit_SSD1306 *display, char *text)
{
  Serial.print(text);
  display->setTextColor(SSD1306_WHITE);
  display->cp437(true);
  display->print(text);
  display->display();

  return 1;
}

int LLPrintln(Adafruit_SSD1306 *display, char *text)
{
  Serial.println(text);
  display->setTextColor(SSD1306_WHITE);
  display->cp437(true);
  display->println(text);
  display->display();

  return 1;
}