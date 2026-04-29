//
// Created by 成雷 on 2026/4/29.
//

#include "mqtt_cmd.h"
#include "esp_log.h"
#include "mqtt_wrapper.h"
#include "mux_control.h"
#include "poll_task.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define TAG "MQTT_CMD"

#define MQTT_TOPIC_CTRL     "uart/control"
#define MQTT_TOPIC_TIMER    "uart/timer"
#define MQTT_TOPIC_STATUS   "uart/status"
#define MQTT_TOPIC_QUERY    "uart/status/query"
#define MQTT_TOPIC_UART_MODE "uart/mode"

// 预定义数据类型
// static const uint8_t data_hello[] = "Hello RS485!\n";
// static const uint8_t data_sensor[] = "SENSOR:TEMP:25.5,HUM:60.2\n";
// static const uint8_t data_cmd[] = "CMD:DEVICE:ON,MODE:1\n";
// static const uint8_t data_custom[] = "CUSTOM:DATA:12345678\n";

uart_mode_t current_uart_mode = UART_MODE_RS485;

// ==================== 状态上报 ====================
void mqtt_cmd_report_status(void)
{
    char status_msg[128];
    const char *mode_str = (current_uart_mode == UART_MODE_RS485) ? "RS485" : "RS232";
    mux_type_t mux_type = (current_uart_mode == UART_MODE_RS485) ? MUX_TYPE_RS485 : MUX_TYPE_RS232;

    snprintf(status_msg, sizeof(status_msg),
             "STATUS:UART=%s,CH=%d,TIMER=%s,INT=%lu",
             mode_str, mux_get_channel(mux_type),
             timer_send_enabled ? "RUNNING" : "STOPPED",
             timer_send_interval);
    ESP_LOGI(TAG, "状态: %s", status_msg);
    mqtt_wrapper_publish(MQTT_TOPIC_STATUS, status_msg, strlen(status_msg));
}

// ==================== MQTT 回调 ====================
static void mqtt_data_callback(const char *topic, const char *data, size_t len)
{
    if (strcmp(topic, MQTT_TOPIC_CTRL) == 0) {
        // 控制命令处理...
        // 原有 mqtt_ctrl_callback 内容
    } else if (strcmp(topic, MQTT_TOPIC_TIMER) == 0) {
        if (len > 0) {
            char cmd = data[0];
            if (cmd == '4') {
                timer_send_enabled = true;
                ESP_LOGI(TAG, "定时发送已启动");
            } else if (cmd == '5') {
                timer_send_enabled = false;
                ESP_LOGI(TAG, "定时发送已停止");
            } else if (cmd == '6' && len > 2 && data[1] == ',') {
                uint32_t interval = (uint32_t)atoi(data + 2);
                if (interval > 100 && interval < 60000) {
                    timer_send_interval = interval;
                    ESP_LOGI(TAG, "间隔已设置 %lums", interval);
                }
            }
        }
    } else if (strcmp(topic, MQTT_TOPIC_QUERY) == 0) {
        mqtt_cmd_report_status();
    } else if (strcmp(topic, MQTT_TOPIC_UART_MODE) == 0) {
        if (len > 0) {
            if (data[0] == 'R' || data[0] == 'r') {
                current_uart_mode = UART_MODE_RS485;
                ESP_LOGI(TAG, "已切换到 RS485");
            } else if (data[0] == 'S' || data[0] == 's') {
                current_uart_mode = UART_MODE_RS232;
                ESP_LOGI(TAG, "已切换到 RS232");
            }
        }
    }
}

// ==================== 初始化 ====================
void mqtt_cmd_init(void)
{
    mqtt_wrapper_set_callback(mqtt_data_callback);
    mqtt_wrapper_subscribe(MQTT_TOPIC_CTRL, 0);
    mqtt_wrapper_subscribe(MQTT_TOPIC_TIMER, 0);
    mqtt_wrapper_subscribe(MQTT_TOPIC_QUERY, 0);
    mqtt_wrapper_subscribe(MQTT_TOPIC_UART_MODE, 0);
    ESP_LOGI(TAG, "MQTT 命令初始化完成");
}
