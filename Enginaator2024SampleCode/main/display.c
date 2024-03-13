/*
 * display.c
 *
 *  Created on: 7 Mar 2024
 *      Author: Joonatan
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"

#include "display.h"

#define LCD_HOST    SPI2_HOST

#define PIN_NUM_DC         5
#define PIN_NUM_RST        3
#define PIN_NUM_CS         4
#define PIN_NUM_BCKL       2


/* Private type definitions */
typedef struct
{
    uint8_t cmd;
    uint8_t data[16];
    uint8_t databytes; //No of data in data; bit 7 = delay after set; 0xFF = end of cmds.
} lcd_init_cmd_t;


/* Forward declarations */
static void lcd_spi_pre_transfer_callback(spi_transaction_t *t);
static void lcd_cmd(spi_device_handle_t spi, const uint8_t cmd, bool keep_cs_active);
static void lcd_data(spi_device_handle_t spi, const uint8_t *data, int len);
static void lcd_init(spi_device_handle_t spi);
static void send_lines(spi_device_handle_t spi, int xPos, int yPos, int width, int height, uint16_t *linedata);
static void wait_line_finish(spi_device_handle_t spi);



DRAM_ATTR static const lcd_init_cmd_t ili_init_cmds[]=
{
    /* Power contorl B, power control = 0, DC_ENA = 1 */
    {0xCF, {0x00, 0x83, 0X30}, 3},
    /* Power on sequence control,
     * cp1 keeps 1 frame, 1st frame enable
     * vcl = 0, ddvdh=3, vgh=1, vgl=2
     * DDVDH_ENH=1
     */
    {0xED, {0x64, 0x03, 0X12, 0X81}, 4},
    /* Driver timing control A,
     * non-overlap=default +1
     * EQ=default - 1, CR=default
     * pre-charge=default - 1
     */
    {0xE8, {0x85, 0x01, 0x79}, 3},
    /* Power control A, Vcore=1.6V, DDVDH=5.6V */
    {0xCB, {0x39, 0x2C, 0x00, 0x34, 0x02}, 5},
    /* Pump ratio control, DDVDH=2xVCl */
    {0xF7, {0x20}, 1},
    /* Driver timing control, all=0 unit */
    {0xEA, {0x00, 0x00}, 2},
    /* Power control 1, GVDD=4.75V */
    {0xC0, {0x26}, 1},
    /* Power control 2, DDVDH=VCl*2, VGH=VCl*7, VGL=-VCl*3 */
    {0xC1, {0x11}, 1},
    /* VCOM control 1, VCOMH=4.025V, VCOML=-0.950V */
    {0xC5, {0x35, 0x3E}, 2},
    /* VCOM control 2, VCOMH=VMH-2, VCOML=VML-2 */
    {0xC7, {0xBE}, 1},
    /* Memory access contorl, MX=MY=0, MV=1, ML=0, BGR=1, MH=0 */
    {0x36, {0x28}, 1},
	//{0x36, {0xF8}, 1},
	/* Pixel format, 16bits/pixel for RGB/MCU interface */
    {0x3A, {0x55}, 1},
    /* Frame rate control, f=fosc, 70Hz fps */
    {0xB1, {0x00, 0x1B}, 2},
    /* Enable 3G, disabled */
    {0xF2, {0x08}, 1},
    /* Gamma set, curve 1 */
    {0x26, {0x01}, 1},
    /* Positive gamma correction */
    {0xE0, {0x1F, 0x1A, 0x18, 0x0A, 0x0F, 0x06, 0x45, 0X87, 0x32, 0x0A, 0x07, 0x02, 0x07, 0x05, 0x00}, 15},
    /* Negative gamma correction */
    {0XE1, {0x00, 0x25, 0x27, 0x05, 0x10, 0x09, 0x3A, 0x78, 0x4D, 0x05, 0x18, 0x0D, 0x38, 0x3A, 0x1F}, 15},
    /* Column address set, SC=0, EC=0xEF */
    {0x2A, {0x00, 0x00, 0x00, 0xEF}, 4},
    /* Page address set, SP=0, EP=0x013F */
    {0x2B, {0x00, 0x00, 0x01, 0x3f}, 4},
    /* Memory write */
    {0x2C, {0}, 0},
    /* Entry mode set, Low vol detect disabled, normal display */
    {0xB7, {0x07}, 1},
    /* Display function control */
    {0xB6, {0x0A, 0x82, 0x27, 0x00}, 4},
    /* Sleep out */
    {0x11, {0}, 0x80},
    /* Display on */
    {0x29, {0}, 0x80},
    {0, {0}, 0xff},
};

