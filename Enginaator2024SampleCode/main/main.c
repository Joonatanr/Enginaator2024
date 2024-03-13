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

/* Private type definitions */


/* Private function forward declarations */
static void configure_led(void);
static void configure_timer(void);
void timer_callback_10msec(void *param);
static void blink_led(void);

/* Private variables */
volatile bool timer_flag = false;
static uint16_t timer_counter = 0u;

uint16_t priv_frame_buffer[320*240];

/* Public functions */
void app_main(void)
{
	printf("Starting program...\n");

	configure_led();

	configure_timer();

	sdCard_init();

	/* Initialize the main display. */
	display_init();

	vTaskDelay(1000 / portTICK_PERIOD_MS);

	display_test_image(priv_frame_buffer);


    while (1)
    {
        /* Required to prevent watchdog reset. */
    	vTaskDelay(CONFIG_BLINK_PERIOD / portTICK_PERIOD_MS);

        if (timer_flag)
        {
        	blink_led();
        	timer_flag = false;
        }
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

static void blink_led(void)
{
	static uint32_t s_led_state = 0u;

	s_led_state = !s_led_state;
	/* Set the GPIO level according to the state (LOW or HIGH)*/
    gpio_set_level(BLINK_GPIO, s_led_state);

    //printf("blink\n");
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


