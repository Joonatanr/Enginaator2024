#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include "driver/gpio.h"

#include "esp_timer.h"
#include "esp_task_wdt.h"
#include "esp_vfs_fat.h"

#include "display.h"
#include "sdCard.h"

/* Private defines */

#define CONFIG_BLINK_PERIOD 10u
#define BACKGROUND_COLOR COLOR_BLACK

#define BUTTON_DOWN 	39u
#define BUTTON_UP 		38u
#define BUTTON_RIGHT 	47u
#define BUTTON_LEFT 	41u

#define BUTTON_TRIGGER 	40u

/* For the S3 board: */
#define PIN_NUM_CLK   12
#define PIN_NUM_MOSI  11
#define PIN_NUM_MISO  13

/* Private type definitions */


/* Private function forward declarations */
static void configure_led(void);
static void configure_timer(void);
static void configure_spi(void);
static void drawRectangleInFrameBuf(int x, int y, int width, int height, uint16_t color);
static void drawBmpInFrameBuf(int xPos, int yPos, int width, int height, uint16_t * data_buf);

static void updateDisplayedElements(void);
static void updateFrameBuffer(void);
static void flushFrameBuffer(void);

static void drawBackGround(void);
static void drawStar(uint16_t xPos, uint16_t yPos);
static void drawBullet(uint16_t xPos, uint16_t yPos);

void timer_callback_10msec(void *param);

static void init_buttons(void);

/* Private variables */
volatile bool timer_flag = false;
static uint16_t timer_counter = 0u;
static const char *TAG = "Main Program";

static int yLocation = 0;
static int ship_y = 90;
static int ship_x = 240;

static int bullet_x = 320;
static int bullet_y = 240;

const static int ship_speed = 3u;

static bool direction = true;
static int speed = 4;

#define ENABLE_DOUBLE_BUFFERING

//uint16_t priv_frame_buffer[240][320];
/* Lets test a double buffered solution. */
uint16_t * priv_frame_buffer1;
#ifdef ENABLE_DOUBLE_BUFFERING
uint16_t * priv_frame_buffer2;
#endif

uint16_t ** priv_curr_frame_buffer;

/* Cached visual elements. */
uint16_t * ship_buf;

/* Public functions */
void app_main(void)
{
	printf("Starting program...\n");
    ESP_LOGI("memory", "Total available memory: %u bytes", heap_caps_get_total_size(MALLOC_CAP_8BIT));

    priv_frame_buffer1 = heap_caps_malloc(240*320*sizeof(uint16_t), MALLOC_CAP_DMA);
    assert(priv_frame_buffer1);

#ifdef ENABLE_DOUBLE_BUFFERING
    priv_frame_buffer2 = heap_caps_malloc(240*320*sizeof(uint16_t), MALLOC_CAP_DMA);
    assert(priv_frame_buffer2);
#endif

    priv_curr_frame_buffer = &priv_frame_buffer1;

	configure_led();

	init_buttons();

	configure_timer();

	configure_spi();

	sdCard_init();

	/* Initialize the main display. */
	display_init();
	display_fillRectangle(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, COLOR_BLUE);

	vTaskDelay(1000 / portTICK_PERIOD_MS);

	sdCard_Read_bmp_file("/test.bmp", *priv_curr_frame_buffer);

	ship_buf = heap_caps_malloc(60*60*sizeof(uint16_t), MALLOC_CAP_DMA);
	sdCard_Read_bmp_file("/ship.bmp", ship_buf);

	display_drawScreenBuffer(*priv_curr_frame_buffer);
	vTaskDelay(2000 / portTICK_PERIOD_MS);


	for(int x = 0; x < 60*60; x++)
	{
		if (ship_buf[x] == 0xffffu)
		{
			ship_buf[x] = BACKGROUND_COLOR;
		}
	}

	vTaskDelay(400 / portTICK_PERIOD_MS);
	display_fillRectangle(0, 0, 60, 40, COLOR_RED);
	vTaskDelay(400 / portTICK_PERIOD_MS);
	display_fillRectangle(40, 40, 60, 40, COLOR_GREEN);
	vTaskDelay(400 / portTICK_PERIOD_MS);
	display_fillRectangle(80, 80, 60, 40, COLOR_BLUE);

	vTaskDelay(400 / portTICK_PERIOD_MS);
	display_fillRectangle(120, 120, 60, 40, COLOR_CYAN);
	vTaskDelay(400 / portTICK_PERIOD_MS);
	display_fillRectangle(160, 160, 60, 40, COLOR_YELLOW);
	vTaskDelay(400 / portTICK_PERIOD_MS);
	display_fillRectangle(200, 200, 60, 40, COLOR_MAGENTA);


	vTaskDelay(1000 / portTICK_PERIOD_MS);

	/* Lets try something dynamic now... */


	TickType_t xLastWakeTime;
	const TickType_t xFrequency = 40u / portTICK_PERIOD_MS;

	xLastWakeTime = xTaskGetTickCount ();

#ifdef ENABLE_DOUBLE_BUFFERING
	bool isBufferOne = true;
#endif

	while(1)
	{
		vTaskDelayUntil( &xLastWakeTime, xFrequency );

#ifdef ENABLE_DOUBLE_BUFFERING
		/* Switch the buffer - here we implement double buffering. */
		if (isBufferOne)
		{
			priv_curr_frame_buffer = &priv_frame_buffer2;
			isBufferOne = false;
		}
		else
		{
			priv_curr_frame_buffer = &priv_frame_buffer1;
			isBufferOne = true;
		}
#endif

		/*Here we update things like the location of the elements. Later we will check for buttons etc. */
		updateDisplayedElements();

		/*Here we draw into the frame buffer. */
		updateFrameBuffer();

		/* Here we send the frame buffer to be drawn by the display driver. */
		flushFrameBuffer();
	}

	printf("System idle Process...\n");
	while(1)
	{
		vTaskDelay(500u / portTICK_PERIOD_MS);
		//gpio_set_level(BLINK_GPIO, 1);
		vTaskDelay(500u / portTICK_PERIOD_MS);
		//gpio_set_level(BLINK_GPIO, 0);
	}
}

