/**
 * @file uart_cmd.h
 * @brief UART command interface for loadcell control
 * 
 * Provides interactive command-line interface for:
 * - Real-time measurement readout
 * - Tare calibration
 * - Full-scale calibration
 * - Statistics display
 * - Configuration commands
 */

#ifndef UART_CMD_H
#define UART_CMD_H

#include "esp_err.h"
#include "loadcell.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize UART command interface
 * 
 * @param[in] device Loadcell device handle
 * @return ESP_OK on success
 */
esp_err_t uart_cmd_init(loadcell_t *device);

/**
 * Process incoming UART command
 * Called from main loop or UART task
 * 
 * @return ESP_OK if command processed, ESP_FAIL if no command waiting
 */
esp_err_t uart_cmd_process(void);

/**
 * Print command help
 */
void uart_cmd_print_help(void);

#ifdef __cplusplus
}
#endif

#endif /* UART_CMD_H */
