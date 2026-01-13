/**
 * @file loadcell.c
 * @brief Loadcell driver implementation for ESP32 with ADS1261
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"  /* Added for gpio functions */
#include "ads1261.h"
#include "loadcell.h"

static const char *TAG = "LoadCell";

/* Constants */
#define ADC_MAX_VALUE           0x7FFFFF        /* Max 24-bit signed: 2^23-1 */
#define ADC_MIN_VALUE           -0x800000       /* Min 24-bit signed: -2^23 */

static ads1261_t adc_device;

/* ============================================================================
 * Initialization & Deinit
 * ============================================================================ */

esp_err_t loadcell_init(loadcell_t *device, spi_host_device_t host,
                       int cs_pin, int drdy_pin,
                       uint8_t pga_gain, uint8_t data_rate)
{
    if (!device) {
        return ESP_FAIL;
    }
    /* Initialize ads1261 device structure */
    memset(&adc_device, 0, sizeof(adc_device));
    adc_device.cs_pin = -1;
    adc_device.drdy_pin = -1;
    memset(device, 0, sizeof(loadcell_t));

    device->cs_pin = cs_pin;
    device->drdy_pin = drdy_pin;
    device->pga_gain = pga_gain;
    device->data_rate = data_rate;

    /* SPI bus already initialized by main.c - don't reinitialize */
    ESP_LOGI(TAG, "Using pre-initialized SPI bus on host %d", host);

    /* Initialize ADS1261 */
    esp_err_t ret = ads1261_init(&adc_device, host, cs_pin, drdy_pin);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize ADS1261");
        return ret;
    }

    /* Configure ADS1261 */
    ads1261_set_pga(&adc_device, pga_gain);
    ads1261_set_datarate(&adc_device, data_rate);
    ads1261_set_ref(&adc_device, ADS1261_REFSEL_EXT1);

    /* Initialize channels */
    for (int i = 0; i < 4; i++) {
        device->channels[i].channel_id = i;
        device->channels[i].calib_state = CALIB_STATE_UNCALIBRATED;
        device->channels[i].offset_raw = 0;
        device->channels[i].scale_factor = 1.0;
        device->channels[i].stats.min_force = 0.0;
        device->channels[i].stats.max_force = 0.0;
        device->channels[i].stats.avg_force = 0.0;
        device->channels[i].stats.sample_count = 0;
    }

    device->frame_count = 0;

    ESP_LOGI(TAG, "Loadcell driver initialized");
    ESP_LOGI(TAG, "  Channels: 4 (differential configuration)");
    ESP_LOGI(TAG, "  PGA Gain: %d", pga_gain);
    ESP_LOGI(TAG, "  Data Rate: %d", data_rate);

    return ESP_OK;
}

esp_err_t loadcell_deinit(loadcell_t *device)
{
    if (!device) {
        return ESP_FAIL;
    }

    ads1261_deinit(&adc_device);
    ESP_LOGI(TAG, "Loadcell driver deinitialized");

    return ESP_OK;
}

/* ============================================================================
 * Measurement Functions
 * ============================================================================ */

esp_err_t loadcell_read(loadcell_t *device)
{
    if (!device) {
        return ESP_FAIL;
    }

    esp_err_t ret;

    /* Read all 4 channels in sequence */
    for (int ch = 0; ch < 4; ch++) {
        ret = loadcell_read_channel(device, ch, &device->measurements[ch]);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read channel %d", ch);
            return ret;
        }
    }

    device->frame_count++;
    return ESP_OK;
}