#define SET_FRAME_BUF_PIXEL(buf,x,y,color) *((buf) + (x) + (320*(y)))=color

static void drawRectangleInFrameBuf(int xPos, int yPos, int width, int height, uint16_t color)
{
	for (int x = xPos; ((x < (xPos+width)) && (x < 320)); x++)
	{
		for (int y = yPos; ((y < (yPos+height)) && (y < 240)); y++)
		{
			//priv_frame_buffer[y][x] = color;
			SET_FRAME_BUF_PIXEL(*priv_curr_frame_buffer, x, y, color);
		}
	}
}

static void drawBmpInFrameBuf(int xPos, int yPos, int width, int height, uint16_t * data_buf)
{
	uint16_t * data_ptr = data_buf;

	for (int x = xPos; (x < (xPos + width)); x++)
	{
		for (int y = yPos; (y < (yPos + height)); y++)
		{
			if ((x >= 0) && (x < DISPLAY_WIDTH) && (y >= 0) && (y < DISPLAY_HEIGHT))
			{
				SET_FRAME_BUF_PIXEL(*priv_curr_frame_buffer, x, y, *data_ptr);
			}
			data_ptr++;
		}
	}
}

static void updateDisplayedElements()
{
	if(direction)
	{
		if(yLocation >= 184u)
		{
			yLocation-= speed;
			direction = false;
		}
		else
		{
			yLocation+= speed;
		}
	}
	else
	{
		if (yLocation <= 0)
		{
			yLocation+= speed;
			direction = true;
		}
		else
		{
			yLocation-= speed;
		}
	}

	if (gpio_get_level(BUTTON_UP) == 0)
	{
		if(ship_x > 0)
		{
			ship_x -= ship_speed;
		}
	}
	else if(gpio_get_level(BUTTON_DOWN) == 0)
	{
		if(ship_x < DISPLAY_WIDTH)
		{
			ship_x += ship_speed;
		}
	}

	else if(gpio_get_level(BUTTON_RIGHT) == 0)
	{
		if(ship_y > 0)
		{
			ship_y-= ship_speed;
		}
	}
	else if(gpio_get_level(BUTTON_LEFT) == 0)
	{
		if(ship_y < DISPLAY_HEIGHT)
		{
			ship_y+= ship_speed;
		}
	}

	if (gpio_get_level(BUTTON_TRIGGER) == 0)
	{
		if (bullet_x <= 0)
		{
			bullet_x = ship_x;
			bullet_y = ship_y + 25;
		}
	}

	if(bullet_x > 0)
	{
		bullet_x -= 6u;
	}
}


static void updateFrameBuffer(void)
{
	/* Draw the whole background */
	drawBackGround();

	/* Draw Elements */
	drawBmpInFrameBuf(ship_x, ship_y, 40, 53, ship_buf);

	drawBullet(bullet_x, bullet_y);

	drawRectangleInFrameBuf(10,  240 - yLocation - 40, 20, 20, COLOR_GREEN);
	drawRectangleInFrameBuf(210, 240 - yLocation - 40, 20, 20, COLOR_MAGENTA);
}


static void flushFrameBuffer(void)
{
	display_drawScreenBuffer(*priv_curr_frame_buffer);
}



/***** Helper functions *****/
#define NUMBER_OF_STARS 20
typedef struct
{
	int xPos;
	int yPos;
} StarElement_T;

StarElement_T stars[NUMBER_OF_STARS];

