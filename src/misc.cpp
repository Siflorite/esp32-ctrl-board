#include "misc.hpp"

#include "constants.hpp"
#include "types.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>
#include <Wire.h>
#include <sys/_types.h>
#include <type_traits>
#include <variant>

constexpr int hexToNibble(const char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

bool hexStringToBytes(std::string_view sv, unsigned char* output) {
    const int len = sv.length();

    for (int i = 0; i < len / 2; i++) {
        const int high = hexToNibble(sv[i*2]);
        const int low = hexToNibble(sv[i*2+1]);
        if (high == -1 || low == -1) return false;
        output[i] = static_cast<unsigned char>((high << 4) | low);
    }

    return true;
}

bool binStringToBytes(std::string_view sv, unsigned char* output) {
    const int len = sv.length();
    if (len % 8 != 0) {
        return false;
    }

    const int size = len / 8;
    for (int i = 0; i < size; i++) {
        output[i] = 0;
        for (int j = 0; j < 8; j++) {
            const int index = len - (i * 8 + j) - 1;
            const char res = sv[index] - '0';
            if (res != 0 && res != 1) {
                return false;
            }
            output[i] |= res << j;
        }
    }

    return true;
}

// 目前的实现是阻塞的，请勿在电机运行时切换旋转阀！
void transmit485(const uint8_t* data, size_t len) {
    // 向串口转485模块发送数据
    // 包含了发送数据和接受反馈的全流程
    Serial.print("待发送数据: (");
    for (int i = 0; i < len; i++) {
        if (i != len-1) {
            Serial.printf("%x, ", data[i]);
        } else {
            Serial.printf("%x)\n", data[i]);
        }
    }
    Serial1.write(data, len);
    Serial.println("数据已发送至485模块");

    // 一般1s内响应，先等待1000ms
    delay(1000);

    if (Serial1.available() >= 8) {
        std::array<uint8_t, 8> response;
        Serial1.readBytes(response.data(), 8);
        Serial.print("收到响应：");
        for (int i = 0; i < 8; i++) {
            if (response[i] < 0x10) Serial.print('0');
            Serial.print(response[i], HEX);
            Serial.print(' ');
        }
        Serial.println();
    } else {
        Serial.println("响应超时");
    }
}

void  procSwitchData(const SwitchCommand& command) {
    std::array<uint8_t, 8> buffer{};

    std::visit([&buffer](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, SwitchRaw>) {
            // RAW指令，传入16个16进制字符的字符串
            const std::string_view cmd_str = arg.raw_cmd;
            if (cmd_str.length() == INSTR_485_LEN * 2 && hexStringToBytes(cmd_str, buffer.data())) {
                transmit485(buffer.data(), INSTR_485_LEN);
            } else {
                Serial.println("十六进制数据格式错误");
                Serial.println("需要16个十六进制字符，例如：CC00200000DDC901");
            }
        } else {
            // 根据指令生成待传输的数据

            // 头和尾都是确定的
            buffer[0] = 0xcc;
            buffer[5] = 0xdd;
            // 通道是buffer[1], 默认为0

            // 处理不同指令
            if constexpr (std::is_same_v<T, SwitchCheck>) {
                buffer[2] = 0x3e;
            } else if constexpr (std::is_same_v<T, SwitchStatus>) {
                buffer[2] = 0x4a;
            } else if constexpr (std::is_same_v<T, SwitchChannel>) {
                const unsigned char channel = arg.channel;
                if (channel >= 1 && channel <= 6) {
                    buffer[2] = 0x44;
                    buffer[3] = channel;
                } else {
                    Serial.println("通道数错误，应在1~6之间");
                    return;
                }
            } else if constexpr (std::is_same_v<T, SwitchReset>) {
                buffer[2] = 0x45;
            } else {
                // unreachable!
            }

            // 计算校验和
            // 校验和为前六位之和，数据第七位是校验和 % 256，第八位是校验和 / 256
            unsigned int sum = 0;
            for (int i = 0; i < 6; i++) {
                sum += buffer[i];
            }
            buffer[6] = static_cast<uint8_t>(sum % 256);
            buffer[7] = static_cast<uint8_t>(sum / 256);

            transmit485(buffer.data(), INSTR_485_LEN);
        } 
    }, command);
}

