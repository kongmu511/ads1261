/**
 * @file uart_cmd.c
 * @brief UART command interface implementation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "esp_log.h"
#include "uart_cmd.h"

#define CMD_BUFFER_SIZE 256
#define MAX_ARGS 10

static loadcell_t *g_device = NULL;
static char cmd_buffer[CMD_BUFFER_SIZE] = {0};
static uint16_t cmd_index = 0;

/* ============================================================================
 * Command Handlers
 * ============================================================================ */

static void cmd_help(int argc, char *argv[])
{
    uart_cmd_print_help();
}

static void cmd_status(int argc, char *argv[])
{
    if (!g_device) {
        printf("Device not initialized\n");
        return;
    }

    printf("\n=== Loadcell Status ===\n");
    printf("Frame count: %lu\n", g_device->frame_count);

    const char *states[] = {
        "UNCALIBRATED",
        "TARE_READY",
        "TARE_DONE",
        "SPAN_READY",
        "CALIBRATED"
    };

    for (int i = 0; i < 4; i++) {
        printf("Channel %d: %s\n", i + 1, states[g_device->channels[i].calib_state]);
    }
    printf("=======================\n\n");
}

static void cmd_read(int argc, char *argv[])
{
    if (!g_device) {
        printf("Device not initialized\n");
        return;
    }

    esp_err_t ret = loadcell_read(g_device);
    if (ret != ESP_OK) {
        printf("Failed to read loadcells\n");
        return;
    }

    loadcell_print_measurements(g_device);
}

static void cmd_tare(int argc, char *argv[])
{
    if (!g_device) {
        printf("Device not initialized\n");
        return;
    }

    if (argc < 2) {
        printf("Usage: tare <channel> [samples]\n");
        printf("  channel: 1-4 (or 0 for all)\n");
        printf("  samples: number of samples to average (default: 200)\n");
        return;
    }

    int channel = atoi(argv[1]);
    uint32_t samples = (argc > 2) ? atoi(argv[2]) : 200;

    if (channel < 0 || channel > 4) {
        printf("Invalid channel: %d\n", channel);
        return;
    }

    if (samples == 0) {
        samples = 200;
    }

    if (channel == 0) {
        /* Tare all channels */
        for (int i = 0; i < 4; i++) {
            printf("Taring channel %d...\n", i + 1);
            esp_err_t ret = loadcell_tare(g_device, i, samples);
            if (ret != ESP_OK) {
                printf("Failed to tare channel %d\n", i + 1);
            }
        }
    } else {
        printf("Taring channel %d with %lu samples...\n", channel, samples);
        esp_err_t ret = loadcell_tare(g_device, channel - 1, samples);
        if (ret == ESP_OK) {
            printf("Tare successful!\n");
        } else {
            printf("Tare failed!\n");
        }
    }
}

static void cmd_calibrate(int argc, char *argv[])
{
    if (!g_device) {
        printf("Device not initialized\n");
        return;
    }

    if (argc < 3) {
        printf("Usage: cal <channel> <known_force_N> [samples]\n");
        printf("  channel: 1-4\n");
        printf("  known_force_N: reference force in Newtons\n");
        printf("  samples: number of samples to average (default: 200)\n");
        printf("\nExample: cal 1 100.5\n");
        printf("  Calibrate channel 1 with 100.5 N reference weight\n");
        return;
    }

    int channel = atoi(argv[1]);
    float force = atof(argv[2]);
    uint32_t samples = (argc > 3) ? atoi(argv[3]) : 200;

    if (channel < 1 || channel > 4) {
        printf("Invalid channel: %d\n", channel);
        return;
    }

    if (fabs(force) < 0.1) {
        printf("Force value too small: %.2f N\n", force);
        return;
    }

    if (samples == 0) {
        samples = 200;
    }

    printf("Calibrating channel %d with %.2f N (using %lu samples)...\n", channel, force, samples);
    printf("Make sure the known weight is applied to the loadcell!\n");

    esp_err_t ret = loadcell_calibrate(g_device, channel - 1, force, samples);
    if (ret == ESP_OK) {
        printf("Calibration successful!\n");
    } else {
        printf("Calibration failed!\n");
    }
}

