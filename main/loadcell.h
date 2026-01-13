/**
 * @file loadcell.h
 * @brief Loadcell driver for ESP32 with ADS1261 ADC
 * 
 * Provides high-level API for 4-channel loadcell measurement with:
 * - Automatic tare/offset calibration
 * - Full-scale sensitivity calibration
 * - Real-time force reading with statistics
 * - Persistent calibration storage
 * 
 * Similar interface to popular HX711 Arduino libraries
 */

#ifndef LOADCELL_H
#define LOADCELL_H

#include "esp_err.h"
#include "driver/spi_master.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

/**
 * Calibration state for each loadcell channel
 */
typedef enum {
    CALIB_STATE_UNCALIBRATED = 0,    /**< No calibration performed */
    CALIB_STATE_TARE_READY = 1,      /**< Ready for tare calibration */
    CALIB_STATE_TARE_DONE = 2,       /**< Tare calibration completed */
    CALIB_STATE_SPAN_READY = 3,      /**< Ready for full-scale calibration */
    CALIB_STATE_CALIBRATED = 4,      /**< Fully calibrated and ready */
} loadcell_calib_state_t;

/**
 * Statistics for a single channel
 */
typedef struct {
    float min_force;        /**< Minimum force reading */
    float max_force;        /**< Maximum force reading */
    float avg_force;        /**< Average force reading */
    uint32_t sample_count;  /**< Number of samples collected */
} loadcell_stats_t;

/**
 * Single channel measurement
 */
typedef struct {
    int32_t raw_adc;        /**< Raw 24-bit ADC value */
    float normalized;       /**< Normalized to ±1.0 range */
    float force_newtons;    /**< Measured force in Newtons */
    uint64_t timestamp_us;  /**< Measurement timestamp in microseconds */
} loadcell_measurement_t;

/**
 * Loadcell channel context
 */
typedef struct {
    uint8_t channel_id;             /**< Channel index (0-3) */
    loadcell_calib_state_t calib_state;
    
    /* Calibration parameters */
    int32_t offset_raw;             /**< Raw ADC offset (from tare) */
    float scale_factor;             /**< N per normalized unit */
    
    /* Running statistics */
    loadcell_stats_t stats;
    loadcell_measurement_t last_measurement;
} loadcell_channel_t;

/**
 * Main loadcell device driver
 */
typedef struct {
    /* Hardware configuration */
    spi_device_handle_t spi_handle;
    int cs_pin;
    int drdy_pin;
    uint8_t pga_gain;
    uint8_t data_rate;
    
    /* Per-channel contexts */
    loadcell_channel_t channels[4];
    
    /* Current measurement frame */
    loadcell_measurement_t measurements[4];
    uint32_t frame_count;
    
} loadcell_t;

/* ============================================================================
 * API Functions
 * ============================================================================ */

/**
 * Initialize loadcell driver with ADS1261
 * 
 * @param[in] device            Loadcell device handle
 * @param[in] host              SPI host (HSPI_HOST or VSPI_HOST)
 * @param[in] cs_pin            Chip select GPIO pin
 * @param[in] drdy_pin          Data ready GPIO pin
 * @param[in] pga_gain          PGA gain setting (e.g., ADS1261_PGA_GAIN_128)
 * @param[in] data_rate         Data rate setting (e.g., ADS1261_DR_40)
 * 
 * @return ESP_OK on success, ESP_FAIL otherwise
 */
esp_err_t loadcell_init(loadcell_t *device, spi_host_device_t host,
                       int cs_pin, int drdy_pin,
                       uint8_t pga_gain, uint8_t data_rate);

/**
 * De-initialize loadcell driver and free resources
 * 
 * @param[in] device Loadcell device handle
 * @return ESP_OK on success
 */
esp_err_t loadcell_deinit(loadcell_t *device);

/* ============================================================================
 * Measurement Functions
 * ============================================================================ */

/**
 * Read all 4 loadcell channels in sequence
 * Updates device->measurements[] with latest values
 * 
 * @param[in] device Loadcell device handle
 * @return ESP_OK on success, ESP_FAIL otherwise
 */