void transmit595(uint8_t data) {
    digitalWrite(STCP, LOW);
    // 参数1：数据引脚
    // 参数2：移位寄存器时钟引脚
    // 参数3：数据格式(MSBFIRST, LSBFIRST)
    // 参数4：数据
    shiftOut(DS, SHCP, MSBFIRST, data);
    digitalWrite(STCP, HIGH);
}

std::string formatSolenoidStatus(const unsigned char& status) {
    std::string msg_str;
    msg_str += "电磁阀状态：";
    for (int i = 0; i < 8; i++) {
        std::string_view status_str = ((status & (1 << i)) == 0) ? "关闭" : "开启";
        std::string output_str = std::format("通道{}: {} ", (i + 1), status_str);
        msg_str += output_str;
    }
    return msg_str;
}

void writeDAC(int data) {
    if (data > 4095) return;
    const uint8_t data_1 = static_cast<uint8_t>(data >> 8); // 高四位为0
    const uint8_t data_2 = static_cast<uint8_t>(data & 0xFF);
    Wire.beginTransmission(DAC_ADDR);
    Wire.write(data_1);
    Wire.write(data_2);
    Wire.endTransmission();
}

void procSerialCommand(CtrlBoardManager& manager) {
    static std::string buffer = "";
    while (Serial.available()) {
        const char c = Serial.read();
        if (c == '\n') {
            // 解析命令
            std::transform(buffer.begin(), buffer.end(), buffer.begin(), ::tolower);
            manager.procInstruction(buffer);
            // 指令处理完成后，清空buffer和tokens
            buffer.clear();

        } else if (isAlpha(c) || c == ' ' || isDigit(c) || c == '.' || c == '-') {
            buffer += c;
        }
    }
}

void printSyringeInstr() {
    Serial.println("sp -f 50  - 注射泵前进50mm");
    Serial.println("sp -b 30  - 注射泵后退30mm");
    Serial.println("sp -fv 5  - 注射泵前进5mL");
    Serial.println("sp -bv 3  - 注射泵后退3mL");
    Serial.println("sp -sv 0.1  - 注射泵设置流速为0.1mL/s");
    Serial.println("sp -ft [0-3]  - 注射泵微调");
    Serial.println("sp -s  - 注射泵停止");
}

void printPeristalticInstr() {
    Serial.println("pp -f 5  - 蠕动泵前进5转");
    Serial.println("pp -b 3  - 蠕动泵后退3转");
    Serial.println("pp -v 1000  - 蠕动泵设置流速为1000步/s");
    Serial.println("pp -fv 5  - 蠕动泵前进5mL");
    Serial.println("pp -bv 3  - 蠕动泵后退3mL");
    Serial.println("pp -sv 0.1  - 蠕动泵设置流速为0.1mL/s");
    Serial.println("pp -s  - 蠕动泵停止");
}

void printSwitchInstr() {
    Serial.println("sv -raw CC00200000DDC901 - 发送8字节数据");
    Serial.println("sv -check - 查询当前通道编号");
    Serial.println("sv -status - 查询切换阀电机状态");
    Serial.println("sv -c [1~6] - 旋转到指定通道");
    Serial.println("sv -r  - 复位");
}

void printSolenoidInstr() {
    Serial.println("sov -d 195 / sov -b 11000011 / sov -h C3 - 发送1字节数据，控制八个电磁阀通道");
    Serial.println("sov -s - 查询电磁阀开关状态");
    Serial.println("sov -c [1~8] [0/1] - 控制电磁阀指定通道开/关");
}

void printProportionInstr() {
    Serial.println("pv -max 100 - 记录比例阀最大压强 (比例阀默认500kPa，程序默认100kPa)");
    Serial.println("pv -p 50 - 设定比例阀压强 (kPa)");
}

void printLightInstr() {
    Serial.println("l -[on/off] - 开启或关闭光源");
    Serial.println("l -b [0~255] - 设置光源亮度");
}