static spi_device_handle_t priv_spi_handle;

/* Public function definitions */
void display_init(void)
{
    esp_err_t ret;

    spi_device_interface_config_t devcfg=
    {
        .clock_speed_hz=10*1000*1000,           //Clock out at 10 MHz
        .mode=0,                                //SPI mode 0
        .spics_io_num=PIN_NUM_CS,               //CS pin
        .queue_size=7,                          //We want to be able to queue 7 transactions at a time
        .pre_cb=lcd_spi_pre_transfer_callback,  //Specify pre-transfer callback to handle D/C line
    };

    printf("Initializing SPI bus... \n");

    //Attach the LCD to the SPI bus that was initialized in main.c
    ret=spi_bus_add_device(LCD_HOST, &devcfg, &priv_spi_handle);
    ESP_ERROR_CHECK(ret);

    printf("Initializing LCD display... \n");

    //Initialize the LCD
    lcd_init(priv_spi_handle);
    printf("Transmitting test data... \n");

    /* Create some test data for 16 lines. */
    uint16_t *line_data = heap_caps_malloc(320*PARALLEL_LINES*sizeof(uint16_t), MALLOC_CAP_DMA);
    assert(line_data != NULL);

    /*
    for(int x = 0; (x < 320*240);x++)
    {
    	//line_data[x] = CONVERT_888RGB_TO_565RGB(0xFFu, 0xFFu, 0x00u);
    	priv_frame_buffer[x] = CONVERT_888RGB_TO_565RGB(0xFFu, 0xFFu, 0x00u);
    }
    */

    for(int x = 0; (x < 320*PARALLEL_LINES);x++)
    {
    	line_data[x] = CONVERT_888RGB_TO_565RGB(0xFFu, 0xFFu, 0x00u);
    }

    //Display some test data.
    for (int y=0; y<240; y+=PARALLEL_LINES)
    {
    	send_lines(priv_spi_handle, 0, y, 320, PARALLEL_LINES, line_data);
    	wait_line_finish(priv_spi_handle);
    }
}

void display_test_image(uint16_t *buf)
{
    uint16_t * buf_ptr = buf;

	//Display some test data.
    for (int y=0; y < 240; y += PARALLEL_LINES)
    {
    	send_lines(priv_spi_handle, 0, y, 320, PARALLEL_LINES, buf_ptr);
    	wait_line_finish(priv_spi_handle);

    	buf_ptr += (PARALLEL_LINES * 320);
    }
}

