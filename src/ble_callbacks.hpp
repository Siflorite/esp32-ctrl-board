#pragma once

#include "NimBLEDevice.h"
#include "types.hpp"

// 核心设备连接外围设备回调函数
class ServerCallbacks : public NimBLEServerCallbacks {
public:
    // 重载默认实现的Callbacks
    virtual void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override;
    virtual void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override;
    virtual void onMTUChange(uint16_t MTU, NimBLEConnInfo& connInfo) override;
    virtual uint32_t onPassKeyDisplay() override;
    virtual void onConfirmPassKey(NimBLEConnInfo& connInfo, uint32_t pin) override;
    virtual void onAuthenticationComplete(NimBLEConnInfo& connInfo) override;
};

class CharacteristicCallbacks : public NimBLECharacteristicCallbacks {
private:
    OnWriteCallBackFunction _on_write_callback;

public:
    virtual void onRead(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override;
    virtual void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override;
    virtual void onSubscribe(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo, uint16_t subValue) override;

    /**
     * @brief 设置写操作回调函数
     *
     * 当BLE特征值被写入时，将调用此回调函数。
     *
     * @param callback 用户定义的回调函数，当特征值被写入时触发
     *
     * 回调函数类型应为`void(NimBLECharacteristic* pCharacteristic)`
     *
     * @note 回调函数将在BLE线程上下文中执行，应避免执行耗时操作
     */
    void setOnWriteCallback(OnWriteCallBackFunction callback);
};