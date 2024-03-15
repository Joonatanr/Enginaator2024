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

#define BLINK_GPIO 38u
#define CONFIG_BLINK_PERIOD 10u
#define BACKGROUND_COLOR COLOR_BLACK


#if 0
#define PIN_NUM_MISO  19
#define PIN_NUM_MOSI  23
#define PIN_NUM_CLK   18
#define PIN_NUM_CS    4
#else
/* For the S3 board: */
#define PIN_NUM_CLK   12
#define PIN_NUM_MOSI  11
#define PIN_NUM_MISO  13
#endif

/* Private type definitions */


/* Private function forward declarations */
static void configure_led(void);
static void configure_timer(void);
static void configure_spi(void);
static void drawRectangleInFrameBuf(int x, int y, int width, int height, uint16_t color);
static void drawBmpInFrameBuf(int xPos, int yPos, int width, int height, uint16_t * data_buf);

static void drawBackGround(void);
static void drawStar(uint16_t xPos, uint16_t yPos);

void timer_callback_10msec(void *param);

/* Private variables */
volatile bool timer_flag = false;
static uint16_t timer_counter = 0u;
static const char *TAG = "Main Program";

uint16_t priv_frame_buffer[240][320];

/* Public functions */
void app_main(void)
{
	printf("Starting program...\n");
    ESP_LOGI("memory", "Total available memory: %u bytes", heap_caps_get_total_size(MALLOC_CAP_8BIT));

	configure_led();

	configure_timer();

	configure_spi();

	sdCard_init();

	/* Initialize the main display. */
	display_init();

	vTaskDelay(1000 / portTICK_PERIOD_MS);

	sdCard_Read_bmp_file("/test.bmp", &priv_frame_buffer[0][0]);

	uint16_t * ship_buf = heap_caps_malloc(60*60*sizeof(uint16_t), MALLOC_CAP_DMA);
	sdCard_Read_bmp_file("/ship.bmp", ship_buf);

	display_test_image(&priv_frame_buffer[0][0]);

	for(int x = 0; x < 60*60; x++)
	{
		if (ship_buf[x] == 0xffffu)
		{
			ship_buf[x] = BACKGROUND_COLOR;
		}
	}

	vTaskDelay(200 / portTICK_PERIOD_MS);
	display_fillRectangle(0, 0, 60, 40, COLOR_RED);
	vTaskDelay(200 / portTICK_PERIOD_MS);
	display_fillRectangle(40, 40, 60, 40, COLOR_GREEN);
	vTaskDelay(200 / portTICK_PERIOD_MS);
	display_fillRectangle(80, 80, 60, 40, COLOR_BLUE);

	vTaskDelay(200 / portTICK_PERIOD_MS);
	display_fillRectangle(120, 120, 60, 40, COLOR_CYAN);
	vTaskDelay(200 / portTICK_PERIOD_MS);
	display_fillRectangle(160, 160, 60, 40, COLOR_YELLOW);
	vTaskDelay(200 / portTICK_PERIOD_MS);
	display_fillRectangle(200, 200, 60, 40, COLOR_MAGENTA);


	vTaskDelay(1000 / portTICK_PERIOD_MS);

	/* Lets try something dynamic now... */
	int yLocation = 0;
	bool direction = true;
	int speed = 4;

	TickType_t xLastWakeTime;
	const TickType_t xFrequency = 40u / portTICK_PERIOD_MS;

	xLastWakeTime = xTaskGetTickCount ();

	while(1)
	{
		vTaskDelayUntil( &xLastWakeTime, xFrequency );

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

		/* Draw the whole background */
		drawBackGround();

		/* Draw */
		drawBmpInFrameBuf(260, yLocation, 40, 53, ship_buf);

		drawRectangleInFrameBuf(10,  240 - yLocation - 40, 40, 40, COLOR_GREEN);
		drawRectangleInFrameBuf(210, 240 - yLocation - 40, 40, 40, COLOR_MAGENTA);

		display_test_image(&priv_frame_buffer[0][0]);
	}

	printf("System idle Process...\n");
	while(1)
	{
		vTaskDelay(500u / portTICK_PERIOD_MS);
		gpio_set_level(BLINK_GPIO, 1);
		vTaskDelay(500u / portTICK_PERIOD_MS);
		gpio_set_level(BLINK_GPIO, 0);
	}
}


static void drawRectangleInFrameBuf(int xPos, int yPos, int width, int height, uint16_t color)
{
	for (int x = xPos; ((x < (xPos+width)) && (x < 320)); x++)
	{
		for (int y = yPos; ((y < (yPos+height)) && (y < 240)); y++)
		{
			priv_frame_buffer[y][x] = color;
		}
	}
}

static void drawBmpInFrameBuf(int xPos, int yPos, int width, int height, uint16_t * data_buf)
{
	uint16_t * data_ptr = data_buf;

	for (int x = xPos; ((x < (xPos+width)) && (x < 320)); x++)
	{
		for (int y = yPos; ((y < (yPos+height)) && (y < 240)); y++)
		{
			priv_frame_buffer[y][x] = *data_ptr;
			data_ptr++;
		}
	}
}


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
		priv_frame_buffer[yPos][xPos] = 0xffffu;
		priv_frame_buffer[yPos + 1][xPos] = 0xffffu;
		priv_frame_buffer[yPos][xPos + 1] = 0xffffu;
		priv_frame_buffer[yPos + 1][xPos + 1] = 0xffffu;
	}
}


/* Private functions */
static void configure_led(void)
{
	printf("Configuring LED...\n");
    gpio_reset_pin(BLINK_GPIO);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
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


