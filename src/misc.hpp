#pragma once

#include <AccelStepper.h>
#include <Arduino.h>
#include "ctrl_board_manager.hpp"
#include <string_view>
#include "types.hpp"

// Helper functions:
constexpr int hexToNibble(const char c);
bool hexStringToBytes(std::string_view sv, unsigned char* output);
bool binStringToBytes(std::string_view sv, unsigned char* output);

void transmit485(const uint8_t* data, size_t len = 8);
void procSwitchData(const SwitchCommand& command);

void transmit595(uint8_t data);
void showSolenoidStatus(const unsigned char& status);

void writeDAC(int data);

void procSerialCommand(CtrlBoardManager& manager);

// Printers:
void printSyringeInstr();
void printPeristalticInstr();
void printSwitchInstr();
void printSolenoidInstr();
void printProportionInstr();
void printLightInstr();

// Template functions

/**
 * 将蓝牙特征值数据转换为指定类型
 * 
 * 返回值不可丢弃
 * @param T 要转换到的类型，支持std::string, bool和数值类型
 * @param data 指向数据的指针
 * @param length 数据长度
 * @return 转换后的值
 */
template<typename T>
[[nodiscard]] T convertValue(const uint8_t* data, const uint16_t length) {
    if (!data || length == 0) {
        return T{};
    }

    // 本来想提供C-sytle的字符串的（char*），但是没有安全实现，自己玩得了

    if constexpr (std::is_same_v<T, std::string>) {
        // std::string类型
        return std::string(reinterpret_cast<const char*>(data), length);
    } else if constexpr (std::is_same_v<T, bool>) {
        // bool类型
        return (data[0] != 0);
    } else if constexpr (std::is_arithmetic_v<T>) {
        // 整形或浮点
        // 长度不够就取0
        // 默认小端序
        T value = 0;
        std::memcpy(&value, data, sizeof(T));
        return value;
    } else {
        static_assert(sizeof(T) == 0, "Unsupported type for BLE value conversion");
        return T{};
    }
}

/**
 * 将蓝牙特征值数据转换为指定类型
 * 
 * 返回值不可丢弃
 * @param T 要转换到的类型，支持`std::string`, `bool`和数值类型
 * @param value 一个`const NimBLEAttValue&`特征值
 * @return 转换后的值
 */
template<typename T>
[[nodiscard]] T convertAttrValue(const NimBLEAttValue& value) {
    auto data = value.data();
    const auto length = value.length();
    return convertValue<T>(data, length);
}