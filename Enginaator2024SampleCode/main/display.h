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

//To speed up transfers, every SPI transfer sends a bunch of lines. This define specifies how many. More means more memory use,
//but less overhead for setting up / finishing transfers. Make sure 240 is dividable by this.
#define PARALLEL_LINES 16
#define DISPLAY_MAX_TRANSFER_SIZE PARALLEL_LINES*320*2+8

void display_init(void);
void display_test_image(uint16_t *buf);


#endif /* MAIN_DISPLAY_H_ */
