#include "ctrl_board_manager.hpp"

#include "constants.hpp"
#include "misc.hpp"
#include "types.hpp"

#include <cmath>
#include <format>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>
#include "Wire.h"

CtrlBoardManager::CtrlBoardManager(AccelStepper* sp, AccelStepper* pp) 
    : stepper(sp), stepper_pp(pp) {
        
    // 配置步进电机参数
    // 这些参数目前都是随手填的，需要规范化
    syringe_speed = 3200; // 等效速度0.2mm/s -> 0.057mL/s
    syringe_status = false;

    peristaltic_speed = 800; // 等效蠕动泵0.5转/s
    peristaltic_status = false;

    switch_channel = 0;

    // 电磁阀状态：默认全关闭
    solenoid_valve_status = 0;

    // 比例阀
    max_pressure = 100;
    cur_pressure = 0;

    // 光源初始化
    brightness = 200;
    light_status = false;

    // BLE相关
    ble_manager = std::make_unique<BLEManager>(SERVICE_UUID, WRITE_CHARA_UUID, NOTIF_CHARA_UUID);
    
    msg_queue.reserve(10);
}

CtrlBoardManager::~CtrlBoardManager() {
    msg_queue.clear();
}

void CtrlBoardManager::init() {
    // 与上位机通信
    Serial.begin(115200);
    // 连接485模块
    Serial1.begin(9600, SERIAL_8N1, RX_485, TX_485);

    pinMode(EN_PIN, OUTPUT);
    pinMode(P_EN, OUTPUT);

    // 74HC595
    pinMode(DS, OUTPUT);
    pinMode(SHCP, OUTPUT);
    pinMode(STCP, OUTPUT);

    // 电磁阀初始化
    transmit595(solenoid_valve_status);

    // DAC2 (比例阀)
    Wire.begin(SDA_PIN, SCL_PIN);
    updatePressure();

    // 旋转阀初始化
    procSwitchData(SwitchReset{});

    // 电机初始化速度和加速度
    if (stepper) {
        stepper->setMaxSpeed(syringe_speed);
        stepper->setAcceleration(200000); // WTF?
        stepper->setCurrentPosition(0);
    }

    if (stepper_pp) {
        stepper_pp->setMaxSpeed(peristaltic_speed);
        stepper_pp->setAcceleration(40000); // WTF?
        stepper_pp->setCurrentPosition(0);
    }

    // LED 初始色彩设置：中心4x4为白，其余为黑
    FastLED.addLeds<WS2812B, WS_IN, GRB>(leds.data(), NUM_LEDS);
    fill_solid(&(leds[0]), NUM_LEDS, CRGB::Black);
    for (auto i : LED_ARR()) {
        leds[i] = CRGB::White;
    }

    ble_manager->init([this](NimBLECharacteristic* pCharacteristic) {
        if (!pCharacteristic) return;
        if (pCharacteristic->getUUID() == NimBLEUUID(WRITE_CHARA_UUID)) {
            auto value = pCharacteristic->getValue();
            std::string command_str = convertAttrValue<std::string>(value);
            // 这里的输出暂时先不通过消息队列处理，毕竟串口只是debug用
            std::string msg_str = std::format("BLE接收到指令: {}", command_str);
            Serial.println(msg_str.c_str());
            this->procInstruction(command_str);
        }
    });
}

void CtrlBoardManager::setSyringeSpeed(float speed, bool b_volume_speed) {
    if (b_volume_speed) {
        // 设置流体速度
        // 电机速度(微步/s) = 流速（mL/s) * 比例（mm/mL）/ 螺距(mm/转) * 微步数 * 每转步数
        // 微步数64下，速度最好不要超过0.5mL/s
        syringe_speed = speed * V2D_RATIO / SCREW_PITCH * MICROSTEPS_1 * STEPS_PER_REV;
    } else {
        syringe_speed = speed;
    }

    if (stepper) {
        stepper->setMaxSpeed(syringe_speed); // 最大速度（步/秒）
    }
}

