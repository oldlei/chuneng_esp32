//
// Created by 成雷 on 2026/4/29.
//
#include "poll_task.h"
#include "esp_log.h"
#include "mux_control.h"
#include "uart_config.h"
#include <string.h>
#include <stdio.h>

#define TAG "POLL_TASK"
#define RESPONSE_TIMEOUT_MS 500

// ==================== 轮询命令定义 ====================
static const uint8_t poll_cmd_1[] = {0x00, 0x04, 0x17, 0x00, 0x00, 0x33, 0xB4, 0x7A};
static const uint8_t poll_cmd_2[] = {0x00, 0x04, 0x10, 0x00, 0x00, 0x12, 0x75, 0x16};
static const uint8_t poll_cmd_3[] = {0x00, 0x04, 0x11, 0x00, 0x00, 0x1A, 0x75, 0x2C};
static const uint8_t poll_cmd_4[] = {0x00, 0x01, 0x12, 0x00, 0x00, 0x90, 0x38, 0xCF};

const uint8_t* const poll_commands[POLL_CMD_COUNT] = {
    poll_cmd_1, poll_cmd_2, poll_cmd_3, poll_cmd_4,
};
const uint16_t poll_cmd_lengths[POLL_CMD_COUNT] = {
    sizeof(poll_cmd_1), sizeof(poll_cmd_2), sizeof(poll_cmd_3), sizeof(poll_cmd_4),
};

// 定时发送控制
volatile bool timer_send_enabled = false;
uint32_t timer_send_interval = 1000;

// ==================== UART 安全操作宏 ====================
#define rs485_write_safe(data, len) ({ \
    int _r = -1; \
    uart_instance_t *_inst = uart_get_rs485(); \
    if (_inst->mutex && xSemaphoreTake(_inst->mutex, pdMS_TO_TICKS(100)) == pdTRUE) { \
        _r = _inst->write((data), (len)); \
        xSemaphoreGive(_inst->mutex); \
    } \
    _r; \
})

#define rs485_read_safe(data, len, to) ({ \
    int _r = -1; \
    uart_instance_t *_inst = uart_get_rs485(); \
    if (_inst->mutex && xSemaphoreTake(_inst->mutex, pdMS_TO_TICKS(100)) == pdTRUE) { \
        _r = _inst->read((data), (len), (to)); \
        xSemaphoreGive(_inst->mutex); \
    } \
    _r; \
})

#define rs232_write_safe(data, len) ({ \
    int _r = -1; \
    uart_instance_t *_inst = uart_get_rs232(); \
    if (_inst->mutex && xSemaphoreTake(_inst->mutex, pdMS_TO_TICKS(100)) == pdTRUE) { \
        _r = _inst->write((data), (len)); \
        xSemaphoreGive(_inst->mutex); \
    } \
    _r; \
})

#define rs232_read_safe(data, len, to) ({ \
    int _r = -1; \
    uart_instance_t *_inst = uart_get_rs232(); \
    if (_inst->mutex && xSemaphoreTake(_inst->mutex, pdMS_TO_TICKS(100)) == pdTRUE) { \
        _r = _inst->read((data), (len), (to)); \
        xSemaphoreGive(_inst->mutex); \
    } \
    _r; \
})

// ==================== RS485 轮询任务 ====================
static void rs485_poll_task(void *pvParameters)
{
    uint8_t rx_buffer[256];
    uint8_t tx_buffer[16];

    while (1) {
        if (timer_send_enabled) {
            mux_channel_t ch = mux_next_channel(MUX_TYPE_RS485);
            ESP_LOGI(TAG, "RS485 轮询通道 %d", ch);

            for (int cmd_idx = 0; cmd_idx < POLL_CMD_COUNT; cmd_idx++) {
                memcpy(tx_buffer, poll_commands[cmd_idx], poll_cmd_lengths[cmd_idx]);
                tx_buffer[poll_cmd_lengths[cmd_idx]] = (uint8_t)ch;
                int tx_len = poll_cmd_lengths[cmd_idx] + 1;

                int sent_len = rs485_write_safe(tx_buffer, tx_len);
                if (sent_len > 0) {
                    ESP_LOGI(TAG, "RS485 CH%d 发送命令%d, 长度=%d", ch, cmd_idx + 1, sent_len);

                    int recv_len = rs485_read_safe(rx_buffer, sizeof(rx_buffer), RESPONSE_TIMEOUT_MS);
                    if (recv_len > 0) {
                        char hex_str[512];
                        int hex_pos = 0;
                        for (int i = 0; i < recv_len && hex_pos < (int)sizeof(hex_str) - 4; i++) {
                            hex_pos += snprintf(hex_str + hex_pos, sizeof(hex_str) - hex_pos, "%02X ", rx_buffer[i]);
                        }
                        ESP_LOGI(TAG, "RS485 CH%d 命令%d 收到 [%d]: %s", ch, cmd_idx + 1, recv_len, hex_str);
                    } else {
                        ESP_LOGW(TAG, "RS485 CH%d 命令%d 超时", ch, cmd_idx + 1);
                    }
                } else {
                    ESP_LOGW(TAG, "RS485 CH%d 命令%d 发送失败", ch, cmd_idx + 1);
                }
            }
            ESP_LOGI(TAG, "RS485 CH%d 完成，等待 %lums", ch, timer_send_interval);
            vTaskDelay(pdMS_TO_TICKS(timer_send_interval));
        } else {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

// ==================== RS232 轮询任务 ====================
static void rs232_poll_task(void *pvParameters)
{
    uint8_t rx_buffer[256];

    while (1) {
        if (timer_send_enabled) {
            mux_channel_t ch = mux_next_channel(MUX_TYPE_RS232);
            ESP_LOGI(TAG, "RS232 轮询通道 %d", ch);

            for (int cmd_idx = 0; cmd_idx < POLL_CMD_COUNT; cmd_idx++) {
                int sent_len = rs232_write_safe((uint8_t*)poll_commands[cmd_idx], poll_cmd_lengths[cmd_idx]);
                if (sent_len > 0) {
                    ESP_LOGI(TAG, "RS232 CH%d 发送命令%d, 长度=%d", ch, cmd_idx + 1, sent_len);

                    int recv_len = rs232_read_safe(rx_buffer, sizeof(rx_buffer), RESPONSE_TIMEOUT_MS);
                    if (recv_len > 0) {
                        ESP_LOGI(TAG, "RS232 CH%d 命令%d 收到 %d 字节", ch, cmd_idx + 1, recv_len);
                    } else {
                        ESP_LOGW(TAG, "RS232 CH%d 命令%d 超时", ch, cmd_idx + 1);
                    }
                } else {
                    ESP_LOGW(TAG, "RS232 CH%d 命令%d 发送失败", ch, cmd_idx + 1);
                }
            }
            ESP_LOGI(TAG, "RS232 CH%d 完成，等待 %lums", ch, timer_send_interval);
            vTaskDelay(pdMS_TO_TICKS(timer_send_interval));
        } else {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

// ==================== 初始化 ====================
void poll_task_init(void)
{
    xTaskCreate(rs485_poll_task, "rs485_poll", 4096, NULL, 5, NULL);
    xTaskCreate(rs232_poll_task, "rs232_poll", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "轮询任务已启动");
}
