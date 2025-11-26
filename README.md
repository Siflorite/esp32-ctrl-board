# ESP32 Control Board
This is a project for utilising an ESP32S3 development board to control multiple peripheral devices, including syringe pump, peristaltic pump, switch valve, solenoid valves, proportional valve and LED array.

The project is based on PlatformIO, specifically using Arduino Framework for ESP-IDF v5.0+ for using some C++20 stuff. Great thanks for [pioarduino's espressif32 framework](https://github.com/pioarduino/platform-espressif32) featuring support for ESP-IDF v5.0+.

## Peripherals
The peripherals which ESP32 controls are listed as follows.

- Syringe pump: Driven by TMC2209 direct insert module.
  
  EN = GPIO7, DIR = GPIO8, STEP = GPIO9.
- Peristaltic pump: Driven by TMC2209 direct insert module.

  EN = GPIO7, DIR = GPIO10, STEP = GPIO11.
- Switch valve: Driven by a Serial-to-485 module.

  TX=GPIO1，RX=GPIO2.
- Solenoid valves: Signals go through a 74HC595D, then driven by a ULN2803 to control 8 channels of solenoid valves.

  DS = GPIO4，SHCP = GPIO5，STCP = GPIO6.
- Proportion valve: Driven by a MCP4725 DAC, providing 1-channel 12-bit (0\~4096) 0~5V analog output.

  SDA=GPIO17，SCL=GPIO18.
- LED Array: An 8x8 WS2812 Array.

  WS_IN = GPIO5.

Sorry for not providing the schematic, this project is mainly focused on ESP-side control through code.

## Source code structure
- `main.cpp`: Entry for main function (`setup()` and `loop()` as for Arduino framework). Initialize the manager in `setup()`, while process command every 50ms and keep motor running in `loop()`
- `ctrl_board_manager.hpp` & `ctrl_board_manager.cpp`: Definition and implementation of class `CtrlBoardManager`, mainly responsible for controlling and tracking all peripherals.
- `misc.hpp` & `misc.cpp`: Providing functions that don't require a `CtrlBoardManager` instance. Including converting strings to byte data, trasmitting 485 and 595 data, handling serial commands, printing instruction usages, etc.
- `types.hpp`: Some specific enums and types used in the project.
- `constants.hpp`: All constants used in this project, including GPIO pin definition, initial and maximum speed for motors, DAC address, etc. The constants are all defined with `constexpr` instead of `#define` to reduce conflict and ensure type safety.

## Usage
Simply clone this project and load it in PlatformIO. PlatformIO will automatically download all external libraries required (AccelStepper and FastLED), then compile and upload the program to an ESP32S3 board. Other ESP32 boards may not provide such many GPIOs as ESP32S3.

Use Serial to connect to ESP32S3 and post commands to send instructions. The command text sent through serial must work at baud rate 115200 and end with a `\n`. All commands will have a reply, and help instructions will be given when receiving illegal commands.

## Clangd support
Clangd provides a better static examination for cpp projects and is strongly supported for substituting old Intellisense, for users using VS Code. (Or you can switch to VAssistX/Resharper C++ plugins for Visual Studio, and CLion IDE by JetBrains.) Here shows a routine for using clangd in VSCode.

1. In extension side window of VS Code, search C++, open the settings of `C/C++` Extension, search `Intellisense`, set `C_cpp: Intelli Sense Engine` to `disabled`.
2. Search and install `clangd` extension.
3. Download [esp-clang](https://github.com/espressif/llvm-project/releases) by espressif.
4. In PlatformIO Terminal, run `pio run -t compiledb`, this will generate a `compile_commands.json` at the root directory of the project, which will provide all files related for clangd to execute examination.
5. Add these contents to `.vscode/settings.json` of your project (create one if it doesn't exist):
```JSON
{
    "clangd.arguments": [
        "--compile-commands-dir=${workspaceFolder}",
        "--query-driver=path/to/your/compiler"
    ],
    "clangd.path": "path/to/your/clangd"
}
```
There are two things that need to be modified: `--query-driver` need to be set to the compiler path (gcc and g++), you can check this through `compile_commands.json`. Usually it will be `PlatformIO_Install_Path/packages/toolchain-xtensa-esp-elf/bin/*`; `"clangd.path"` is where the esp-clang's clangd is located, for example, if you extract the zip at `/esp-clang` then you need to fill in `/esp-clang/bin/clangd.exe`

You can also modify these settings in `clangd` extension's settings.

6. `Ctrl`+`Shift`+`P` in VS Code and execute `clangd: Restart language server`. Usually all will be fine, since I've added errors to ignore in the file `.clangd`, but it may sometimes still come to errors, which need to be solved yourself.

PlatformIO requires the `C/C++` extension of Microsoft to run, which shouldn't be uninstalled. And everytime you open a PlatformIO project, a window will pop up, showing that `clangd` extension is in conflict with PlatformIO. Actually it doesn't show any conflict and can be ignored.

## Instructions
Instructions available for v0.1.0 (2025-11-26):

**注射泵：**

sp -f [小数]：注射泵前进[小数]mm

sp -b [小数]：注射泵后退[小数]mm

sp -fv [小数]  - 注射泵前进[小数]mL

sp -bv [小数]  - 注射泵后退[小数]mL

sp -sv [小数]  - 注射泵设置流速为[小数]mL/s，范围为(0,0.5]

sp -ft [0\~3] - 注射泵微调（持续运动），0\~3模式依次为快速上升、慢速上升、慢速下降和快速下降

sp -s - 注射泵停止

**蠕动泵：**

pp -f [小数]  - 蠕动泵前进[小数]mm

pp -b [小数]  - 蠕动泵后退[小数]mm

pp -fv [小数]  - 蠕动泵前进[小数]mL

pp -bv [小数]  - 蠕动泵后退[小数]mL

pp -v [小数]  - 蠕动泵设置流速为[小数]步/s

pp -sv [小数]  - 蠕动泵设置流速为[小数]mL/s，范围为(0,0.5]

pp -s - 蠕动泵停止

**切换阀：**

sv -raw [hex]: 向切换阀输出原始数据，格式为8个byte的16进制，如CC00200000DDC901，具体见说明

sv -check，查询通道编号，范围为1~6（返回值）；

sv -status，查询切换阀电机状态；

sv -c [整数]，旋转到目标通道[整数]，范围为1~6；

sv -r，复位。

**电磁阀：**

sov -d [u8] / sov -b [] / sov -h []: 输入一个8bit的整数 / 8个0或1 / 16进制数，同时控制八个电磁阀的状态。范围为0~255（十进制下）。

sov -s：显示当前八个电磁阀的状态。

sov -c [1\~8] [1/0]：控制第1~8个通道中其中一个的开关（1为开，0为关），其余不变。


**比例阀：**

pv -max [0~500] - 记录比例阀最大压强 (比例阀默认500kPa，程序默认100kPa)

pv -p [整数] - 设定比例阀压强 (kPa)，范围为0~最大压强

**LED阵列：**

l -[on/off]: 开启/关闭灯光

l -b [0\~255]: 设置灯光亮度，范围为0\~255的整数

## 中文文档
我在飞书上提供了公开的本项目的飞书文档，详见[https://pcnhx1x03hi7.feishu.cn/wiki/SW8QwELKXirzG0k3eBUc2YKUn6g](https://pcnhx1x03hi7.feishu.cn/wiki/SW8QwELKXirzG0k3eBUc2YKUn6g)。