void CtrlBoardManager::setPeristalticSpeed(float speed, bool b_volume_speed) {
    if (b_volume_speed) {
        peristaltic_speed = speed * V2R_RATIO * MICROSTEPS_2 * STEPS_PER_REV;
    } else {
        peristaltic_speed = speed;
    }

    if (stepper_pp) {
        stepper_pp->setMaxSpeed(peristaltic_speed);
    }
}

void CtrlBoardManager::moveMm(float mm) {
    const long target = mm * (STEPS_PER_REV * MICROSTEPS_1) / SCREW_PITCH;
    if (stepper) {
        stepper->move(target);
    }
    syringe_status = true;
}

void CtrlBoardManager::ppMoveRounds(float rounds) {
    const long target = rounds * (STEPS_PER_REV * MICROSTEPS_2);
    if (stepper_pp) {
        stepper_pp->move(target);
    }
    peristaltic_status = true;
}

void CtrlBoardManager::syrineFinetune(const SyringeFinetuneType& type) {
    // 微调
    // 0,1,2,3分别表示快升，慢升，慢降，快降
    // finetune使用强制位移到一个很远的地方，比如30mL
    const float distance = 30.0 * V2D_RATIO;

    if (stepper) {
        switch (type) {
            using enum SyringeFinetuneType;
            // 不能用setSyringeSpeed，因为这是用户保存的速度，不能覆盖
            case SPEED_UP:
                stepper->setMaxSpeed(FINETUNE_FAST);
                moveMm(distance);
                msg_queue.emplace_back("注射泵快速上移");
                break;
            case SLOW_UP:
                stepper->setMaxSpeed(FINETUNE_SLOW);
                moveMm(distance);
                msg_queue.emplace_back("注射泵慢速上移");
                break;
            case SLOW_DOWN:
                stepper->setMaxSpeed(FINETUNE_SLOW);
                moveMm(-distance);
                msg_queue.emplace_back("注射泵慢速下移");
                break;
            case SPEED_DOWN:
                stepper->setMaxSpeed(FINETUNE_FAST);
                moveMm(-distance);
                msg_queue.emplace_back("注射泵快速下移");
                break;
        }
    }
}

void CtrlBoardManager::stopSyringe() {
    if (stepper) {
        stepper->stop();
        stepper->setMaxSpeed(syringe_speed); // finetune后恢复
    }
    syringe_status = false;
}

void CtrlBoardManager::stopPeristaltic() {
    if (stepper_pp) {
        stepper_pp->stop();
    }
    peristaltic_status = false;
}

void CtrlBoardManager::maintainMotor() {
    if (stepper) {
        if (stepper->distanceToGo() != 0) {
            stepper->run();
        } else {
            if (syringe_status == true) {
                msg_queue.emplace_back("注射泵运动完成");
                syringe_status = false;
            }
        }
    }

    if (stepper) {
        if (stepper_pp->distanceToGo() != 0) {
            stepper_pp->run();
        } else {
            if (peristaltic_status == true) {
                msg_queue.emplace_back("蠕动泵运动完成");
                peristaltic_status = false;
            }
        }
    }

    // 不用时关闭使能
    if (syringe_status) {
        digitalWrite(EN_PIN, LOW);
    } else {
        digitalWrite(EN_PIN, HIGH);
    }

    if (peristaltic_status) {
        digitalWrite(P_EN, LOW);
    } else {
        digitalWrite(P_EN, HIGH);
    }
}

void CtrlBoardManager::solenoidToggleChannel(int channel, bool status) {
    const int bit_num = channel - 1;
    const unsigned char val = 1 << bit_num;
    const unsigned char mask = ~val;
    
    solenoid_valve_status &= mask;
    if (status) {
        solenoid_valve_status |= val;
    }
    transmit595(solenoid_valve_status);
}

void CtrlBoardManager::updatePressure() {
    const float proportion = static_cast<float>(cur_pressure) / static_cast<float>(max_pressure);
    const int quantized_data = static_cast<int>(std::round(proportion * 4096.0));
    const int data = std::min(quantized_data, 4095);
    writeDAC(data);
}

void CtrlBoardManager::shutLED() {
    FastLED.setBrightness(0);
    FastLED.show();
    light_status = false;
}

void CtrlBoardManager::updateLED() {
    FastLED.setBrightness(brightness);
    FastLED.show();
}