esp_err_t loadcell_read_channel(loadcell_t *device, uint8_t channel, loadcell_measurement_t *measurement)
{
    if (!device || !measurement || channel >= 4) {
        return ESP_ERR_INVALID_ARG;
    }

    // Switch to the appropriate channel
    esp_err_t switch_ret = loadcell_switch_channel(device, channel);
    if (switch_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to switch to channel %d", channel);
        return switch_ret;
    }

    // Small settling delay after switching channels
    esp_rom_delay_us(100);

    // Read from ADC
    int32_t raw_value;
    esp_err_t adc_ret = ads1261_read_adc(&adc_device, &raw_value);
    if (adc_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read ADC for channel %d", channel);
        return adc_ret;
    }

    ESP_LOGI(TAG, "Channel %d read: raw=0x%06X (%d)", channel, raw_value & 0xFFFFFF, raw_value);

    // Fill in the measurement structure
    measurement->raw_adc = raw_value;
    measurement->timestamp_us = esp_timer_get_time();
    
    // Calculate normalized value based on calibration
    loadcell_channel_t *channel_ctx = &device->channels[channel];
    float normalized_raw = (float)(raw_value - channel_ctx->offset_raw);
    measurement->normalized = normalized_raw; 
    measurement->force_newtons = normalized_raw * channel_ctx->scale_factor;

    return ESP_OK;
}

/**
 * @brief Read raw ADC values from all channels
 * 
 * @param device Pointer to loadcell device
 * @param results Array to store results
 * @param num_results Size of results array
 * @return esp_err_t 
 */
esp_err_t loadcell_read_all_channels(loadcell_t *device, int32_t *results, int num_results)
{
    if (!device || !results || num_results < 4) {  // Assuming 4 channels
        return ESP_ERR_INVALID_ARG;
    }

    // Check current MODE3 register to determine if we're in standalone mode
    uint8_t mode3 = 0;
    esp_err_t mode3_ret = ads1261_read_register(&adc_device, ADS1261_REG_MODE3, &mode3);
    bool in_standalone_mode = (mode3_ret == ESP_OK) && (((mode3 >> 4) & 1) == 1);

    if (in_standalone_mode) {
        ESP_LOGW(TAG, "ADS1261 is in standalone mode - direct DOUT reading may be needed");
    }

    for (int i = 0; i < 4; i++) {  // Loop through all 4 channels
        loadcell_measurement_t measurement;
        esp_err_t ret = loadcell_read_channel(device, i, &measurement);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read channel %d", i);
            return ret;
        }
        results[i] = measurement.raw_adc;
    }

    return ESP_OK;
}

esp_err_t loadcell_get_measurement(loadcell_t *device, uint8_t channel,
                                   loadcell_measurement_t *measurement)
{
    if (!device || channel > 3 || !measurement) {
        return ESP_FAIL;
    }

    *measurement = device->channels[channel].last_measurement;
    return ESP_OK;
}

/* ============================================================================
 * Calibration Functions
 * ============================================================================ */

esp_err_t loadcell_tare(loadcell_t *device, uint8_t channel, uint32_t num_samples)
{
    if (!device || channel >= 4 || num_samples == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Starting tare calibration for channel %d (%lu samples)...", channel, num_samples);

    // Collect multiple samples to average the offset
    int64_t sum = 0;
    for (uint32_t i = 0; i < num_samples; i++) {
        loadcell_measurement_t meas;
        esp_err_t ret = loadcell_read_channel(device, channel, &meas);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Error reading channel %d for tare", channel);
            return ret;
        }
        sum += meas.raw_adc;
        vTaskDelay(pdMS_TO_TICKS(1));  // Small delay between samples
    }

    // Calculate and store the average as the offset
    int32_t avg = (int32_t)(sum / num_samples);
    device->channels[channel].offset_raw = avg;
    device->channels[channel].calib_state = CALIB_STATE_TARE_DONE;

    ESP_LOGI(TAG, "Tare calibration for channel %d: offset=%ld", channel, (long)avg);
    return ESP_OK;
}

