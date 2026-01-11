/**
 * @file loadcell.c
 * @brief Loadcell driver implementation for ESP32 with ADS1261
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
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

    memset(device, 0, sizeof(loadcell_t));

    device->cs_pin = cs_pin;
    device->drdy_pin = drdy_pin;
    device->pga_gain = pga_gain;
    device->data_rate = data_rate;

    /* Initialize ADS1261 */
    esp_err_t ret = ads1261_init(&adc_device, NULL, cs_pin, drdy_pin);
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

esp_err_t loadcell_read_channel(loadcell_t *device, uint8_t channel,
                                loadcell_measurement_t *measurement)
{
    if (!device || channel > 3 || !measurement) {
        return ESP_FAIL;
    }

    esp_err_t ret;

    /* Configure multiplexer for differential measurement
     * Ch0: AIN0+ / AIN1-
     * Ch1: AIN2+ / AIN3-
     * Ch2: AIN4+ / AIN5-
     * Ch3: AIN6+ / AIN7-
     */
    uint8_t muxp = channel * 2;
    uint8_t muxn = muxp + 1;
    ads1261_set_mux(&adc_device, muxp, muxn);

    /* Start conversion */
    ads1261_start_conversion(&adc_device);

    /* Read ADC value (24-bit signed) */
    ret = ads1261_read_adc(&adc_device, &measurement->raw_adc);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read ADC for channel %d", channel);
        return ret;
    }

    /* Convert raw ADC to normalized ratiometric value (±1.0) */
    measurement->normalized = (float)measurement->raw_adc / ADC_MAX_VALUE;

    /* Apply offset (tare) calibration */
    measurement->normalized -= ((float)device->channels[channel].offset_raw / ADC_MAX_VALUE);

    /* Convert to force using sensitivity calibration */
    measurement->force_newtons = measurement->normalized * device->channels[channel].scale_factor;

    /* Timestamp */
    measurement->timestamp_us = esp_timer_get_time();

    /* Update channel statistics */
    loadcell_channel_t *ch = &device->channels[channel];
    ch->last_measurement = *measurement;

    if (ch->stats.sample_count == 0) {
        ch->stats.min_force = measurement->force_newtons;
        ch->stats.max_force = measurement->force_newtons;
        ch->stats.avg_force = measurement->force_newtons;
    } else {
        if (measurement->force_newtons < ch->stats.min_force) {
            ch->stats.min_force = measurement->force_newtons;
        }
        if (measurement->force_newtons > ch->stats.max_force) {
            ch->stats.max_force = measurement->force_newtons;
        }
        /* Incremental average */
        float old_avg = ch->stats.avg_force;
        ch->stats.avg_force = (old_avg * ch->stats.sample_count + measurement->force_newtons) /
                             (ch->stats.sample_count + 1);
    }
    ch->stats.sample_count++;

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
    if (!device || channel > 3 || num_samples == 0) {
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Starting tare calibration for channel %d (%lu samples)...", channel, num_samples);

    loadcell_channel_t *ch = &device->channels[channel];
    int64_t sum = 0;

    /* Average multiple samples */
    for (uint32_t i = 0; i < num_samples; i++) {
        loadcell_measurement_t meas;
        esp_err_t ret = loadcell_read_channel(device, channel, &meas);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Tare: Failed to read sample %lu", i);
            return ret;
        }
        sum += meas.raw_adc;

        if (i % (num_samples / 10 + 1) == 0) {
            ESP_LOGI(TAG, "  Progress: %lu/%lu", i, num_samples);
        }

        vTaskDelay(pdMS_TO_TICKS(10));  /* Small delay between samples */
    }

    /* Store offset */
    ch->offset_raw = (int32_t)(sum / num_samples);
    ch->calib_state = CALIB_STATE_TARE_DONE;

    ESP_LOGI(TAG, "Tare calibration done. Offset: %ld", ch->offset_raw);

    return ESP_OK;
}

esp_err_t loadcell_calibrate(loadcell_t *device, uint8_t channel,
                             float known_force_n, uint32_t num_samples)
{
    if (!device || channel > 3 || num_samples == 0) {
        return ESP_FAIL;
    }

    if (device->channels[channel].calib_state != CALIB_STATE_TARE_DONE) {
        ESP_LOGW(TAG, "Channel %d not tared yet. Performing tare first...", channel);
        esp_err_t ret = loadcell_tare(device, channel, num_samples);
        if (ret != ESP_OK) {
            return ret;
        }
    }

    ESP_LOGI(TAG, "Starting span calibration for channel %d (%.2f N, %lu samples)...",
             channel, known_force_n, num_samples);

    loadcell_channel_t *ch = &device->channels[channel];
    float sum = 0.0;

    /* Average multiple readings after tare offset */
    for (uint32_t i = 0; i < num_samples; i++) {
        loadcell_measurement_t meas;
        esp_err_t ret = loadcell_read_channel(device, channel, &meas);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Calibrate: Failed to read sample %lu", i);
            return ret;
        }
        sum += meas.normalized;

        if (i % (num_samples / 10 + 1) == 0) {
            ESP_LOGI(TAG, "  Progress: %lu/%lu (avg normalized: %.6f)", i, num_samples, sum / (i + 1));
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }

    /* Calculate scale factor */
    float avg_normalized = sum / num_samples;
    if (fabs(avg_normalized) < 1e-6) {
        ESP_LOGE(TAG, "Invalid calibration: normalized reading is too small");
        return ESP_FAIL;
    }

    ch->scale_factor = known_force_n / avg_normalized;
    ch->calib_state = CALIB_STATE_CALIBRATED;

    ESP_LOGI(TAG, "Span calibration done.");
    ESP_LOGI(TAG, "  Scale factor: %.6f N per unit", ch->scale_factor);
    ESP_LOGI(TAG, "  Channel %d fully calibrated", channel);

    return ESP_OK;
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
