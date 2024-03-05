#include <SPI.h>
#include <SD.h>
#include <Adafruit_GFX.h>    // Core graphics library
#include "Adafruit_ILI9341.h" // Hardware-specific library

#define LED_BUILTIN 2

#define SD_CS 4
#define TFT_DC 14
#define TFT_CS 12

#define MAX_BMP_LINE_LENGTH 320u
#define CONVERT_888RGB_TO_565BGR(b, g, r) ((r >> 3) | ((g >> 2) << 5) | ((b >> 3) << 11))

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

void loadBmp(char * path, uint16_t * output_buffer);

/** Private variable declarations **/
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);
bool isInitComplete = false;
char str_buffer[128];

uint16_t test_buffer[40*40];
uint8_t  bmp_line_buffer[(MAX_BMP_LINE_LENGTH * 3) + 4u];
//static uint16_t frame_buffer[320*240]; //Cannot get this much contiguous memory.
static uint16_t frame_buffer1[320*120];
//static uint16_t frame_buffer2[320*120];

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

  tft.begin();
  tft.setRotation(1);

  tft.fillScreen(ILI9341_BLUE);

  loadBmp("/logo_upper.bmp", frame_buffer1);
  tft.drawRGBBitmap(0,0,frame_buffer1,320,120);

  loadBmp("/logo_lower.bmp", frame_buffer1);
  tft.drawRGBBitmap(0,120,frame_buffer1,320,120);

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
  delay(1000);                      // wait for a second
  digitalWrite(LED_BUILTIN, LOW);   // turn the LED off by making the voltage LOW
  delay(1000);                      // wait for a second
}

void loadBmp(char * path, uint16_t * output_buffer)
{
  File     bmpFile;
  BMPHeader header;
  uint16_t line_stride;
  uint16_t line_px_data_len;
  uint16_t * dest_ptr = output_buffer;

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

  /* Now lets try and read RGB data... */

  /* Take padding into account... */
  line_px_data_len = header.width_px * 3u;
  line_stride = (line_px_data_len + 3u) & ~0x03;

  for (int y = 0u; y < header.height_px; y++)
  {
    //file_res = f_lseek(&priv_f_obj, ((priv_bmp_header.height_px - (y + 1)) * line_stride) + priv_bmp_header.offset);
    //file_res = f_read(&priv_f_obj, priv_bmp_line_buffer, line_stride, &bytes_read);

    bmpFile.seek(((header.height_px - (y + 1)) * line_stride) + header.offset);
    bmpFile.readBytes((char *)&bmp_line_buffer, line_stride);

    for (int x = 0u; x < line_px_data_len; x+=3u )
    {
      *dest_ptr++ = CONVERT_888RGB_TO_565BGR(bmp_line_buffer[x+2], bmp_line_buffer[x+1], bmp_line_buffer[x]);
    }
  }
}
