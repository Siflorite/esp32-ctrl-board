#pragma once

#include <AccelStepper.h>
#include <Arduino.h>
#include <array>
#include "ble_manager.hpp"
#include "constants.hpp"
#include <FastLED.h>
#include <memory>
#include "types.hpp"

class CtrlBoardManager {
private:
    AccelStepper* stepper;
    AccelStepper* stepper_pp;

    // 注射泵与蠕动泵电机速度，单位为步/秒
    float syringe_speed;
    float peristaltic_speed;

    bool syringe_status;
    bool peristaltic_status;

    // 旋转阀当前通道，关闭时为0，开始时范围为1~6
    unsigned char switch_channel;

    // 电磁阀状态，一共8位，就是8bit数据，用unsigned char保存即可
    // 25.11.25 review: 笑嘻了，半年前居然无意间自己实现了个vector<bool>
    unsigned char solenoid_valve_status;

    // 比例阀压强记录
    int max_pressure;
    int cur_pressure;

    // WS2812光源
    std::array<CRGB, NUM_LEDS> leds;
    uint8_t brightness;
    bool light_status;

    // 蓝牙相关, init后才会初始化！
    std::unique_ptr<BLEManager> ble_manager;
public:
    CtrlBoardManager(AccelStepper* sp = nullptr, AccelStepper* pp = nullptr);
    ~CtrlBoardManager();

    void init();

    void setSyringeSpeed(float speed, bool b_volume_speed = false);
    void setPeristalticSpeed(float speed, bool b_volume_speed = false);
    void moveMm(float mm);
    void ppMoveRounds(float rounds);
    void syrineFinetune(const SyringeFinetuneType& type);
    void stopSyringe();
    void stopPeristaltic();
    void maintainMotor();

    void solenoidToggleChannel(int channel, bool status);

    void updatePressure(); 

    void shutLED();
    void updateLED();

    void procInstruction(std::string_view instruction);
};