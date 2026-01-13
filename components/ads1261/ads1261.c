/* ADS1261 driver - clean implementation */
#include "ads1261.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include <machine/endian.h>

static const char *TAG = "ADS1261";
static volatile int s_drdy_isr_count = 0;

/* ADS1261 Commands */
#define ADS1261_CMD_RESET       0x06
#define ADS1261_CMD_START       0x08
#define ADS1261_CMD_STOP        0x0A
#define ADS1261_CMD_RDATA       0x12
#define ADS1261_CMD_RREG        0x20
#define ADS1261_CMD_WREG        0x40

#define ADS1261_TIMEOUT_MS      1000

/* Software SPI pin definitions - matching main.c */
#define MOSI_PIN 2
#define MISO_PIN 7
#define CLK_PIN  6


// Declare the ISR function once without IRAM_ATTR here to avoid conflicts
static void ads1261_drdy_isr(void *arg);

esp_err_t ads1261_init(ads1261_t *device, spi_host_device_t host, int cs_pin, int drdy_pin)
{
    ESP_LOGI(TAG, "=== ADS1261 INIT STARTING ===");
    
    if (!device) return ESP_ERR_INVALID_ARG;
    if (drdy_pin < -1 || drdy_pin >= 32) return ESP_ERR_INVALID_ARG;
    if (cs_pin < -1 || cs_pin >= 32) return ESP_ERR_INVALID_ARG;

    device->cs_pin = cs_pin;
    device->drdy_pin = drdy_pin;
    device->spi_handle = NULL;
    device->drdy_sem = NULL;

    // Test: Configure test GPIO to see if GPIO matrix is working
    gpio_config_t test_cfg = {
        .pin_bit_mask = (1ULL << 8),  // Test GPIO8
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&test_cfg);
    
    // Toggle test GPIO to verify GPIO matrix is responsive
    ESP_LOGI(TAG, "GPIO Matrix Test: Toggling GPIO8...");
    for (int i = 0; i < 10; i++) {
        gpio_set_level(8, i % 2);
        esp_rom_delay_us(100);
    }
    ESP_LOGI(TAG, "GPIO Matrix Test: Complete");

    if (drdy_pin >= 0) {
        gpio_config_t gpio_cfg = {
            .pin_bit_mask = (1ULL << drdy_pin),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,  // Initially disable interrupts
        };
        gpio_config(&gpio_cfg);
    }

    if (cs_pin >= 0) {
        gpio_config_t cs_cfg = {
            .pin_bit_mask = (1ULL << cs_pin),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        gpio_config(&cs_cfg);
        gpio_set_level(cs_pin, 1);  // Start with CS high
    }
    
    // Determine if we're using hardware CS (managed by SPI driver) or manual GPIO CS
    // When cs_pin = -1, CS is tied to ground and we let the SPI driver handle it (no manual control)
    // When cs_pin >= 0, we use that GPIO for manual CS control
    device->use_hw_cs = false;  /* Manual CS control */

    /* Add device to SPI bus */
    spi_device_interface_config_t dev_cfg = {
        .mode = 1,  /* SPI Mode 1: CPOL=0, CPHA=1 */
        .clock_speed_hz = 8 * 1000 * 1000,  /* 8 MHz */
        .spics_io_num = -1,  /* Manual CS via GPIO */
        .queue_size = 3,
    };

    esp_err_t err = spi_bus_add_device(host, &dev_cfg, &device->spi_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_add_device failed: %s", esp_err_to_name(err));
        return err;
    }

    /* Long delay to ensure GPIO matrix settles */
    esp_rom_delay_us(50000);  /* 50ms delay - matching Arduino delay after begin() */

    /* Send RESET command - matching Arduino timing (no delays) */
    ESP_LOGI(TAG, "Sending RESET command (0x06)...");
    uint8_t reset_cmd = ADS1261_CMD_RESET;
    spi_transaction_t reset_t = {
        .length = 8,
        .tx_buffer = &reset_cmd,
    };
    spi_device_polling_transmit(device->spi_handle, &reset_t);
    
    /* Small delay for device to complete reset */
    esp_rom_delay_us(10000);  // 10ms delay - matching Arduino

    /* Try to read ID register to verify communication */
    uint8_t id = 0;
    esp_err_t id_ret = ads1261_read_register(device, ADS1261_REG_ID, &id);
    
    if (id_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read ID register: %s", esp_err_to_name(id_ret));
        ESP_LOGE(TAG, "DEVICE NOT RESPONDING - Check SPI pins, CS connection, and power");
        return id_ret;
    }
    
    ESP_LOGI(TAG, "ADS1261 ID: 0x%02x", id);
    
    if (id != 0x08) {
        ESP_LOGW(TAG, "Unexpected ID (expected 0x08). Device may not be ADS1261 or communication issue.");
        ESP_LOGW(TAG, "Continuing anyway, but verify your hardware setup.");
    }

    /* Configure registers similar to Arduino initialization */
    /* Set PGA gain to 128 (same as Arduino code) */
    uint8_t pga_reg = 0x07;  // GAIN=128, BYPASS=0 (normal mode)
    esp_err_t pga_ret = ads1261_write_register(device, ADS1261_REG_PGA, pga_reg);
    if (pga_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set PGA register: %s", esp_err_to_name(pga_ret));
    } else {
        ESP_LOGI(TAG, "PGA register set to 0x%02x (gain=128)", pga_reg);
    }
    
    /* Set data rate to 40ksps to match project requirements */
    uint8_t mode0_reg = (ADS1261_REG_MODE0_FILTER_FIR << 5) | ADS1261_DR_40000_SPS;  // 40ksps
    esp_err_t mode0_ret = ads1261_write_register(device, ADS1261_REG_MODE0, mode0_reg);
    if (mode0_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set MODE0 register: %s", esp_err_to_name(mode0_ret));
    } else {
        ESP_LOGI(TAG, "MODE0 register set to 0x%02x (40ksps, FIR filter)", mode0_reg);
    }

    /* Configure MODE1 for continuous conversion */
    uint8_t mode1_reg = 0x00;  // CONVRT=0 (continuous conversion mode), others default
    esp_err_t mode1_ret = ads1261_write_register(device, ADS1261_REG_MODE1, mode1_reg);
    if (mode1_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set MODE1 register: %s", esp_err_to_name(mode1_ret));
    } else {
        ESP_LOGI(TAG, "MODE1 register set to 0x%02x (continuous conversion)", mode1_reg);
    }

    /* Configure MODE3 - for now, use default (SPITIM=0 / DRDY mode) */
    /* Do NOT set SPITIM=1 yet until basic communication works */
    ESP_LOGI(TAG, "MODE3 configuration: Using DRDY mode (SPITIM=0) for explicit data requests");
    
    /* Read MODE3 to verify we can communicate */
    uint8_t mode3 = 0;
    esp_err_t mode3_ret = ads1261_read_register(device, ADS1261_REG_MODE3, &mode3);
    if (mode3_ret == ESP_OK) {
        ESP_LOGI(TAG, "MODE3 read successful: 0x%02x", mode3);
    } else {
        ESP_LOGE(TAG, "Failed to read MODE3 register: %s", esp_err_to_name(mode3_ret));
        /* Don't fail init on this - device might still be functional */
    }

    /* Only set up DRDY interrupt if we're in DOUT/DRDY mode */
    uint8_t final_mode3 = 0;
    bool use_status_polling = true;  // Default to polling method
    if (ads1261_read_register(device, ADS1261_REG_MODE3, &final_mode3) == ESP_OK) {
        if (((final_mode3 >> 4) & 1) == 0) {  // SPITIM=0, meaning DRDY mode
            if (drdy_pin >= 0) {
                device->drdy_sem = xSemaphoreCreateBinary();
                if (device->drdy_sem == NULL) {
                    ESP_LOGW(TAG, "Failed to create DRDY semaphore; falling back to polling");
                } else {
                    esp_err_t isr_ret = gpio_install_isr_service(0);
                    if (isr_ret != ESP_OK && isr_ret != ESP_ERR_INVALID_STATE) {
                        ESP_LOGW(TAG, "gpio_install_isr_service failed: %s", esp_err_to_name(isr_ret));
                    }
                    esp_err_t intr_ret = gpio_set_intr_type(drdy_pin, GPIO_INTR_NEGEDGE);
                    if (intr_ret != ESP_OK) {
                        ESP_LOGW(TAG, "gpio_set_intr_type failed: %s", esp_err_to_name(intr_ret));
                    }
                    esp_err_t add_ret = gpio_isr_handler_add(drdy_pin, ads1261_drdy_isr, device);
                    if (add_ret != ESP_OK) {
                        ESP_LOGW(TAG, "gpio_isr_handler_add failed: %s — falling back to polling", esp_err_to_name(add_ret));
                        vSemaphoreDelete((SemaphoreHandle_t)device->drdy_sem);
                        device->drdy_sem = NULL;
                    } else {
                        ESP_LOGI(TAG, "DRDY ISR installed on GPIO %d", drdy_pin);
                        use_status_polling = false;  // Successfully set up interrupt
                    }
                }
            }
        }
    }

    if (use_status_polling) {
        ESP_LOGW(TAG, "Using STATUS register polling for data ready detection");
    }

    /* In Standalone DOUT mode, conversion is continuous - no START command needed */
    /* But we'll add a small delay to allow first conversion to complete */
    esp_rom_delay_us(50000);  // 50ms for first conversion to complete
    
    ESP_LOGI(TAG, "ADS1261 initialized successfully in Standalone DOUT mode");
    return ESP_OK;
}

// Define the ISR with IRAM_ATTR to avoid the conflict
static void IRAM_ATTR ads1261_drdy_isr(void *arg)
{
    ads1261_t *dev = (ads1261_t *)arg;
    if (!dev) return;
    if (dev->drdy_sem) {
        BaseType_t awakened = pdFALSE;
        xSemaphoreGiveFromISR((SemaphoreHandle_t)dev->drdy_sem, &awakened);
        s_drdy_isr_count++;
        portYIELD_FROM_ISR(awakened);
    }
}

esp_err_t ads1261_write_register(ads1261_t *device, uint8_t reg, uint8_t value)
{
    if (!device) return ESP_ERR_INVALID_ARG;

    uint8_t cmd = ADS1261_CMD_WREG | (reg & 0x1F);

    if (!device->use_hw_cs && device->cs_pin >= 0) {
        gpio_set_level(device->cs_pin, 0);  /* CS low */
    }

    /* Write register using individual byte transfers (like Arduino) */
    spi_transaction_t t1 = {
        .length = 8,
        .tx_buffer = &cmd,
    };
    esp_err_t ret = spi_device_polling_transmit(device->spi_handle, &t1);
    if (ret != ESP_OK) {
        goto cleanup;
    }
    
    spi_transaction_t t2 = {
        .length = 8,
        .tx_buffer = &value,
    };
    ret = spi_device_polling_transmit(device->spi_handle, &t2);
    if (ret != ESP_OK) {
        goto cleanup;
    }

cleanup:
    if (!device->use_hw_cs && device->cs_pin >= 0) {
        gpio_set_level(device->cs_pin, 1);  /* CS high */
    }
    
    ESP_LOGI(TAG, "WriteReg 0x%02X: value=0x%02X", reg, value);

    return ret;
}

esp_err_t ads1261_read_register(ads1261_t *device, uint8_t reg, uint8_t *value)
{
    if (!device || !value) return ESP_ERR_INVALID_ARG;

    uint8_t cmd = ADS1261_CMD_RREG | (reg & 0x1F);

    if (!device->use_hw_cs && device->cs_pin >= 0) {
        gpio_set_level(device->cs_pin, 0);  /* CS low */
    }

    /* Read register using individual byte transfers (like Arduino) */
    spi_transaction_t t1 = {
        .length = 8,
        .tx_buffer = &cmd,
    };
    esp_err_t ret = spi_device_polling_transmit(device->spi_handle, &t1);
    if (ret != ESP_OK) {
        goto cleanup;
    }
    
    /* Dummy byte */
    uint8_t dummy = 0x00;
    spi_transaction_t t2 = {
        .length = 8,
        .tx_buffer = &dummy,
    };
    ret = spi_device_polling_transmit(device->spi_handle, &t2);
    if (ret != ESP_OK) {
        goto cleanup;
    }
    
    /* Read data byte */
    uint8_t rx_buf[1] = {0};
    spi_transaction_t t3 = {
        .length = 8,
        .rx_buffer = rx_buf,
    };
    ret = spi_device_polling_transmit(device->spi_handle, &t3);
    if (ret != ESP_OK) {
        goto cleanup;
    }
    
    *value = rx_buf[0];

cleanup:
    if (!device->use_hw_cs && device->cs_pin >= 0) {
        gpio_set_level(device->cs_pin, 1);  /* CS high */
    }

    return ret;
}

esp_err_t ads1261_set_mux(ads1261_t *device, uint8_t muxp, uint8_t muxn)
{
    uint8_t inpmux = ((muxp & 0x0F) << 4) | (muxn & 0x0F);
    return ads1261_write_register(device, ADS1261_REG_INPMUX, inpmux);
}

esp_err_t ads1261_set_pga(ads1261_t *device, uint8_t gain)
{
    return ads1261_write_register(device, ADS1261_REG_PGA, ((gain & 0x07) << 4) | 0x01);
}

esp_err_t ads1261_set_datarate(ads1261_t *device, uint8_t datarate)
{
    /* Data rate is set in MODE0 register bits [3:0] */
    return ads1261_write_register(device, ADS1261_REG_MODE0, datarate & 0x1F);
}

esp_err_t ads1261_set_ref(ads1261_t *device, uint8_t refsel)
{
    return ads1261_write_register(device, ADS1261_REG_REF, ((refsel & 0x03) << 5));
}

esp_err_t ads1261_start_conversion(ads1261_t *device)
{
    if (!device) return ESP_ERR_INVALID_ARG;
    if (!device->use_hw_cs && device->cs_pin >= 0) gpio_set_level(device->cs_pin, 0);
    esp_rom_delay_us(10);  /* More precise delay */

    uint8_t cmd = ADS1261_CMD_START;
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &cmd,
    };
    esp_err_t ret = spi_device_transmit(device->spi_handle, &t);

    esp_rom_delay_us(10);  /* More precise delay */
    if (!device->use_hw_cs && device->cs_pin >= 0) gpio_set_level(device->cs_pin, 1);
    return ret;
}

