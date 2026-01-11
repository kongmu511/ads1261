#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "esp_log.h"
#include "driver/spi_master.h"
#include "loadcell.h"
#include "uart_cmd.h"
#include "ads1261.h"

static const char *TAG = "GRF_Platform";

/* Pin Configuration */
#define MOSI_PIN    GPIO_NUM_7
#define MISO_PIN    GPIO_NUM_8
#define CLK_PIN     GPIO_NUM_6
#define CS_PIN      GPIO_NUM_9
#define DRDY_PIN    GPIO_NUM_10

/* Force Platform Configuration */
#define PGA_GAIN                ADS1261_PGA_GAIN_128        /* 128x gain for high resolution */
#define DATA_RATE               ADS1261_DR_40               /* 40 kSPS = ~1000-1200 Hz per channel */
#define MEASUREMENT_INTERVAL_MS 10                          /* Read all 4 channels every 10ms */

/* Output Format Selection */
#define OUTPUT_FORMAT_HUMAN     1   /* Readable format with labels */
#define OUTPUT_FORMAT_CSV       0   /* CSV format for data logging */
#define OUTPUT_FORMAT           OUTPUT_FORMAT_HUMAN

static loadcell_t loadcell_device;
static uint32_t measurement_count = 0;

/**
 * Measurement task - reads loadcells periodically
 */
static void measurement_task(void *arg)
{
    ESP_LOGI(TAG, "Measurement task started");

    while (1) {
        esp_err_t ret = loadcell_read(&loadcell_device);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read loadcells");
            vTaskDelay(pdMS_TO_TICKS(MEASUREMENT_INTERVAL_MS));
            continue;
        }

        measurement_count++;

        /* Log measurements periodically (every 50 frames ~ every 500ms) */
        if (measurement_count % 50 == 0) {
            float total_force = 0.0;

#if OUTPUT_FORMAT == OUTPUT_FORMAT_CSV
            /* CSV format: frame,timestamp,ch1,ch2,ch3,ch4,total */
            printf("%lu,%llu", measurement_count, loadcell_device.measurements[0].timestamp_us);
#else
            /* Human-readable format */
            ESP_LOGI(TAG, "[Frame %lu] Force readings:", measurement_count);
#endif

            for (int ch = 0; ch < 4; ch++) {
                total_force += loadcell_device.measurements[ch].force_newtons;

#if OUTPUT_FORMAT == OUTPUT_FORMAT_CSV
                printf(",%.4f", loadcell_device.measurements[ch].force_newtons);
#else
                ESP_LOGI(TAG, "  Ch%d: %.2f N (raw=%06lx, norm=%.6f)",
                        ch + 1,
                        loadcell_device.measurements[ch].force_newtons,
                        loadcell_device.measurements[ch].raw_adc & 0xFFFFFF,
                        loadcell_device.measurements[ch].normalized);
#endif
            }

#if OUTPUT_FORMAT == OUTPUT_FORMAT_CSV
            printf(",%.4f\n", total_force);
#else
            ESP_LOGI(TAG, "  Total GRF: %.2f N", total_force);
#endif
        }

        vTaskDelay(pdMS_TO_TICKS(MEASUREMENT_INTERVAL_MS));
    }
}

/**
 * UART command interface task
 */
static void uart_cmd_task(void *arg)
{
    ESP_LOGI(TAG, "UART command task started");

    printf("> ");
    fflush(stdout);

    while (1) {
        uart_cmd_process();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  GRF Force Platform - Loadcell System");
    ESP_LOGI(TAG, "  ESP32-C6 + ADS1261 (4-channel)");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "");

    /* Configure SPI bus */
    spi_bus_config_t spi_cfg = {
        .mosi_io_num = MOSI_PIN,
        .miso_io_num = MISO_PIN,
        .sclk_io_num = CLK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };

    /* Initialize SPI bus */
    esp_err_t ret = spi_bus_initialize(HSPI_HOST, &spi_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
        return;
    }

    /* Initialize loadcell driver */
    ret = loadcell_init(&loadcell_device, HSPI_HOST, CS_PIN, DRDY_PIN, PGA_GAIN, DATA_RATE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize loadcell driver: %s", esp_err_to_name(ret));
        spi_bus_free(HSPI_HOST);
        return;
    }

    ESP_LOGI(TAG, "Configuration:");
    ESP_LOGI(TAG, "  - Channels: 4 (differential bridge configuration)");
    ESP_LOGI(TAG, "  - PGA Gain: 128x");
    ESP_LOGI(TAG, "  - Data Rate: 40 kSPS system (~1000-1200 Hz per channel)");
    ESP_LOGI(TAG, "  - Sample Interval: %d ms", MEASUREMENT_INTERVAL_MS);
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Initial State: UNCALIBRATED (perform tare first)");
    ESP_LOGI(TAG, "");

    /* Initialize UART command interface */
    uart_cmd_init(&loadcell_device);

    /* Start measurement task */
    xTaskCreate(measurement_task, "measurement", 4096, NULL, 5, NULL);

    /* Start UART command task */
    xTaskCreate(uart_cmd_task, "uart_cmd", 4096, NULL, 4, NULL);

    ESP_LOGI(TAG, "All tasks started. Ready for commands!");
}

