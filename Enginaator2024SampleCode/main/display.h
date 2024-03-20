/*
 * display.h
 *
 *  Created on: 7 Mar 2024
 *      Author: Joonatan
 */

#ifndef MAIN_DISPLAY_H_
#define MAIN_DISPLAY_H_

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif


#define MAX_BMP_LINE_LENGTH 320u
#define CONVERT_888RGB_TO_565RGB(r, g, b) (((r >> 3) << 3) | (g >> 5) | (((g >> 2) & 0x7u) << 13) | ((b >> 3) << 8))

#define COLOR_BLACK    CONVERT_888RGB_TO_565RGB(0,  0,  0   )
#define COLOR_BLUE     CONVERT_888RGB_TO_565RGB(0,  0,  255 )
#define COLOR_RED      CONVERT_888RGB_TO_565RGB(255,0,  0   )
#define COLOR_GREEN    CONVERT_888RGB_TO_565RGB(0,  255,0   )
#define COLOR_CYAN     CONVERT_888RGB_TO_565RGB(0,  255,255 )
#define COLOR_MAGENTA  CONVERT_888RGB_TO_565RGB(255,  0,255 )
#define COLOR_YELLOW   CONVERT_888RGB_TO_565RGB(255,255,0   )
#define COLOR_WHITE    CONVERT_888RGB_TO_565RGB(255,255,255 )

#define DISPLAY_WIDTH 320u
#define DISPLAY_HEIGHT 240u

#define DISPLAY_MAX_TRANSFER_SIZE 40*320*2

void display_init(void);
void display_drawScreenBuffer(uint16_t *buf);
void display_fillRectangle(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color);
void display_drawBitmap(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t *bmp_buf);

#endif /* MAIN_DISPLAY_H_ */
