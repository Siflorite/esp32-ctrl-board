#include <Arduino.h>
#include <AccelStepper.h>

#include "constants.hpp"
#include "ctrl_board_manager.hpp"
#include "misc.hpp"


static AccelStepper stepper(AccelStepper::DRIVER, STEP_PIN, DIR_PIN);
static AccelStepper stepper_pp(AccelStepper::DRIVER, P_STEP, P_DIR);

static CtrlBoardManager manager(&stepper, &stepper_pp);

unsigned long previous_millis = 0;

void setup() {
    manager.init();
    Serial.println("系统已启动");
}

void loop() {
    const unsigned long current_millis = millis();
    if (current_millis - previous_millis >= INTERVAL) {
        previous_millis = current_millis;
        
        procSerialCommand(manager);
        if (stepper_pp.distanceToGo() > 0) Serial.println(stepper_pp.distanceToGo());
    }
    
    manager.maintainMotor();
}