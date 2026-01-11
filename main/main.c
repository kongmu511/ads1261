#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "esp_log.h"
#include "driver/spi_master.h"
#include "ads1261.h"

static const char *TAG = "LoadCell";

/* Pin Configuration */
#define MOSI_PIN    GPIO_NUM_7
#define MISO_PIN    GPIO_NUM_8
#define CLK_PIN     GPIO_NUM_6
#define CS_PIN      GPIO_NUM_9
#define DRDY_PIN    GPIO_NUM_10

/* Loadcell Configuration */
#define NUM_LOADCELLS           4
#define PGA_GAIN                ADS1261_PGA_GAIN_128        /* 128x gain for higher resolution */
#define DATA_RATE               ADS1261_DR_600              /* 600 SPS for high data rate */
#define REFERENCE_VOLTAGE_MV    5000                        /* 5V external reference in mV */

typedef struct {
    int32_t raw_value;
    float voltage_mv;   /* Voltage in mV */
    float weight_kg;    /* Weight in kg */
} loadcell_data_t;

/* Calibration parameters */
/* For 24-bit ADC with PGA_GAIN_128 and 5V reference:
 * Scale factor = (VREF / PGA) / 2^23 = (5V / 128) / 2^23 */
static const float SCALE_FACTOR = 0.00004577636719;  /* mV per LSB with gain 128 */
static const float ZERO_OFFSET = 0.0;                /* Zero offset in mV - adjust based on calibration */
static const float CALIBRATION_FACTOR = 1.0;         /* kg per mV - adjust based on loadcell calibration */

static ads1261_t adc_device;
static loadcell_data_t loadcells[NUM_LOADCELLS];

void app_main(void)
{
    ESP_LOGI(TAG, "Starting ESP32C6 Loadcell Measurement System");
    ESP_LOGI(TAG, "Using ADS1261 ADC with %d loadcells", NUM_LOADCELLS);

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
        ESP_LOGE(TAG, "Failed to initialize SPI bus");
        return;
    }

    /* Initialize ADS1261 */
    ret = ads1261_init(&adc_device, &spi_cfg, CS_PIN, DRDY_PIN);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize ADS1261");
        return;
    }

    /* Configure ADS1261 */
    ads1261_set_pga(&adc_device, PGA_GAIN);
    ads1261_set_datarate(&adc_device, DATA_RATE);
    /* Use external reference for ratiometric measurement with excitation voltage */
    ads1261_set_ref(&adc_device, ADS1261_REFSEL_EXT1);

    ESP_LOGI(TAG, "ADS1261 configured: PGA=128x, DataRate=600SPS, Reference=External (Ratiometric)");

    /* Main measurement loop */
    int measurement_count = 0;
    while (1) {
        measurement_count++;
        ESP_LOGI(TAG, "=== Measurement %d ===", measurement_count);

        /* Read all four loadcell channels */
        for (int i = 0; i < NUM_LOADCELLS; i++) {
            /* Configure multiplexer for differential measurement */
            uint8_t muxp = i * 2;
            uint8_t muxn = muxp + 1;
            ads1261_set_mux(&adc_device, muxp, muxn);

            /* Start conversion */
            ads1261_start_conversion(&adc_device);

            /* Read ADC value */
            ret = ads1261_read_adc(&adc_device, &loadcells[i].raw_value);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to read loadcell %d", i + 1);
                continue;
            }

            /* Convert to voltage in mV (ratiometric measurement) */
            loadcells[i].voltage_mv = (float)loadcells[i].raw_value * SCALE_FACTOR;

            /* Apply calibration to get weight */
            loadcells[i].weight_kg = (loadcells[i].voltage_mv - ZERO_OFFSET) * CALIBRATION_FACTOR;

            ESP_LOGI(TAG, "Loadcell %d: Raw=%ld, Voltage=%.3fmV, Weight=%.3fkg",
                     i + 1, loadcells[i].raw_value, loadcells[i].voltage_mv, loadcells[i].weight_kg);
        }

        /* Print summary */
        float total_weight_kg = 0;
        for (int i = 0; i < NUM_LOADCELLS; i++) {
            total_weight_kg += loadcells[i].weight_kg;
        }
        ESP_LOGI(TAG, "Total Weight: %.3f kg\n", total_weight_kg);

        /* Wait before next measurement */
        vTaskDelay(1000 / portTICK_PERIOD_MS);  // 1 second interval
    }

    /* Cleanup */
    ads1261_deinit(&adc_device);
    spi_bus_free(HSPI_HOST);
}
