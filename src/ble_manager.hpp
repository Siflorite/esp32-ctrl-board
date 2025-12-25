#pragma once

#include "ble_callbacks.hpp"
#include "constants.hpp"
#include <memory>
#include "NimBLEDevice.h"
#include <string_view>
#include "types.hpp"

class BLEManager {
private:
    NimBLEServer* server;
    NimBLECharacteristic* notif_chara;
    NimBLECharacteristic* write_chara;
    NimBLECharacteristic* data_chara;

    std::unique_ptr<ServerCallbacks> server_callbacks;
    std::unique_ptr<CharacteristicCallbacks> chara_callbacks;

    uint16_t _service_uuid;
    uint16_t _write_chara_uuid;
    uint16_t _notif_chara_uuid;
    uint16_t _data_chara_uuid;

public:
    BLEManager(uint16_t service_uuid, uint16_t write_chara_uuid, uint16_t notif_chara_uuid, uint16_t data_chara_uuid);
    ~BLEManager();
    void init(OnWriteCallBackFunction write_callback);

    bool isConnected();
    void notify(std::string_view message);
    void notify_data(const std::array<uint8_t, PACKET_SIZE>& buffer);
};