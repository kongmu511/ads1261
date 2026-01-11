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

/* Force Platform Configuration */
#define NUM_LOADCELLS           4
#define PGA_GAIN                ADS1261_PGA_GAIN_128        /* 128x gain for high resolution */
#define DATA_RATE               ADS1261_DR_600              /* 600 SPS = 150 SPS per channel (4-channel mux) */
#define SAMPLE_BUFFER_SIZE      1000                        /* Buffer for streaming measurements */

/* Note: Ratiometric measurement (bridge transducers)
 * - Do NOT need to know exact reference voltage value
 * - Bridge output is naturally ratiometric to excitation voltage
 * - Any voltage variations cancel out in the ratio
 * - Sensitivity is determined by bridge gauge factor, not Vref */

typedef struct {
    int32_t raw_adc;       /* Raw 24-bit ADC value */
    float normalized;      /* Normalized to ±1.0 range (ratiometric) */
    float force_newtons;   /* Measured force in Newtons */
    uint32_t timestamp_us; /* Measurement timestamp in microseconds */
} force_sample_t;

/* Calibration parameters - ratiometric approach */
/* No need for SCALE_FACTOR since we work with normalized ratio
 * Only need: zero offset (tare) and force sensitivity calibration */
static const float ZERO_OFFSET_RAW = 0.0;              /* ADC offset - measure with no load */
static const float FORCE_SENSITIVITY = 1.0;            /* N per normalized unit - calibrate with known load */
static const int32_t ADC_MAX_VALUE = 0x7FFFFF;         /* Max 24-bit signed: 2^23-1 */
static const int32_t ADC_MIN_VALUE = -0x800000;        /* Min 24-bit signed: -2^23 */

static ads1261_t adc_device;
static force_sample_t force_samples[NUM_LOADCELLS];  /* Latest samples from each channel */
static force_sample_t sample_buffer[SAMPLE_BUFFER_SIZE];
static uint16_t buffer_index = 0;
static uint32_t sample_count = 0;

void app_main(void)
{
    ESP_LOGI(TAG, "ESP32C6 Force Platform - Ground Reaction Force Measurement");
    ESP_LOGI(TAG, "4-channel loadcell bridge measurement system (ratiometric)");
    ESP_LOGI(TAG, "Max data rate: 10 kSPS system (600 SPS per channel with 4-ch mux)");

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
    /* Use external reference for ratiometric bridge measurement
     * Note: With ratiometric measurement, reference voltage variations don't affect accuracy
     * The bridge output ratio is independent of absolute Vref value */
    ads1261_set_ref(&adc_device, ADS1261_REFSEL_EXT1);

    ESP_LOGI(TAG, "ADS1261 configured:");
    ESP_LOGI(TAG, "  - PGA: 128x (high resolution)");
    ESP_LOGI(TAG, "  - Data rate: 600 SPS system");
    ESP_LOGI(TAG, "  - Per-channel: ~150 SPS (4-channel sequential mux)");
    ESP_LOGI(TAG, "  - Reference: External (ratiometric - no Vref value needed)");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Ground Reaction Force Measurement Starting...");

    /* Main measurement loop */
    int measurement_count = 0;
    while (1) {
        /* Read all four loadcell channels in sequence */
        for (int ch = 0; ch < NUM_LOADCELLS; ch++) {
            /* Configure multiplexer for differential measurement */
            uint8_t muxp = ch * 2;      /* Positive: AIN0, AIN2, AIN4, AIN6 */
            uint8_t muxn = muxp + 1;    /* Negative: AIN1, AIN3, AIN5, AIN7 */
            ads1261_set_mux(&adc_device, muxp, muxn);

            /* Start conversion */
            ads1261_start_conversion(&adc_device);

            /* Read ADC value (24-bit signed) */
            ret = ads1261_read_adc(&adc_device, &force_samples[ch].raw_adc);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to read loadcell %d", ch + 1);
                continue;
            }

            /* Convert raw ADC to normalized ratiometric value (±1.0)
             * With ratiometric measurement and external reference:
             * normalized = raw_adc / ADC_MAX_VALUE
             * No need for VREF scaling - bridge is naturally ratiometric */
            force_samples[ch].normalized = (float)force_samples[ch].raw_adc / ADC_MAX_VALUE;

            /* Apply zero offset (tare) calibration */
            force_samples[ch].normalized -= (ZERO_OFFSET_RAW / ADC_MAX_VALUE);

            /* Convert to force using sensitivity calibration
             * FORCE_SENSITIVITY = measured_force / normalized_adc_value
             * This must be calibrated with known loads during setup */
            force_samples[ch].force_newtons = force_samples[ch].normalized * FORCE_SENSITIVITY;

            force_samples[ch].timestamp_us = esp_timer_get_time();

            sample_count++;
        }

        /* Log current frame (all 4 channels) */
        if (sample_count % 4 == 0) {
            float total_force = 0;
            ESP_LOGI(TAG, "[Frame %lu] Force readings (N):", sample_count / 4);
            for (int ch = 0; ch < NUM_LOADCELLS; ch++) {
                ESP_LOGI(TAG, "  Ch%d: %.2f N (raw=%06lx, norm=%.4f)",
                        ch + 1,
                        force_samples[ch].force_newtons,
                        force_samples[ch].raw_adc & 0xFFFFFF,
                        force_samples[ch].normalized);
                total_force += force_samples[ch].force_newtons;
            }
            ESP_LOGI(TAG, "  Total: %.2f N", total_force);
        }

    /* Cleanup */
    ads1261_deinit(&adc_device);
    spi_bus_free(HSPI_HOST);
}