esp_err_t loadcell_read(loadcell_t *device);

/**
 * Switch to a specific channel
 * 
 * @param[in] device    Loadcell device handle
 * @param[in] channel   Channel index (0-3)
 * 
 * @return ESP_OK on success
 */
esp_err_t loadcell_switch_channel(loadcell_t *device, uint8_t channel);

/**
 * Read single channel
 * 
 * @param[in] device        Loadcell device handle
 * @param[in] channel       Channel index (0-3)
 * @param[out] measurement  Measurement result
 * 
 * @return ESP_OK on success
 */
esp_err_t loadcell_read_channel(loadcell_t *device, uint8_t channel,
                                loadcell_measurement_t *measurement);

/**
 * Get last measurement for channel
 * 
 * @param[in] device       Loadcell device handle
 * @param[in] channel      Channel index (0-3)
 * @param[out] measurement Measurement result
 * 
 * @return ESP_OK on success
 */
esp_err_t loadcell_get_measurement(loadcell_t *device, uint8_t channel,
                                   loadcell_measurement_t *measurement);

/* ============================================================================
 * Calibration Functions
 * ============================================================================ */

/**
 * Tare (zero) calibration - must be done with no load applied
 * Captures offset value from multiple averaged samples
 * 
 * @param[in] device        Loadcell device handle
 * @param[in] channel       Channel index (0-3)
 * @param[in] num_samples   Number of samples to average (typically 100-500)
 * 
 * @return ESP_OK on success
 */
esp_err_t loadcell_tare(loadcell_t *device, uint8_t channel, uint32_t num_samples);

/**
 * Full-scale calibration - done with known weight on loadcell
 * Must call loadcell_tare() first!
 * 
 * @param[in] device        Loadcell device handle
 * @param[in] channel       Channel index (0-3)
 * @param[in] known_force_n Known force in Newtons (e.g., 100.0 for 100N)
 * @param[in] num_samples   Number of samples to average (typically 100-500)
 * 
 * @return ESP_OK on success
 */
esp_err_t loadcell_calibrate(loadcell_t *device, uint8_t channel,
                             float known_force_n, uint32_t num_samples);

/**
 * Get calibration status of channel
 * 
 * @param[in] device  Loadcell device handle
 * @param[in] channel Channel index (0-3)
 * 
 * @return Current calibration state
 */
loadcell_calib_state_t loadcell_get_calib_state(loadcell_t *device, uint8_t channel);

/**
 * Reset calibration for channel
 * 
 * @param[in] device  Loadcell device handle
 * @param[in] channel Channel index (0-3)
 * 
 * @return ESP_OK on success
 */
esp_err_t loadcell_reset_calibration(loadcell_t *device, uint8_t channel);

/**
 * Get channel statistics
 * 
 * @param[in] device  Loadcell device handle
 * @param[in] channel Channel index (0-3)
 * @param[out] stats  Statistics result
 * 
 * @return ESP_OK on success
 */
esp_err_t loadcell_get_stats(loadcell_t *device, uint8_t channel, loadcell_stats_t *stats);

/**
 * Reset channel statistics
 * 
 * @param[in] device  Loadcell device handle
 * @param[in] channel Channel index (0-3) or 4 for all channels
 * 
 * @return ESP_OK on success
 */
esp_err_t loadcell_reset_stats(loadcell_t *device, uint8_t channel);

/* ============================================================================
 * Debug/Utility Functions
 * ============================================================================ */

/**
 * @brief Run diagnostic on the ADS1261 communication
 * 
 * @param[in] device Loadcell device handle
 * 
 * @return ESP_OK if all diagnostics pass
 */
esp_err_t loadcell_diagnostic(loadcell_t *device);

/**
 * Print calibration info for all channels
 * 
 * @param[in] device Loadcell device handle
 */
void loadcell_print_calib_info(loadcell_t *device);

/**
 * Print current measurements and statistics
 * 
 * @param[in] device Loadcell device handle
 */
void loadcell_print_measurements(loadcell_t *device);

#ifdef __cplusplus
}
#endif

#endif /* LOADCELL_H */