void CtrlBoardManager::procInstruction(std::string_view instruction) {
    auto tokens = instruction
        | std::views::split(' ') // 只能使用字符(char)，不能使用字符串
        | std::views::transform([](auto token) {
            return std::string(token.begin(), token.end());
        });
    // 展望C++23用上 | std::ranges::to<std::string>();
    const std::vector<std::string> tokens_vec(tokens.begin(), tokens.end());
    const int token_count = tokens_vec.size();
    if (token_count == 0) return;

    // 处理指令
    bool b_proc_success = false;

    if (tokens_vec[0] == "sp") {
        // 注射泵控制
        if (token_count == 2 && tokens_vec[1] == "-s") {
            stopSyringe();
            msg_queue.emplace_back("注射泵已停止");
            b_proc_success = true;
        } else if (token_count == 3) {
            const auto instruction = tokens_vec[1];
            const auto value = tokens_vec[2];

            if (instruction == "-v") {
                const float speed = std::stof(value);
                if (speed > 0 && speed <= FINETUNE_FAST) {
                    setSyringeSpeed(speed);
                    std::string msg_str = std::format(
                        "已设置注射泵速度为 {} 微步/s，对应电机转速 {} rps",
                        speed,
                        speed / MICROSTEPS_1 / STEPS_PER_REV
                    );
                    msg_queue.push_back(std::move(msg_str));
                    b_proc_success = true;
                }
            } else if (instruction == "-sv") {
                const float speed = std::stof(value);
                if (speed > 0 && speed <= SYRINGE_MAXIMUM_SPEED) {
                    setSyringeSpeed(speed, true);
                    std::string msg_str = std::format(
                        "已设置注射泵速度为 {} mL/s，对应电机转速 {} rps",
                        speed,
                        speed * V2D_RATIO / SCREW_PITCH
                    );
                    msg_queue.push_back(std::move(msg_str));
                    b_proc_success = true;
                }
            } else if (instruction == "-f" || instruction == "-b") {
                const float distance = std::stof(value);
                if (distance > 0) {
                    const float distance_r = (instruction == "-f") ? distance : -distance;
                    const std::string_view pos_str = (instruction == "-f") ? "正向" : "反向";
                    moveMm(distance_r);
                    std::string msg_str = std::format(
                        "注射泵 {} 移动 {} mm",
                        pos_str,
                        distance
                    );
                    msg_queue.push_back(std::move(msg_str));
                    b_proc_success = true;
                }
            } else if (instruction == "-fv" || instruction == "-bv") {
                const float volume = std::stof(value);
                if (volume > 0) {
                    const float volume_r = (instruction == "-fv") ? volume : -volume;
                    const float distance_r = volume_r * V2D_RATIO;
                    const std::string_view pos_str = (instruction == "-fv") ? "正向" : "反向";
                    moveMm(distance_r);
                    std::string msg_str = std::format(
                        "注射泵 {} 移动 {} mL",
                        pos_str,
                        volume
                    );
                    msg_queue.push_back(std::move(msg_str));
                    b_proc_success = true;
                }
            } else if (instruction == "-ft") {
                const int param = std::stoi(value);
                if (param >= 0 && param <=3) {
                    SyringeFinetuneType finetune_type = static_cast<SyringeFinetuneType>(param);
                    syrineFinetune(finetune_type);
                    b_proc_success = true;
                }
            }
        }

        if (!b_proc_success) {
            msg_queue.emplace_back("无效注射泵指令");
            printSyringeInstr();
        }
    } else if (tokens_vec[0] == "pp") {
        // 蠕动泵控制
        if (token_count == 2 && tokens_vec[1] == "-s") {
            stopPeristaltic();
            msg_queue.emplace_back("蠕动泵已停止");
            b_proc_success = true;
        } else if (token_count == 3) {
            auto instruction = tokens_vec[1];
            auto value = tokens_vec[2];

            if (instruction == "-v") {
                const float speed = std::stof(value);
                if (speed > 0 && speed <= PERISTALTIC_MAXIMUM_MICROSTEP) {
                    setPeristalticSpeed(speed);
                    std::string msg_str = std::format(
                        "已设置蠕动泵速度为 {} 微步/s，对应电机转速 {} rps",
                        speed,
                        speed / MICROSTEPS_2 / STEPS_PER_REV
                    );
                    msg_queue.push_back(std::move(msg_str));
                    b_proc_success = true;
                }
            } else if (instruction == "-sv") {
                const float speed = std::stof(value);
                if (speed > 0 && speed <= PERISTALTIC_MAXIMUM_SPEED) {
                    setPeristalticSpeed(speed, true);
                    std::string msg_str = std::format(
                        "已设置蠕动泵速度为 {} mL/s，对应电机转速 {} rps",
                        speed,
                        speed * V2R_RATIO
                    );
                    msg_queue.push_back(std::move(msg_str));
                    b_proc_success = true;
                }
            } else if (instruction == "-f" || instruction == "-b") {
                const float rounds = std::stof(value);
                if (rounds > 0) {
                    const float rounds_r = (instruction == "-f") ? rounds : -rounds;
                    const std::string_view pos_str = (instruction == "-f") ? "正向" : "反向";
                    ppMoveRounds(rounds_r);
                    std::string msg_str = std::format(
                        "蠕动泵 {} 转动 {} 转",
                        pos_str,
                        rounds
                    );
                    msg_queue.push_back(std::move(msg_str));
                    b_proc_success = true;
                }
            } else if (instruction == "-fv" || instruction == "-bv") {
                const float volume = std::stof(value);
                if (volume > 0) {
                    const float volume_r = (instruction == "-fv") ? volume : -volume;
                    const float rounds_r = volume_r * V2R_RATIO;
                    const std::string_view pos_str = (instruction == "-fv") ? "正向" : "反向";
                    ppMoveRounds(rounds_r);
                    std::string msg_str = std::format(
                        "蠕动泵 {} 转动 {} mL",
                        pos_str,
                        volume
                    );
                    msg_queue.push_back(std::move(msg_str));
                    b_proc_success = true;
                }
            }
        }

        if (!b_proc_success) {
            msg_queue.emplace_back("无效蠕动泵指令");
            printPeristalticInstr();
        }
    } else if (tokens_vec[0] == "sv") {
        // 切换阀控制
        SwitchCommand cmd;
        if (token_count == 2) {
            if (tokens_vec[1] == "-check") {
                cmd = SwitchCheck{};
                b_proc_success = true;
            } else if (tokens_vec[1] == "-status") {
                cmd = SwitchStatus{};
                b_proc_success = true;
            } else if (tokens_vec[1] == "-r") {
                cmd = SwitchReset{};
                b_proc_success = true;
            }
        } else if (token_count == 3) {
            if (tokens_vec[1] == "-raw") {
                cmd = SwitchRaw{tokens_vec[2]};
                b_proc_success = true;
            } else if (tokens_vec[1] == "-c") {
                const int channel = std::stoi(tokens_vec[2]);
                cmd = SwitchChannel{channel};
                b_proc_success = true;
            }
        }

        if (b_proc_success) {
            procSwitchData(cmd);
            if (ble_manager) {
                ble_manager->notify("已发送旋转阀指令");
            }
        } else {
            msg_queue.emplace_back("无效旋转阀指令");
            printSwitchInstr();
        }

    } else if (tokens_vec[0] == "sov") {
        // 电磁阀控制
        if (token_count == 2 && tokens_vec[1] == "-s") {
            b_proc_success = true;
        } else if (token_count == 3) {
            const std::string& instr = tokens_vec[1];
            const std::string& val = tokens_vec[2];
            if (instr == "-d") {
                const int status_val = std::stoi(val);
                if (status_val >= 0 && status_val <= 255) {
                    b_proc_success = true;
                    solenoid_valve_status = static_cast<unsigned char>(status_val);
                    transmit595(solenoid_valve_status);
                }  else {
                    msg_queue.emplace_back("输入整数参数必须在0~255之间");
                }
            } else if (instr == "-b") {
                if (val.length() == 8 && binStringToBytes(val, &solenoid_valve_status)) {
                    b_proc_success = true;
                    transmit595(solenoid_valve_status);
                } else {
                    msg_queue.emplace_back("输入参数必须是8个二进制数（0或1）");
                }
            } else if (instr == "-h") {
                if (val.length() == 2 && hexStringToBytes(val, &solenoid_valve_status)) {
                    b_proc_success = true;
                    transmit595(solenoid_valve_status);
                } else {
                    msg_queue.emplace_back("输入参数必须是2个十六进制字符（0~F）");
                }
            }
        } else if (token_count == 4) {
            if (tokens_vec[1] == "-c") {
                b_proc_success = true;
                const int channel = std::stoi(tokens_vec[2]);
                const bool status = static_cast<bool>(std::stoi(tokens_vec[3]));
                if (channel >= 1 && channel <= 8) {
                    solenoidToggleChannel(channel, status);
                } else {
                    msg_queue.emplace_back("参数错误：通道（即第一个参数）需要在[1,8]范围");
                }
            }
        }

        if (b_proc_success) {
            std::string msg_str = formatSolenoidStatus(solenoid_valve_status);
            msg_queue.push_back(std::move(msg_str));
        } else{
            msg_queue.emplace_back("电磁阀指令错误");
            printSolenoidInstr();
        }
    } else if (tokens_vec[0] == "pv") {
        // 比例阀控制
        if (token_count == 3) {
            const std::string& cmd = tokens_vec[1];
            int val = std::stoi(tokens_vec[2]);
            if (cmd == "-max") {
                b_proc_success = true;
                if (val > 0 && val <= 500) {
                    max_pressure = val;
                    updatePressure();

                    std::string msg_str = std::format(
                        "已将最大压强记录为 {} kPa",
                        max_pressure
                    );
                    msg_queue.push_back(std::move(msg_str));
                } else {
                    msg_queue.emplace_back("最大压强必须在 (0, 500] kPa范围内");
                }
            } else if (cmd == "-p") {
                b_proc_success = true;
                if (val >= 0 && val <= max_pressure) {
                    cur_pressure = val;
                    updatePressure();

                    std::string msg_str = std::format(
                        "输出压强 {} kPa",
                        cur_pressure
                    );
                    msg_queue.push_back(std::move(msg_str));
                } else {
                    std::string msg_str = std::format(
                        "输出压强必须在 [0, {}] kPa范围内",
                        max_pressure
                    );
                    msg_queue.push_back(std::move(msg_str));
                }
            }
        }

        if (!b_proc_success) {
            msg_queue.emplace_back("比例阀指令错误");
            printProportionInstr();
        }

    } else if (tokens_vec[0] == "l") {
        // 光源控制
        if (token_count == 2 && (tokens_vec[1] == "-on" || tokens_vec[1] == "-off")) {
            if (tokens_vec[1] == "-off") {
                shutLED();
                msg_queue.emplace_back("已关闭光源");
            } else {
                updateLED();
                light_status = true;
                std::string msg_str = std::format(
                    "已开启光源，亮度为 {}",
                    brightness
                );
                msg_queue.push_back(std::move(msg_str));
            }
        } else if (token_count == 3 && tokens_vec[1] == "-b") {
            int val = std::stoi(tokens_vec[2]);
            if (val >= 0 && val <= 255) {
                brightness = static_cast<uint8_t>(val);
                if (light_status) {
                    updateLED();
                }
                std::string msg_str = std::format(
                    "已调整亮度为 {}",
                    brightness
                );
                msg_queue.push_back(std::move(msg_str));
            } else {
                msg_queue.emplace_back("亮度值需要在[0,255]范围内");
            }
        } else {
            msg_queue.emplace_back("光源指令错误");
            printLightInstr();
        }
    } else {
        msg_queue.emplace_back("无效指令");
        Serial.println("可用命令：");
        printSyringeInstr();
        printPeristalticInstr();
        printSwitchInstr();
        printSolenoidInstr();
        printProportionInstr();
        printLightInstr();
    }
}

void CtrlBoardManager::postNewMessage() {
    for (const auto& msg_str : msg_queue) {
        Serial.println(msg_str.c_str());
        if (ble_manager && ble_manager->isConnected()) {
            ble_manager->notify(msg_str);
        }
    }
    msg_queue.clear();
}