/* Lets try a blocking implementation here... */
void display_fillRectangle(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color)
{
	uint8_t data[16];

	uint16_t end_column = (x + width) - 1u;
    uint16_t end_row = (y + height) - 1u;

#if 0
    esp_err_t ret;
    spi_transaction_t t;

    memset(&t, 0, sizeof(t));       //Zero out the transaction
    t.length = 16;                 	//Len is in bytes, transaction length is in bits.
    t.tx_data[0] = color >> 8u;     //Data
    t.tx_data[1] = color & 0xffu;
    t.user=(void*)1;                //D/C needs to be set to 1

    /* Column Address Set */
    lcd_cmd(priv_spi_handle, 0x2A, false);
    data[0] = x >> 8; 				/* Start Col High 	*/
    data[1] = x & 0xffu;			/* Start Col Low 	*/
    data[2] = end_column >> 8;		/* End Col High	 	*/
    data[3] = end_column & 0xffu;	/* End Col Low 		*/
    lcd_data(priv_spi_handle, data, 4u);

    /* Page Address Set */
    lcd_cmd(priv_spi_handle, 0x2B, false);
    data[0] = y >> 8; 			/* Start page High 	*/
    data[1] = y & 0xffu;		/* Start page Low 	*/
    data[2] = end_row >> 8;		/* End Row High	 	*/
    data[3] = end_row & 0xffu;	/* End Row Low 		*/
    lcd_data(priv_spi_handle, data, 4u);

    lcd_cmd(priv_spi_handle, 0x2C, false);

    /* Now we send the actual data. */
    for (int x = 0; x < width; x++)
    {
    	for (int y = 0; y < height; y++)
    	{
    		ret = spi_device_polling_transmit(priv_spi_handle, &t);  //Transmit!
    	}
    }
#endif
}
/******************** Private function definitions *********************/

//This function is called (in irq context!) just before a transmission starts. It will
//set the D/C line to the value indicated in the user field.
static void lcd_spi_pre_transfer_callback(spi_transaction_t *t)
{
    int dc=(int)t->user;
    gpio_set_level(PIN_NUM_DC, dc);
}


//Initialize the display
static void lcd_init(spi_device_handle_t spi)
{
    int cmd=0;
    const lcd_init_cmd_t* lcd_init_cmds;

    //Initialize non-SPI GPIOs
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = ((1ULL<<PIN_NUM_DC) | (1ULL<<PIN_NUM_RST) | (1ULL<<PIN_NUM_BCKL));
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en = true;
    gpio_config(&io_conf);

    //Reset the display
    gpio_set_level(PIN_NUM_RST, 0);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    gpio_set_level(PIN_NUM_RST, 1);
    vTaskDelay(100 / portTICK_PERIOD_MS);

    lcd_init_cmds = ili_init_cmds;

    //Send all the commands
    while (lcd_init_cmds[cmd].databytes!=0xff)
    {
        lcd_cmd(spi, lcd_init_cmds[cmd].cmd, false);
        lcd_data(spi, lcd_init_cmds[cmd].data, lcd_init_cmds[cmd].databytes&0x1F);

        if (lcd_init_cmds[cmd].databytes&0x80)
        {
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }

        cmd++;
    }

    ///Enable backlight
    gpio_set_level(PIN_NUM_BCKL, 1);
}


/* Send a command to the LCD. Uses spi_device_polling_transmit, which waits
 * until the transfer is complete.
 *
 * Since command transactions are usually small, they are handled in polling
 * mode for higher speed. The overhead of interrupt transactions is more than
 * just waiting for the transaction to complete.
 */
static void lcd_cmd(spi_device_handle_t spi, const uint8_t cmd, bool keep_cs_active)
{
    esp_err_t ret;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));       //Zero out the transaction
    t.length=8;                     //Command is 8 bits
    t.tx_buffer=&cmd;               //The data is the cmd itself
    t.user=(void*)0;                //D/C needs to be set to 0
    if (keep_cs_active)
    {
      t.flags = SPI_TRANS_CS_KEEP_ACTIVE;   //Keep CS active after data transfer
    }
    ret=spi_device_polling_transmit(spi, &t);  //Transmit!
    assert(ret==ESP_OK);            //Should have had no issues.
}


/* Send data to the LCD. Uses spi_device_polling_transmit, which waits until the
 * transfer is complete.
 *
 * Since data transactions are usually small, they are handled in polling
 * mode for higher speed. The overhead of interrupt transactions is more than
 * just waiting for the transaction to complete.
 */
