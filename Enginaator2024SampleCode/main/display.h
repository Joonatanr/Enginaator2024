/*
 * display.h
 *
 *  Created on: 7 Mar 2024
 *      Author: Joonatan
 */

#ifndef MAIN_DISPLAY_H_
#define MAIN_DISPLAY_H_


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

#define PARALLEL_LINES 16

#define DISPLAY_MAX_TRANSFER_SIZE PARALLEL_LINES*320*2+8
//#define DISPLAY_MAX_TRANSFER_SIZE 320u * 240u * sizeof(uint16_t)

extern uint16_t priv_frame_buffer[320*240];

void display_init(void);
void display_test_image(uint16_t *buf);
void display_fillRectangle(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color);


#endif /* MAIN_DISPLAY_H_ */