static void cmd_stats(int argc, char *argv[])
{
    if (!g_device) {
        printf("Device not initialized\n");
        return;
    }

    printf("\n=== Channel Statistics ===\n");

    for (int i = 0; i < 4; i++) {
        loadcell_stats_t stats;
        loadcell_get_stats(g_device, i, &stats);

        printf("Channel %d:\n", i + 1);
        printf("  Min:   %.2f N\n", stats.min_force);
        printf("  Max:   %.2f N\n", stats.max_force);
        printf("  Avg:   %.2f N\n", stats.avg_force);
        printf("  Count: %lu\n", stats.sample_count);
    }

    printf("===========================\n\n");
}

static void cmd_reset_stats(int argc, char *argv[])
{
    if (!g_device) {
        printf("Device not initialized\n");
        return;
    }

    if (argc < 2) {
        printf("Usage: rst_stats <channel>\n");
        printf("  channel: 1-4 (or 0 for all)\n");
        return;
    }

    int channel = atoi(argv[1]);

    if (channel < 0 || channel > 4) {
        printf("Invalid channel: %d\n", channel);
        return;
    }

    if (channel == 0) {
        loadcell_reset_stats(g_device, 4);
        printf("Statistics reset for all channels\n");
    } else {
        loadcell_reset_stats(g_device, channel - 1);
        printf("Statistics reset for channel %d\n", channel);
    }
}

static void cmd_raw(int argc, char *argv[])
{
    if (!g_device) {
        printf("Device not initialized\n");
        return;
    }

    printf("\n=== Raw ADC Values ===\n");

    for (int i = 0; i < 4; i++) {
        loadcell_measurement_t m;
        loadcell_get_measurement(g_device, i, &m);

        printf("Channel %d:\n", i + 1);
        printf("  Raw (24-bit): 0x%06lx (%ld)\n", m.raw_adc & 0xFFFFFF, m.raw_adc);
        printf("  Normalized:  %.8f\n", m.normalized);
        printf("  Offset:      %ld\n", g_device->channels[i].offset_raw);
        printf("  Scale:       %.6f\n", g_device->channels[i].scale_factor);
    }

    printf("======================\n\n");
}

static void cmd_info(int argc, char *argv[])
{
    if (!g_device) {
        printf("Device not initialized\n");
        return;
    }

    loadcell_print_calib_info(g_device);
}

static void cmd_diag(int argc, char *argv[])
{
    if (!g_device) {
        printf("Device not initialized\n");
        return;
    }

    printf("\n=== ADS1261 Diagnostic ===\n");
    esp_err_t ret = loadcell_diagnostic(g_device);
    if (ret == ESP_OK) {
        printf("Diagnostic completed successfully.\n");
    } else {
        printf("Diagnostic completed with errors.\n");
    }
    printf("=========================\n");
}

static void cmd_reset_calib(int argc, char *argv[])
{
    if (!g_device) {
        printf("Device not initialized\n");
        return;
    }

    if (argc < 2) {
        printf("Usage: rst_calib <channel>\n");
        printf("  channel: 1-4 (or 0 for all)\n");
        return;
    }

    int channel = atoi(argv[1]);

    if (channel < 0 || channel > 4) {
        printf("Invalid channel: %d\n", channel);
        return;
    }

    if (channel == 0) {
        for (int i = 0; i < 4; i++) {
            loadcell_reset_calibration(g_device, i);
        }
        printf("Calibration reset for all channels\n");
    } else {
        loadcell_reset_calibration(g_device, channel - 1);
        printf("Calibration reset for channel %d\n", channel);
    }
}

/* ============================================================================
 * Command Table
 * ============================================================================ */

typedef struct {
    const char *cmd;
    void (*handler)(int argc, char *argv[]);
    const char *help;
} cmd_entry_t;

static const cmd_entry_t commands[] = {
    {"help",        cmd_help,         "Show this help message"},
    {"status",      cmd_status,       "Show current status"},
    {"read",        cmd_read,         "Read all channels once"},
    {"tare",        cmd_tare,         "Tare (zero) calibration - usage: tare <ch> [samples]"},
    {"cal",         cmd_calibrate,    "Full-scale calibration - usage: cal <ch> <force_N> [samples]"},
    {"stats",       cmd_stats,        "Show channel statistics"},
    {"raw",         cmd_raw,          "Show raw ADC values"},
    {"info",        cmd_info,         "Show calibration info"},
    {"diag",        cmd_diag,         "Hardware diagnostic - check pin connections"},
    {"rst_stats",   cmd_reset_stats,  "Reset statistics - usage: rst_stats <ch>"},
    {"rst_calib",   cmd_reset_calib,  "Reset calibration - usage: rst_calib <ch>"},
    {NULL, NULL, NULL}
};

