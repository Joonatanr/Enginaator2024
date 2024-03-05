#include <SPI.h>
#include <SD.h>
#include <Adafruit_GFX.h>    // Core graphics library
#include "Adafruit_ILI9341.h" // Hardware-specific library

#define LED_BUILTIN 2

#define SD_CS 4
#define TFT_DC 14
#define TFT_CS 12

/** Private type definitions **/

#pragma pack(push)  // save the original data alignment
#pragma pack(1)     // Set data alignment to 1 byte boundary
typedef struct {
    uint16_t type;              // Magic identifier: 0x4d42
    uint32_t size;              // File size in bytes
    uint16_t reserved1;         // Not used
    uint16_t reserved2;         // Not used
    uint32_t offset;            // Offset to image data in bytes from beginning of file
    uint32_t dib_header_size;   // DIB Header size in bytes
    int32_t  width_px;          // Width of the image
    int32_t  height_px;         // Height of image
    uint16_t num_planes;        // Number of color planes
    uint16_t bits_per_pixel;    // Bits per pixel
    uint32_t compression;       // Compression type
    uint32_t image_size_bytes;  // Image size in bytes
    int32_t  x_resolution_ppm;  // Pixels per meter
    int32_t  y_resolution_ppm;  // Pixels per meter
    uint32_t num_colors;        // Number of colors
    uint32_t important_colors;  // Important colors
} BMPHeader;
#pragma pack(pop)  // restore the previous pack setting

/** Private function forward declarations **/

void loadBmp(char * path);

/** Private variable declarations **/
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);
bool isInitComplete = false;
char str_buffer[128];

/** Public functions **/
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

  loadBmp("/ghost.bmp");

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

void loadBmp(char * path)
{
  File     bmpFile;
  BMPHeader header;

  bmpFile = SD.open(path, FILE_READ);

  if (bmpFile == NULL) 
  {
    Serial.println(F("File not found"));
  }
  else
  {
    Serial.println(F("File opened successfully."));
  }

  bmpFile.readBytes((char *)&header, sizeof(BMPHeader));

  sprintf(str_buffer, "Bitmap width : %d", header.width_px);
  Serial.println(str_buffer);
  sprintf(str_buffer, "Bitmap height : %d", header.height_px);
  Serial.println(str_buffer);
}
