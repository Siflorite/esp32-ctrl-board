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