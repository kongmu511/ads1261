#ifndef ADS1261_H
#define ADS1261_H

#include <stdint.h>
#include <stdbool.h>
#include "driver/spi_master.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ADS1261 Register Addresses - Per datasheet */
#define ADS1261_REG_ID          0x00
#define ADS1261_REG_STATUS      0x01
#define ADS1261_REG_MODE0       0x02
#define ADS1261_REG_MODE1       0x03
#define ADS1261_REG_MODE2       0x04
#define ADS1261_REG_MODE3       0x05
#define ADS1261_REG_REF         0x06
#define ADS1261_REG_OFCAL0      0x07
#define ADS1261_REG_OFCAL1      0x08
#define ADS1261_REG_OFCAL2      0x09
#define ADS1261_REG_FSCAL0      0x0A
#define ADS1261_REG_FSCAL1      0x0B
#define ADS1261_REG_FSCAL2      0x0C
#define ADS1261_REG_IMUX        0x0D
#define ADS1261_REG_IMAG        0x0E
#define ADS1261_REG_RESERVED    0x0F
#define ADS1261_REG_PGA         0x10
#define ADS1261_REG_INPMUX      0x11
#define ADS1261_REG_INPBIAS     0x12

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

/* Data Rate Settings (in SPS - Samples Per Second) - Placed in MODE0[3:0] */
#define ADS1261_DR_2_5_SPS      0x00  /* 2.5 SPS */
#define ADS1261_DR_5_SPS        0x01  /* 5 SPS */
#define ADS1261_DR_10_SPS       0x02  /* 10 SPS */
#define ADS1261_DR_16_6_SPS     0x03  /* 16.6 SPS */
#define ADS1261_DR_20_SPS       0x04  /* 20 SPS (default) */
#define ADS1261_DR_50_SPS       0x05  /* 50 SPS */
#define ADS1261_DR_60_SPS       0x06  /* 60 SPS */
#define ADS1261_DR_100_SPS      0x07  /* 100 SPS */
#define ADS1261_DR_400_SPS      0x08  /* 400 SPS */
#define ADS1261_DR_1200_SPS     0x09  /* 1200 SPS */
#define ADS1261_DR_2400_SPS     0x0A  /* 2400 SPS */
#define ADS1261_DR_4800_SPS     0x0B  /* 4800 SPS */
#define ADS1261_DR_7200_SPS     0x0C  /* 7200 SPS */
#define ADS1261_DR_14400_SPS    0x0D  /* 14400 SPS */
#define ADS1261_DR_19200_SPS    0x0E  /* 19200 SPS */
#define ADS1261_DR_25600_SPS    0x0F  /* 25600 SPS */
#define ADS1261_DR_40000_SPS    0x10  /* 40000 SPS */
/* For compatibility */
#define ADS1261_DR_1000         ADS1261_DR_1200_SPS

/* Filter Settings - Placed in MODE0[7:5] */
#define ADS1261_REG_MODE0_FILTER_SINC1    0x00
#define ADS1261_REG_MODE0_FILTER_SINC2    0x01
#define ADS1261_REG_MODE0_FILTER_SINC3    0x02
#define ADS1261_REG_MODE0_FILTER_SINC4    0x03
#define ADS1261_REG_MODE0_FILTER_FIR      0x04  /* Default filter type */

/* Reference Selection */
#define ADS1261_REFSEL_INT      0x00
#define ADS1261_REFSEL_EXT1     0x01
#define ADS1261_REFSEL_EXT2     0x02

typedef struct {
    spi_device_handle_t spi_handle;
    int cs_pin;
    int drdy_pin;
    bool use_hw_cs;
    // internal SPI buffers (DMA-safe) used by driver transactions
    uint8_t tx_buf[4];
    uint8_t rx_buf[4];
    void *drdy_sem; /* opaque pointer to FreeRTOS semaphore (created in driver) */
} ads1261_t;

/* Initialize ADS1261 */
esp_err_t ads1261_init(ads1261_t *device, spi_host_device_t host, int cs_pin, int drdy_pin);

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