esp_err_t ads1261_read_adc(ads1261_t *device, int32_t *result)
{
    if (!device || !result) return ESP_ERR_INVALID_ARG;

    if (!device->use_hw_cs && device->cs_pin >= 0) {
        gpio_set_level(device->cs_pin, 0);  /* CS low */
    }

    /* Read ADC using individual byte transfers (like Arduino) */
    uint8_t cmd = ADS1261_CMD_RDATA;
    spi_transaction_t t1 = {
        .length = 8,
        .tx_buffer = &cmd,
    };
    esp_err_t ret = spi_device_polling_transmit(device->spi_handle, &t1);
    if (ret != ESP_OK) {
        goto cleanup;
    }
    
    /* Read 3 data bytes individually */
    uint8_t data_bytes[3];
    uint8_t dummy = 0x00;
    
    for (int i = 0; i < 3; i++) {
        spi_transaction_t t_data = {
            .length = 8,
            .tx_buffer = &dummy,
            .rx_buffer = &data_bytes[i],
        };
        ret = spi_device_polling_transmit(device->spi_handle, &t_data);
        if (ret != ESP_OK) {
            goto cleanup;
        }
    }
    
    if (!device->use_hw_cs && device->cs_pin >= 0) {
        gpio_set_level(device->cs_pin, 1);  /* CS high */
    }

    /* Assemble 24-bit result from 3 bytes (big-endian) */
    uint32_t raw_value = ((uint32_t)data_bytes[0] << 16) | ((uint32_t)data_bytes[1] << 8) | data_bytes[2];
    
    /* Convert to signed 24-bit value */
    if (raw_value & 0x800000) {
        *result = (int32_t)(raw_value - 0x1000000);
    } else {
        *result = (int32_t)raw_value;
    }
    
    ESP_LOGI(TAG, "SPI: RDATA data=[%02X %02X %02X]", data_bytes[0], data_bytes[1], data_bytes[2]);

    return ESP_OK;

cleanup:
    if (!device->use_hw_cs && device->cs_pin >= 0) {
        gpio_set_level(device->cs_pin, 1);  /* CS high */
    }
    
    return ret;
}

void ads1261_deinit(ads1261_t *device)
{
    if (!device) return;
    if (device->drdy_pin >= 0) gpio_isr_handler_remove(device->drdy_pin);
    if (device->drdy_sem) { vSemaphoreDelete((SemaphoreHandle_t)device->drdy_sem); device->drdy_sem = NULL; }
    if (device->spi_handle) { spi_bus_remove_device(device->spi_handle); device->spi_handle = NULL; }
}

