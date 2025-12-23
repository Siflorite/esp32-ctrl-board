#pragma once
#include <Arduino.h>
#include <array>

// 引脚定义
// 注射泵(motor1)
constexpr uint8_t STEP_PIN = 8;
constexpr uint8_t DIR_PIN = 9;
constexpr uint8_t EN_PIN = 12;

// 蠕动泵(motor2)
constexpr uint8_t P_STEP = 10;
constexpr uint8_t P_DIR = 11;
constexpr uint8_t P_EN = 13;

// 连接切换阀的UART1
constexpr uint8_t RX_485 = 2;
constexpr uint8_t TX_485 = 1;

// 第二组74HC595
constexpr uint8_t DS = 14;
constexpr uint8_t SHCP = 42;
constexpr uint8_t STCP = 41;

// 第一组74HC595
// constexpr uint8_t DS = 4;
// constexpr uint8_t SHCP = 5;
// constexpr uint8_t STCP = 6;

// 第一组DAC
// constexpr uint8_t DAC_ADDR  = 0x60;
// constexpr uint8_t SDA       = 15;
// constexpr uint8_t SCL       = 16;

// 第二组DAC
constexpr uint8_t DAC_ADDR = 0x61;
constexpr uint8_t SDA_PIN = 15;
constexpr uint8_t SCL_PIN = 16;
// constexpr uint8_t SDA      = 17;
// constexpr uint8_t SCL      = 18;

// WS2812阵列数据
constexpr uint8_t WS_IN = 7;

// 运动参数
constexpr float SCREW_PITCH = 0.8; // M5螺纹，螺距0.8mm/转
constexpr int STEPS_PER_REV = 200;  // 电机步数/转
constexpr int MICROSTEPS_1 = 64; // 电机1微步数为64
constexpr int MICROSTEPS_2 = 8; // 电机2微步数为8
constexpr float V2D_RATIO = 3.51; // 1mL液体->运动3.51mm
constexpr float V2R_RATIO = 9.524; // 1转 -> 0.1873mL => 1mL -> 5.339转 // 50r->5.25ml
// 注射泵微调，快速：0.5mL/s，慢速：0.05mL/s; 快速顺便用作最大限制速度
constexpr float SYRINGE_MAXIMUM_SPEED = 0.5;
constexpr float FINETUNE_FAST = SYRINGE_MAXIMUM_SPEED * V2D_RATIO / SCREW_PITCH * STEPS_PER_REV * MICROSTEPS_1;
constexpr float FINETUNE_SLOW = 0.05 * V2D_RATIO / SCREW_PITCH * STEPS_PER_REV * MICROSTEPS_1;
// 蠕动泵最快速度设置为0.5mL/s，等效0.5*9.524*8*200=7619.2微步/s
constexpr float PERISTALTIC_MAXIMUM_SPEED = 0.5;
constexpr float PERISTALTIC_MAXIMUM_MICROSTEP = PERISTALTIC_MAXIMUM_SPEED * V2R_RATIO *STEPS_PER_REV *MICROSTEPS_2;

// 485模块指令长度，默认为8byte
constexpr int INSTR_485_LEN = 8;

constexpr long INTERVAL = 50; // 间隔时间(毫秒)
constexpr int NUM_LEDS = 64; // WS2812 LED数量
// LED中心4*4阵列编号
constexpr auto LED_ARR = []() {
    std::array<int, 16> arr{};
    for(int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            arr[i * 4 + j] = (i + 2) * 8 + (j + 2);
        }
    }
    return arr;
};

// 蓝牙相关
constexpr uint16_t SERVICE_UUID = 0xABCD;
constexpr uint16_t WRITE_CHARA_UUID = 0x1234;
constexpr uint16_t NOTIF_CHARA_UUID = 0x5678;