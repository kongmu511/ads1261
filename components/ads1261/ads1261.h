#ifndef ADS1261_H
#define ADS1261_H

#include <stdint.h>
#include <stdbool.h>
#include "driver/spi_master.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ADS1261 Register Addresses */
#define ADS1261_REG_INPMUX      0x00
#define ADS1261_REG_PGA         0x01
#define ADS1261_REG_DATARATE    0x02
#define ADS1261_REG_REF         0x03
#define ADS1261_REG_IDACMAG     0x04
#define ADS1261_REG_IDACMUX     0x05
#define ADS1261_REG_VBIAS       0x06
#define ADS1261_REG_SYS         0x07
#define ADS1261_REG_OFCAL0      0x08
#define ADS1261_REG_OFCAL1      0x09
#define ADS1261_REG_OFCAL2      0x0A
#define ADS1261_REG_FSCAL0      0x0B
#define ADS1261_REG_FSCAL1      0x0C
#define ADS1261_REG_FSCAL2      0x0D
#define ADS1261_REG_GPIODAT     0x0E
#define ADS1261_REG_GPIOCON     0x0F

/* PGA Gain Settings */
#define ADS1261_PGA_GAIN_1      0x00
#define ADS1261_PGA_GAIN_2      0x01
#define ADS1261_PGA_GAIN_4      0x02
#define ADS1261_PGA_GAIN_8      0x03
#define ADS1261_PGA_GAIN_16     0x04
#define ADS1261_PGA_GAIN_32     0x05
#define ADS1261_PGA_GAIN_64     0x06
#define ADS1261_PGA_GAIN_128    0x07

/* Input Multiplexer Settings */
#define ADS1261_MUXP_AIN0       0x00
#define ADS1261_MUXN_AIN1       0x01

/* Data Rate Settings (in SPS - Samples Per Second) */
#define ADS1261_DR_20           0x00  /* 20 SPS */
#define ADS1261_DR_45           0x01  /* 45 SPS */
#define ADS1261_DR_90           0x02  /* 90 SPS */
#define ADS1261_DR_175          0x03  /* 175 SPS */
#define ADS1261_DR_330          0x04  /* 330 SPS */
#define ADS1261_DR_600          0x05  /* 600 SPS */
#define ADS1261_DR_1000         0x06  /* 1000 SPS */

/* Reference Selection */
#define ADS1261_REFSEL_INT      0x00
#define ADS1261_REFSEL_EXT1     0x01
#define ADS1261_REFSEL_EXT2     0x02

typedef struct {
    spi_device_handle_t spi_handle;
    int cs_pin;
    int drdy_pin;
} ads1261_t;

/* Initialize ADS1261 */
esp_err_t ads1261_init(ads1261_t *device, spi_bus_config_t *spi_bus_cfg, int cs_pin, int drdy_pin);

/* Configure input multiplexer for differential measurement */
esp_err_t ads1261_set_mux(ads1261_t *device, uint8_t muxp, uint8_t muxn);

/* Set PGA gain */
esp_err_t ads1261_set_pga(ads1261_t *device, uint8_t gain);

/* Set data rate */
esp_err_t ads1261_set_datarate(ads1261_t *device, uint8_t datarate);

/* Set reference selection */
esp_err_t ads1261_set_ref(ads1261_t *device, uint8_t refsel);

/* Start conversion */
esp_err_t ads1261_start_conversion(ads1261_t *device);

/* Read ADC value (24-bit) */
esp_err_t ads1261_read_adc(ads1261_t *device, int32_t *value);

/* Read register */
esp_err_t ads1261_read_register(ads1261_t *device, uint8_t reg, uint8_t *value);

/* Write register */
esp_err_t ads1261_write_register(ads1261_t *device, uint8_t reg, uint8_t value);

/* Deinitialize */
void ads1261_deinit(ads1261_t *device);

#ifdef __cplusplus
}
#endif

#endif // ADS1261_H
