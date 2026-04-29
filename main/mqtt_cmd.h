//
// Created by 成雷 on 2026/4/29.
//

#ifndef ESP32CHUNENG_MQTT_CMD_H
#define ESP32CHUNENG_MQTT_CMD_H

#include <stdint.h>
#include <stdbool.h>

// 当前串口模式
typedef enum {
    UART_MODE_RS485 = 0,
    UART_MODE_RS232 = 1,
} uart_mode_t;

extern uart_mode_t current_uart_mode;

// 获取/设置串口模式
uart_mode_t mqtt_cmd_get_uart_mode(void);
void mqtt_cmd_set_uart_mode(uart_mode_t mode);

// 状态查询
void mqtt_cmd_report_status(void);

// 定时器控制
void mqtt_cmd_set_timer_enabled(bool enabled);
void mqtt_cmd_set_timer_interval(uint32_t interval_ms);
bool mqtt_cmd_get_timer_enabled(void);
uint32_t mqtt_cmd_get_timer_interval(void);

// 初始化
void mqtt_cmd_init(void);


#endif //ESP32CHUNENG_MQTT_CMD_H