static void lcd_data(spi_device_handle_t spi, const uint8_t *data, int len)
{
    esp_err_t ret;
    spi_transaction_t t;
    if (len==0) return;             //no need to send anything
    memset(&t, 0, sizeof(t));       //Zero out the transaction
    t.length=len*8;                 //Len is in bytes, transaction length is in bits.
    t.tx_buffer=data;               //Data
    t.user=(void*)1;                //D/C needs to be set to 1
    ret=spi_device_polling_transmit(spi, &t);  //Transmit!
    assert(ret==ESP_OK);            //Should have had no issues.
}


/* Updates the whole screen */
static void send_lines(spi_device_handle_t spi, int xPos, int yPos, int width, int height, uint16_t *linedata)
{
    esp_err_t ret;
    int total_size_bytes = width * height * 2;

    //Transaction descriptors. Declared static so they're not allocated on the stack; we need this memory even when this
    //function is finished because the SPI driver needs access to it even while we're already calculating the next line.
    static spi_transaction_t trans[6];

	uint16_t end_column = (xPos + width) - 1u;
    uint16_t end_row = (yPos + height) - 1u;

    if (total_size_bytes > DISPLAY_MAX_TRANSFER_SIZE)
    {
    	/* Looks like we cannot go over this limit, so this will need to be taken into account when calling this function. */
    	printf("Jama...\n");
    	return;
    }

    //In theory, it's better to initialize trans and data only once and hang on to the initialized
    //variables. We allocate them on the stack, so we need to re-init them each call.
    for (int ix = 0; ix < 6; ix++)
    {
        memset(&trans[ix], 0, sizeof(spi_transaction_t));
        if ((ix & 1)==0)
        {
            //Even transfers are commands
            trans[ix].length=8;
            trans[ix].user=(void*)0;
        }
        else
        {
            //Odd transfers are data
            trans[ix].length=8*4;
            trans[ix].user=(void*)1;
        }
        trans[ix].flags=SPI_TRANS_USE_TXDATA;
    }

    trans[0].tx_data[0]=0x2A;           	//Column Address Set
    trans[1].tx_data[0]=xPos >> 8;      	//Start Col High
    trans[1].tx_data[1]=xPos & 0xffu;   	//Start Col Low
    trans[1].tx_data[2]=end_column >> 8;	//End Col High
    trans[1].tx_data[3]=end_column & 0xff;	//End Col Low
    trans[2].tx_data[0]=0x2B;           	//Page address set
    trans[3].tx_data[0]=yPos >> 8;        	//Start page high
    trans[3].tx_data[1]=yPos & 0xff;      	//start page low
    trans[3].tx_data[2]=end_row >> 8;    	//end page high
    trans[3].tx_data[3]=end_row & 0xff;  	//end page low
    trans[4].tx_data[0]=0x2C;           	//memory write
    trans[5].tx_buffer=linedata;        	//finally send the line data
    trans[5].length=total_size_bytes * 8;   //Data length, in bits
    trans[5].flags=0; //undo SPI_TRANS_USE_TXDATA flag

    //Queue all transactions.
    for (int ix=0; ix<6; ix++)
    {
        ret=spi_device_queue_trans(spi, &trans[ix], portMAX_DELAY);
        assert(ret==ESP_OK);
    }

    //When we are here, the SPI driver is busy (in the background) getting the transactions sent. That happens
    //mostly using DMA, so the CPU doesn't have much to do here. We're not going to wait for the transaction to
    //finish because we may as well spend the time calculating the next line. When that is done, we can call
    //send_line_finish, which will wait for the transfers to be done and check their status.
}


static void wait_line_finish(spi_device_handle_t spi)
{
    spi_transaction_t *rtrans;
    esp_err_t ret;
    //Wait for all 6 transactions to be done and get back the results.
    for (int x=0; x<6; x++)
    {
        ret=spi_device_get_trans_result(spi, &rtrans, portMAX_DELAY);
        assert(ret==ESP_OK);
        //We could inspect rtrans now if we received any info back. The LCD is treated as write-only, though.
    }
}
