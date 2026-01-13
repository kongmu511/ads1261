#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp32c6/rom/gpio.h"  /* For gpio_matrix_in/out ROM functions */
#include "loadcell.h"
#include "uart_cmd.h"
#include "ads1261.h"

static const char *TAG = "GRF_Platform";

/* Pin Configuration for ESP32-C6-WROOM Module
 * 
 * Hardware: ESP32-C6-WROOM-1U-N4 (module, not bare SOC)
 * Datasheet: ESP32-C6-WROOM-1 Datasheet
 * 
 * ⚠️ IMPORTANT: Custom board has non-standard SPI pin routing!
 * 
 * SPI Configuration (on custom board):
 * - MOSI: GPIO2  (SPI2 Data Out)
 * - MISO: GPIO7  (SPI2 Data In)
 * - SCLK: GPIO6  (SPI2 Clock)
 * - CS:   GND    (Hardwired, not GPIO controlled)
 * - DRDY: GPIO10 (ADS1261 Data Ready interrupt)
 */
#define MOSI_PIN    2       /* SPI2 MOSI */
#define MISO_PIN    7       /* SPI2 MISO */
#define CLK_PIN     6       /* SPI2 SCLK */
#define DRDY_PIN    10      /* ADS1261 DRDY */

/* Force Platform Configuration */
#define PGA_GAIN                ADS1261_PGA_GAIN_128        /* 128x gain for high resolution */
#define DATA_RATE               ADS1261_DR_40000_SPS        /* 40ksps system rate (max data rate) */
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

        /* Log measurements periodically (every 100 frames ~ every 1000ms) */
        if (measurement_count % 100 == 0) {
            float total_force = 0.0;

#if OUTPUT_FORMAT == OUTPUT_FORMAT_CSV
            /* CSV format: frame,timestamp,ch1,ch2,ch3,ch4,total */
            printf("%lu,%llu", measurement_count, loadcell_device.measurements[0].timestamp_us);
#else
            /* Human-readable format */
            ESP_LOGI(TAG, "[Frame %lu] Force readings:", measurement_count);
#endif

            for (int ch = 0; ch < 1; ch++) {  /* TEST: Read only 1 channel */
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

    /* Pre-configure GPIO pins to ensure proper GPIO matrix routing */
    /* MISO must be INPUT, others can be OUTPUT */
    gpio_set_direction(MISO_PIN, GPIO_MODE_INPUT);
    gpio_set_direction(MOSI_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(CLK_PIN, GPIO_MODE_OUTPUT);
    
    /* Disable pull-ups/downs initially */
    gpio_set_pull_mode(MISO_PIN, GPIO_FLOATING);
    gpio_set_pull_mode(MOSI_PIN, GPIO_FLOATING);
    gpio_set_pull_mode(CLK_PIN, GPIO_FLOATING);
    
    /* Test GPIO7 readability */
    ESP_LOGI(TAG, "Testing GPIO7 readability...");
    for (int i = 0; i < 10; i++) {
        int val = gpio_get_level(MISO_PIN);
        ESP_LOGI(TAG, "  GPIO7 read attempt %d: %d", i, val);
        esp_rom_delay_us(100);
    }
    
    ESP_LOGI(TAG, "GPIO pre-configured for SPI: MOSI=%d, MISO=%d, CLK=%d", MOSI_PIN, MISO_PIN, CLK_PIN);

    /* CRITICAL: Route GPIO7 (MISO) through GPIO matrix like Arduino-ESP32 does
     * The Arduino library uses SPI.begin(6, 7, 2) which internally calls gpio_matrix_in/out
     * We must do the same to make GPIO7 work as MISO for SPI2 on ESP32-C6
     * 
     * Signal indices for SPI2 on ESP32-C6:
     *   - MISO (input): 64 (SPI2_D)
     *   - MOSI (output): 66 (SPI2_Q) 
     *   - CLK (output): 65 (SPI2_CK)
     */
    gpio_matrix_in(MISO_PIN, 64, false);              /* GPIO7 <- SPI2 MISO signal */
    gpio_matrix_out(MOSI_PIN, 66, false, false);      /* GPIO2 -> SPI2 MOSI signal */
    gpio_matrix_out(CLK_PIN, 65, false, false);       /* GPIO6 -> SPI2 CLK signal */

    /* Configure SPI bus */
    spi_bus_config_t spi_cfg = {
        .mosi_io_num = MOSI_PIN,
        .miso_io_num = MISO_PIN,
        .sclk_io_num = CLK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
        .flags = SPICOMMON_BUSFLAG_GPIO_PINS,  /* Enable GPIO matrix for GPIO7 MISO routing */
    };

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "=== SPI Bus Configuration ===");
    ESP_LOGI(TAG, "MOSI GPIO: %d", MOSI_PIN);
    ESP_LOGI(TAG, "MISO GPIO: %d", MISO_PIN);
    ESP_LOGI(TAG, "CLK GPIO:  %d", CLK_PIN);
    ESP_LOGI(TAG, "max_transfer_sz: %d", spi_cfg.max_transfer_sz);
    ESP_LOGI(TAG, "flags: 0x%x", spi_cfg.flags);

    /* Initialize SPI bus */
    esp_err_t ret = spi_bus_initialize(SPI2_HOST, &spi_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "FAILED to initialize SPI bus: %s (0x%x)", esp_err_to_name(ret), ret);
        return;
    }

    ESP_LOGI(TAG, "");

    /* Initialize loadcell driver */
    ret = loadcell_init(&loadcell_device, SPI2_HOST, -1, DRDY_PIN, PGA_GAIN, DATA_RATE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize loadcell driver: %s", esp_err_to_name(ret));
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

