/*
 * display.h
 *
 *  Created on: 7 Mar 2024
 *      Author: Joonatan
 */

#ifndef MAIN_DISPLAY_H_
#define MAIN_DISPLAY_H_


#define MAX_BMP_LINE_LENGTH 320u
#define CONVERT_888RGB_TO_565BGR(b, g, r) ((r >> 3) | ((g >> 2) << 5) | ((b >> 3) << 11))
#define CONVERT_888RGB_TO_565RGB(r, g, b) (((r >> 3) << 3) | (g >> 5) | (((g >> 2) & 0x7u) << 13) | ((b >> 3) << 8))

void display_init(void);
void display_test_image(uint16_t *buf);


#endif /* MAIN_DISPLAY_H_ */
