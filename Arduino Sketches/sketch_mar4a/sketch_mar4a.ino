#include <SPI.h>
#include <SD.h>
#include <Adafruit_GFX.h>    // Core graphics library
#include "Adafruit_ILI9341.h" // Hardware-specific library

#define LED_BUILTIN 2

#define SD_CS 4
#define TFT_DC 14
#define TFT_CS 12

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);

bool isInitComplete = false;

void setup() 
{
  File     bmpFile;
  
  // put your setup code here, to run once:
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);

  Serial.print("Initializing SD card...");
  if (!SD.begin(SD_CS)) 
  {
    Serial.println("failed!");
  }
  else
  {
    Serial.println("OK!");
  }
  //Lets try and open a BMP file.
  bmpFile = SD.open("/ghost.bmp", FILE_READ);
  
  if (bmpFile == NULL) 
  {
    Serial.println(F("File not found"));
  }
  else
  {
    Serial.println(F("File opened successfully."));
  }

  //Lets try messing with the display
  tft.begin();
  tft.fillScreen(ILI9341_BLUE);
  
  delay(1000);                      // wait for a second
  isInitComplete = true;
}


void loop() 
{
  if(!isInitComplete)
  {
      return;
  }

  // put your main code here, to run repeatedly:
  digitalWrite(LED_BUILTIN, HIGH);  // turn the LED on (HIGH is the voltage level)
  tft.fillScreen(ILI9341_RED);
  delay(1000);                      // wait for a second
  digitalWrite(LED_BUILTIN, LOW);   // turn the LED off by making the voltage LOW
  tft.fillScreen(ILI9341_BLUE);
  delay(1000);                      // wait for a second
  //Serial.println("blink");
}