static void drawBackGround(void)
{
	static bool isStarsInited = false;

	if(!isStarsInited)
	{
		for (int x = 0; x < NUMBER_OF_STARS; x++)
		{
			stars[x].xPos = random() % 320;
			stars[x].yPos = random() % 240;

			printf("Star at : X%d, Y%d\n", stars[x].xPos, stars[x].yPos);
		}

		isStarsInited = true;
	}

	drawRectangleInFrameBuf(0, 0, 320, 240, BACKGROUND_COLOR);

	for (int x = 0; x < NUMBER_OF_STARS; x++)
	{
		stars[x].xPos++;
		if (stars[x].xPos >= 319)
		{
			stars[x].xPos = 0;
		}

		drawStar(stars[x].xPos, stars[x].yPos);
	}
}

static void drawStar(uint16_t xPos, uint16_t yPos)
{
	if(xPos < 319u && yPos < 239u)
	{
		SET_FRAME_BUF_PIXEL(*priv_curr_frame_buffer, xPos, 		yPos, 		0xffffu);
		SET_FRAME_BUF_PIXEL(*priv_curr_frame_buffer, xPos + 1, 	yPos,	 	0xffffu);
		SET_FRAME_BUF_PIXEL(*priv_curr_frame_buffer, xPos, 		yPos + 1, 	0xffffu);
		SET_FRAME_BUF_PIXEL(*priv_curr_frame_buffer, xPos + 1, 	yPos + 1, 	0xffffu);
	}
}

static void drawBullet(uint16_t xPos, uint16_t yPos)
{
	if (xPos < (DISPLAY_WIDTH - 1) && (yPos < (DISPLAY_HEIGHT - 1)) && (xPos > 0) &&(yPos > 0))
	{
		SET_FRAME_BUF_PIXEL(*priv_curr_frame_buffer, xPos, 		yPos, 		COLOR_YELLOW);
		SET_FRAME_BUF_PIXEL(*priv_curr_frame_buffer, xPos + 1, 	yPos,	 	COLOR_RED);
		SET_FRAME_BUF_PIXEL(*priv_curr_frame_buffer, xPos, 		yPos + 1, 	COLOR_RED);
		SET_FRAME_BUF_PIXEL(*priv_curr_frame_buffer, xPos + 1, 	yPos + 1, 	COLOR_RED);

		SET_FRAME_BUF_PIXEL(*priv_curr_frame_buffer, xPos - 1, 	yPos,	 	COLOR_RED);
		SET_FRAME_BUF_PIXEL(*priv_curr_frame_buffer, xPos, 		yPos - 1, 	COLOR_RED);
		SET_FRAME_BUF_PIXEL(*priv_curr_frame_buffer, xPos - 1, 	yPos - 1, 	COLOR_RED);

		SET_FRAME_BUF_PIXEL(*priv_curr_frame_buffer, xPos - 1, 	yPos + 1,	COLOR_RED);
		SET_FRAME_BUF_PIXEL(*priv_curr_frame_buffer, xPos + 1, 	yPos - 1, 	COLOR_RED);
	}
}


/* Private functions */
static void configure_led(void)
{
	printf("Configuring LED...\n");
    //gpio_reset_pin(BLINK_GPIO);
    /* Set the GPIO as a push/pull output */
    //gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
}

static void configure_timer(void)
{
	const esp_timer_create_args_t my_timer_args =
	{
	      .callback = &timer_callback_10msec,
	      .name = "My Timer"
	};

	esp_timer_handle_t timer_handler;

	printf("Configuring timer...\n");

	ESP_ERROR_CHECK(esp_timer_create(&my_timer_args, &timer_handler));
	ESP_ERROR_CHECK(esp_timer_start_periodic(timer_handler, 10000)); /* Set timer to 10000 microseconds, (10ms) */
}


static void configure_spi(void)
{
    esp_err_t ret;

    // Use settings defined above to initialize SD card and mount FAT filesystem.
    // Note: esp_vfs_fat_sdmmc/sdspi_mount is all-in-one convenience functions.
    // Please check its source code and implement error recovery when developing
    // production applications.
    ESP_LOGI(TAG, "Setting up SPI peripheral");

    spi_bus_config_t bus_cfg =
    {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = DISPLAY_MAX_TRANSFER_SIZE,
    };

    ret = spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH_AUTO);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize bus.");
        return;
    }
}



void timer_callback_10msec(void *param)
{
	timer_counter++;

	if (timer_counter >= 100u)
	{
		timer_counter = 0u;
		timer_flag = true;
	}
}




static void init_buttons(void)
{
    gpio_set_direction(BUTTON_UP, GPIO_MODE_INPUT);
    gpio_set_direction(BUTTON_DOWN, GPIO_MODE_INPUT);
    gpio_set_direction(BUTTON_RIGHT, GPIO_MODE_INPUT);
    gpio_set_direction(BUTTON_TRIGGER, GPIO_MODE_INPUT);
    gpio_set_direction(BUTTON_LEFT, GPIO_MODE_INPUT);
}


