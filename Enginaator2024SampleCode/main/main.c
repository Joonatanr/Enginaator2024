#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include "driver/gpio.h"

#include "esp_timer.h"
#include "esp_task_wdt.h"

#include "esp_vfs_fat.h"

/* Private defines */

#define BLINK_GPIO 2u
#define CONFIG_BLINK_PERIOD 10u

#define MOUNT_POINT "/sdcard"

#define PIN_NUM_MISO  19
#define PIN_NUM_MOSI  23
#define PIN_NUM_CLK   18
#define PIN_NUM_CS    4

#define MAX_BMP_LINE_LENGTH 320u
#define CONVERT_888RGB_TO_565BGR(b, g, r) ((r >> 3) | ((g >> 2) << 5) | ((b >> 3) << 11))

/* Private type definitions */
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

/* Private function forward declarations */
static void configure_led(void);
static void configure_timer(void);
void timer_callback_10msec(void *param);
static void blink_led(void);

static void sdCard_init(void);
static esp_err_t read_bmp_file(const char *path, uint16_t * output_buffer);

/* Private variables */
volatile bool timer_flag = false;
static uint16_t timer_counter = 0u;
static const char *TAG = "SampleCode";

uint8_t  bmp_line_buffer[(MAX_BMP_LINE_LENGTH * 3) + 4u];
static uint16_t frame_buffer[320*240];
//static char str_buffer[64];

/* Public functions*/
void app_main(void)
{
	printf("Starting program...\n");

	configure_led();

	configure_timer();

	sdCard_init();

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

    //printf("blink");
}


static void sdCard_init(void)
{
    esp_err_t ret;

    // Options for mounting the filesystem.
    esp_vfs_fat_sdmmc_mount_config_t mount_config =
    {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    sdmmc_card_t *card;
    const char mount_point[] = MOUNT_POINT;
    ESP_LOGI(TAG, "Initializing SD card");

    // Use settings defined above to initialize SD card and mount FAT filesystem.
    // Note: esp_vfs_fat_sdmmc/sdspi_mount is all-in-one convenience functions.
    // Please check its source code and implement error recovery when developing
    // production applications.
    ESP_LOGI(TAG, "Using SPI peripheral");

    // By default, SD card frequency is initialized to SDMMC_FREQ_DEFAULT (20MHz)
    // For setting a specific frequency, use host.max_freq_khz (range 400kHz - 20MHz for SDSPI)
    // Example: for fixed frequency of 10MHz, use host.max_freq_khz = 10000;
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = VSPI_HOST;
    host.max_freq_khz = 4000;

    spi_bus_config_t bus_cfg =
    {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    ret = spi_bus_initialize(host.slot, &bus_cfg, VSPI_HOST);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize bus.");
        return;
    }

    // This initializes the slot without card detect (CD) and write protect (WP) signals.
    // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = host.slot;

    ESP_LOGI(TAG, "Mounting filesystem");
    ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
        {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                     "If you want the card to be formatted, set the CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
        }
        else
        {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                     "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
        }
        return;
    }

    ESP_LOGI(TAG, "Filesystem mounted");

    /* Lets try to load a bitmap. */
    const char *file_logo = MOUNT_POINT"/logo.bmp";
    ret = read_bmp_file(file_logo, frame_buffer);
    if (ret != ESP_OK)
    {
        return;
    }

    /* Cleanup */
    // All done, unmount partition and disable SPI peripheral
    esp_vfs_fat_sdcard_unmount(mount_point, card);
    ESP_LOGI(TAG, "Card unmounted");

    //deinitialize the bus after all devices are removed
    spi_bus_free(host.slot);
}


static esp_err_t read_bmp_file(const char *path, uint16_t * output_buffer)
{
	BMPHeader header;
	FILE *f;
	uint16_t line_stride;
	uint16_t line_px_data_len;
	uint16_t * dest_ptr = output_buffer;

	ESP_LOGI(TAG, "Reading file %s", path);
    f = fopen(path, "r");

    if (f == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file for reading");
        return ESP_FAIL;
    }

    fread(&header, sizeof(BMPHeader), 1u, f);

    ESP_LOGE(TAG, "Bitmap width : %ld", header.width_px);
    ESP_LOGE(TAG, "Bitmap height : %ld", header.height_px);

    /* Take padding into account... */
    line_px_data_len = header.width_px * 3u;
    line_stride = (line_px_data_len + 3u) & ~0x03;

    for (int y = 0u; y < header.height_px; y++)
    {
    	fseek(f, ((header.height_px - (y + 1)) * line_stride) + header.offset, SEEK_SET);
    	fread(bmp_line_buffer, sizeof(uint8_t), line_stride, f);

      for (int x = 0u; x < line_px_data_len; x+=3u )
      {
        *dest_ptr++ = CONVERT_888RGB_TO_565BGR(bmp_line_buffer[x+2], bmp_line_buffer[x+1], bmp_line_buffer[x]);
      }
    }

    fclose(f);

    return ESP_OK;
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


