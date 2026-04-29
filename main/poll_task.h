//
// Created by 成雷 on 2026/4/29.
//

#ifndef ESP32CHUNENG_POLL_TASK_H
#define ESP32CHUNENG_POLL_TASK_H

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"

// 轮询命令数量
#define POLL_CMD_COUNT 4

// 轮询命令（外部可访问）
extern const uint8_t* const poll_commands[POLL_CMD_COUNT];
extern const uint16_t poll_cmd_lengths[POLL_CMD_COUNT];

// 定时发送控制
extern volatile bool timer_send_enabled;
extern uint32_t timer_send_interval;

// 初始化
void poll_task_init(void);

#endif //ESP32CHUNENG_POLL_TASK_H