esp_err_t loadcell_calibrate(loadcell_t *device, uint8_t channel, float known_force_n, uint32_t num_samples)
{
    if (!device || channel >= 4 || num_samples == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    // Must have completed tare calibration first
    if (device->channels[channel].calib_state != CALIB_STATE_TARE_DONE) {
        ESP_LOGE(TAG, "Must perform tare calibration before full-scale calibration on channel %d", channel);
        return ESP_ERR_INVALID_STATE;
    }

    // Collect multiple samples with known weight applied
    int64_t sum = 0;
    for (uint32_t i = 0; i < num_samples; i++) {
        loadcell_measurement_t meas;
        esp_err_t ret = loadcell_read_channel(device, channel, &meas);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Error reading channel %d for calibration", channel);
            return ret;
        }
        sum += meas.raw_adc;
        vTaskDelay(pdMS_TO_TICKS(1));  // Small delay between samples
    }

    int32_t avg = (int32_t)(sum / num_samples);
    int32_t delta_raw = avg - device->channels[channel].offset_raw;

    // Calculate scale factor: how many raw units per Newton
    if (delta_raw != 0) {
        device->channels[channel].scale_factor = known_force_n / (float)delta_raw;
        device->channels[channel].calib_state = CALIB_STATE_CALIBRATED;

        ESP_LOGI(TAG, "Scale calibration for channel %d: avg=%ld, delta=%ld, scale=%.6f/N",
                 channel, (long)avg, (long)delta_raw, 1.0/device->channels[channel].scale_factor);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Zero delta detected for channel %d - invalid calibration", channel);
        return ESP_FAIL;
    }
}

loadcell_calib_state_t loadcell_get_calib_state(loadcell_t *device, uint8_t channel)
{
    if (!device || channel > 3) {
        return CALIB_STATE_UNCALIBRATED;
    }
    return device->channels[channel].calib_state;
}

esp_err_t loadcell_reset_calibration(loadcell_t *device, uint8_t channel)
{
    if (!device || channel > 3) {
        return ESP_FAIL;
    }

    device->channels[channel].calib_state = CALIB_STATE_UNCALIBRATED;
    device->channels[channel].offset_raw = 0;
    device->channels[channel].scale_factor = 1.0;

    ESP_LOGI(TAG, "Calibration reset for channel %d", channel);

    return ESP_OK;
}

esp_err_t loadcell_get_stats(loadcell_t *device, uint8_t channel, loadcell_stats_t *stats)
{
    if (!device || channel > 3 || !stats) {
        return ESP_FAIL;
    }

    *stats = device->channels[channel].stats;
    return ESP_OK;
}

esp_err_t loadcell_reset_stats(loadcell_t *device, uint8_t channel)
{
    if (!device) {
        return ESP_FAIL;
    }

    if (channel == 4) {
        /* Reset all channels */
        for (int i = 0; i < 4; i++) {
            device->channels[i].stats.min_force = 0.0;
            device->channels[i].stats.max_force = 0.0;
            device->channels[i].stats.avg_force = 0.0;
            device->channels[i].stats.sample_count = 0;
        }
        ESP_LOGI(TAG, "Statistics reset for all channels");
    } else if (channel <= 3) {
        device->channels[channel].stats.min_force = 0.0;
        device->channels[channel].stats.max_force = 0.0;
        device->channels[channel].stats.avg_force = 0.0;
        device->channels[channel].stats.sample_count = 0;
        ESP_LOGI(TAG, "Statistics reset for channel %d", channel);
    } else {
        return ESP_FAIL;
    }

    return ESP_OK;
}

/* ============================================================================
 * Debug/Utility Functions
 * ============================================================================ */

void loadcell_print_calib_info(loadcell_t *device)
{
    if (!device) return;

    printf("\n=== Loadcell Calibration Status ===\n");
    const char *states[] = {
        "UNCALIBRATED",
        "TARE_READY",
        "TARE_DONE",
        "SPAN_READY",
        "CALIBRATED"
    };

    for (int i = 0; i < 4; i++) {
        loadcell_channel_t *ch = &device->channels[i];
        printf("Channel %d:\n", i + 1);
        printf("  State: %s\n", states[ch->calib_state]);
        printf("  Offset: %ld\n", ch->offset_raw);
        printf("  Scale: %.6f N/unit\n", ch->scale_factor);
    }
    printf("===================================\n\n");
}

