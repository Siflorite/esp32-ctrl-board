#pragma once

#include <string>
#include "NimBLEDevice.h"
#include <variant>

enum class SyringeFinetuneType : unsigned char {
    SPEED_UP = 0,
    SLOW_UP = 1,
    SLOW_DOWN = 2,
    SPEED_DOWN = 3
};

// 旋转阀指令类型
struct SwitchRaw { std::string raw_cmd; };
struct SwitchCheck {};
struct SwitchStatus {};
struct SwitchChannel { int channel; };
struct SwitchReset {};
using SwitchCommand = std::variant<SwitchRaw, SwitchCheck, SwitchStatus, SwitchChannel, SwitchReset>;

// 回调函数
using OnWriteCallBackFunction = std::function<void(NimBLECharacteristic*)>;