/* ============================================================================
 * Command Processing
 * ============================================================================ */

static void parse_and_execute(char *line)
{
    /* Skip leading whitespace */
    while (*line && (*line == ' ' || *line == '\t')) {
        line++;
    }

    if (*line == '\0' || *line == '\n' || *line == '#') {
        return;  /* Empty line or comment */
    }

    /* Parse arguments */
    char *argv[MAX_ARGS];
    int argc = 0;

    char *token = strtok(line, " \t\n");
    while (token && argc < MAX_ARGS) {
        argv[argc++] = token;
        token = strtok(NULL, " \t\n");
    }

    if (argc == 0) {
        return;
    }

    /* Find and execute command */
    for (int i = 0; commands[i].cmd != NULL; i++) {
        if (strcmp(argv[0], commands[i].cmd) == 0) {
            commands[i].handler(argc, argv);
            return;
        }
    }

    printf("Unknown command: %s\n", argv[0]);
    printf("Type 'help' for available commands\n");
}

/* ============================================================================
 * UART Interface
 * ============================================================================ */

esp_err_t uart_cmd_init(loadcell_t *device)
{
    g_device = device;
    cmd_index = 0;

    printf("\n");
    printf("╔════════════════════════════════════════╗\n");
    printf("║  GRF Force Platform - UART Interface  ║\n");
    printf("║  Type 'help' for commands             ║\n");
    printf("╚════════════════════════════════════════╝\n\n");

    return ESP_OK;
}

esp_err_t uart_cmd_process(void)
{
    /* Check for incoming data */
    int c = getchar();
    if (c == EOF || c == 0) {
        return ESP_FAIL;
    }

    /* Handle backspace */
    if (c == '\b' || c == 0x7F) {
        if (cmd_index > 0) {
            cmd_index--;
            printf("\b \b");  /* Visual feedback */
        }
        return ESP_OK;
    }

    /* Handle newline */
    if (c == '\n' || c == '\r') {
        printf("\n");

        if (cmd_index > 0) {
            cmd_buffer[cmd_index] = '\0';
            parse_and_execute(cmd_buffer);
            cmd_index = 0;
        }

        printf("> ");
        fflush(stdout);
        return ESP_OK;
    }

    /* Printable character */
    if (c >= 32 && c <= 126) {
        if (cmd_index < CMD_BUFFER_SIZE - 1) {
            cmd_buffer[cmd_index++] = c;
            printf("%c", c);  /* Echo */
            fflush(stdout);
        }
        return ESP_OK;
    }

    return ESP_FAIL;
}

void uart_cmd_print_help(void)
{
    printf("\n╔════════════════════════════════════════╗\n");
    printf("║         Available Commands            ║\n");
    printf("╚════════════════════════════════════════╝\n\n");

    for (int i = 0; commands[i].cmd != NULL; i++) {
        printf("%-12s - %s\n", commands[i].cmd, commands[i].help);
    }

    printf("\n");
    printf("CALIBRATION WORKFLOW:\n");
    printf("  1. tare 1 500     - Zero calibration (channel 1, 500 samples)\n");
    printf("  2. cal 1 100.5    - Span calibration (channel 1, 100.5 N reference)\n");
    printf("  3. read           - Verify calibration\n");
    printf("\nCALIBRATION COMMANDS:\n");
    printf("  tare <ch> [samples]       - Tare calibration (ch: 1-4 or 0 for all)\n");
    printf("  cal <ch> <force> [samples] - Full-scale calibration\n");
    printf("  rst_calib <ch>            - Reset calibration (ch: 1-4 or 0 for all)\n");
    printf("\nMEASUREMENT COMMANDS:\n");
    printf("  read              - Read all channels once\n");
    printf("  status            - Show device status\n");
    printf("  stats             - Show channel statistics\n");
    printf("  raw               - Show raw ADC values\n");
    printf("  info              - Show calibration info\n");
    printf("\nUTILITY COMMANDS:\n");
    printf("  rst_stats <ch>    - Reset statistics (ch: 1-4 or 0 for all)\n");
    printf("  help              - Show this message\n");
    printf("\n");
}