void loadcell_print_measurements(loadcell_t *device)
{
    if (!device) return;

    printf("\n=== Loadcell Measurements (Frame %lu) ===\n", device->frame_count);

    float total = 0.0;
    for (int i = 0; i < 4; i++) {
        loadcell_measurement_t *m = &device->measurements[i];
        loadcell_stats_t *s = &device->channels[i].stats;

        printf("Channel %d: %.2f N\n", i + 1, m->force_newtons);
        printf("  Raw ADC: 0x%06lx (normalized: %.6f)\n", 
               m->raw_adc & 0xFFFFFF, m->normalized);
        printf("  Stats: min=%.2f, max=%.2f, avg=%.2f (n=%lu)\n",
               s->min_force, s->max_force, s->avg_force, s->sample_count);

        total += m->force_newtons;
    }

    printf("Total GRF: %.2f N\n", total);
    printf("========================================\n\n");
}

esp_err_t loadcell_diagnostic(loadcell_t *device)
{
    if (!device) {
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "=== Loadcell Diagnostic Report ===");
    
    // Read all registers to verify communication
    uint8_t reg_values[19];
    bool all_reads_ok = true;
    
    for (int i = 0; i < 19; i++) {
        esp_err_t ret = ads1261_read_register(&adc_device, i, &reg_values[i]);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to read register 0x%02x: %s", i, esp_err_to_name(ret));
            all_reads_ok = false;
        }
    }
    
    if (all_reads_ok) {
        ESP_LOGI(TAG, "All register reads successful!");
        ESP_LOGI(TAG, "ID:0x%02x ST:0x%02x M0:0x%02x M1:0x%02x M2:0x%02x M3:0x%02x REF:0x%02x PGA:0x%02x INP:0x%02x", 
                 reg_values[0], reg_values[1], reg_values[2], reg_values[3], 
                 reg_values[4], reg_values[5], reg_values[6], reg_values[0x10], reg_values[0x11]);
        
        // Check if ID register has expected value
        if (reg_values[0] != 0x08) {
            ESP_LOGW(TAG, "⚠️  Unexpected ID register value (expected 0x08, got 0x%02x)", reg_values[0]);
            ESP_LOGW(TAG, "    Possible causes:");
            ESP_LOGW(TAG, "    - Wrong SPI pins connected");
            ESP_LOGW(TAG, "    - ADS1261 not powered");
            ESP_LOGW(TAG, "    - CS pin not properly connected (should be tied to GND or controlled via GPIO)");
            ESP_LOGW(TAG, "    - SPI clock speed too high");
            ESP_LOGW(TAG, "    - Hardware wiring issues");
            ESP_LOGW(TAG, "    - ADS1261 chip may be damaged");
        } else {
            ESP_LOGI(TAG, "✅ ADS1261 ID register OK");
        }
        
        // Check MODE3 register for SPITIM bit
        uint8_t spitim = (reg_values[5] >> 4) & 1;
        if (spitim) {
            ESP_LOGW(TAG, "⚠️  Device is in STANDALONE DOUT mode (SPITIM=1)");
            ESP_LOGW(TAG, "    Driver expects DOUT/DRDY mode (SPITIM=0)");
            ESP_LOGW(TAG, "    This explains DRDY timeout errors!");
            ESP_LOGW(TAG, "    The device continuously outputs data - no DRDY signal expected");
            ESP_LOGW(TAG, "    Check hardware setup and MODE3 register configuration");
        } else {
            ESP_LOGI(TAG, "✅ Device is in DOUT/DRDY mode (SPITIM=0) - as expected");
        }
        
        // Check PGA gain setting
        uint8_t gain_bits = (reg_values[0x10] >> 4) & 0x07;
        if (gain_bits != device->pga_gain) {
            ESP_LOGW(TAG, "⚠️  PGA gain mismatch! Expected: %d, Actual: %d", device->pga_gain, gain_bits);
        } else {
            ESP_LOGI(TAG, "✅ PGA gain setting correct");
        }
        
        // Check data rate setting
        uint8_t drate_bits = reg_values[2] & 0x0F;
        if (drate_bits != device->data_rate) {
            ESP_LOGW(TAG, "⚠️  Data rate mismatch! Expected: %d, Actual: %d", device->data_rate, drate_bits);
        } else {
            ESP_LOGI(TAG, "✅ Data rate setting correct");
        }
    } else {
        ESP_LOGE(TAG, "❌ Some register reads failed - communication issue detected");
    }
    
    // Check DRDY pin status
    if (device->drdy_pin >= 0) {
        int drdy_level = gpio_get_level(device->drdy_pin);
        ESP_LOGI(TAG, "DRDY pin (GPIO %d) level: %d", device->drdy_pin, drdy_level);
        if (drdy_level == 1) {
            ESP_LOGW(TAG, "⚠️  DRDY pin is HIGH - should go LOW when data ready");
            ESP_LOGW(TAG, "    This may indicate:");
            ESP_LOGW(TAG, "    - ADS1261 not converting (need to send START command)");
            ESP_LOGW(TAG, "    - DRDY pin not connected properly");
            ESP_LOGW(TAG, "    - ADS1261 not responding");
            ESP_LOGW(TAG, "    - ADS1261 is in standalone mode (SPITIM=1)");
        } else {
            ESP_LOGI(TAG, "✅ DRDY pin is LOW - this is expected when data is ready");
        }
    } else {
        ESP_LOGI(TAG, "DRDY pin not configured (using polling mode)");
    }
    
    // Test reading ADC values
    ESP_LOGI(TAG, "Testing ADC read...");
    int32_t test_val;
    esp_err_t adc_ret = ads1261_read_adc(&adc_device, &test_val);
    if (adc_ret == ESP_OK) {
        ESP_LOGI(TAG, "ADC read successful: 0x%06lx (%ld)", test_val & 0xFFFFFF, test_val);
        if (test_val == 0x00FFFFFF || test_val == 0x00000000) {
            ESP_LOGW(TAG, "⚠️  ADC value suspicious: all 1s or all 0s - likely communication issue");
        }
    } else {
        ESP_LOGE(TAG, "ADC read failed: %s", esp_err_to_name(adc_ret));
    }
    
    ESP_LOGI(TAG, "===============================");
    ESP_LOGI(TAG, "Hardware Troubleshooting Tips:");
    ESP_LOGI(TAG, "1. Verify all SPI connections (MOSI, MISO, CLK) are correct");
    ESP_LOGI(TAG, "2. Ensure CS pin is properly connected (tied to GND or GPIO controlled)");
    ESP_LOGI(TAG, "3. Verify DRDY pin is connected to GPIO %d", device->drdy_pin);
    ESP_LOGI(TAG, "4. Check power supply (3.3V) to ADS1261");
    ESP_LOGI(TAG, "5. If using standalone mode, modify software to read continuously from DOUT");
    ESP_LOGI(TAG, "===============================");
    return all_reads_ok ? ESP_OK : ESP_FAIL;
}

esp_err_t loadcell_switch_channel(loadcell_t *device, uint8_t channel)
{
    if (!device || channel >= 4) {
        return ESP_ERR_INVALID_ARG;
    }

    // Define channel pairs for differential measurement
    // This mapping assumes a Wheatstone bridge configuration
    uint8_t pos_inputs[4] = {0, 2, 4, 6};  // Positive inputs for channels 0-3
    uint8_t neg_inputs[4] = {1, 3, 5, 7};  // Negative inputs for channels 0-3

    // Select the appropriate positive and negative inputs for the channel
    uint8_t pos_input = pos_inputs[channel];
    uint8_t neg_input = neg_inputs[channel];

    // Configure the input multiplexer register (INPMUX)
    uint8_t inpmux_reg = (pos_input << 4) | neg_input;  // MUXP in upper 4 bits, MUXN in lower 4 bits
    esp_err_t ret = ads1261_write_register(&adc_device, ADS1261_REG_INPMUX, inpmux_reg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure INPMUX register for channel %d", channel);
        return ret;
    }

    // Small settling delay after switching channels
    esp_rom_delay_us(100);

    ESP_LOGD(TAG, "Switched to channel %d (AIN%d - AIN%d), INPMUX=0x%02x", 
             channel, pos_input, neg_input, inpmux_reg);
    return ESP_OK;
}
