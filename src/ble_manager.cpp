#include "ble_manager.hpp"

BLEManager::BLEManager(uint16_t service_uuid, uint16_t write_chara_uuid, uint16_t notif_chara_uuid, uint16_t data_chara_uuid) 
    : server(nullptr), notif_chara(nullptr), write_chara(nullptr),
        _service_uuid(service_uuid), 
        _write_chara_uuid(write_chara_uuid), 
        _notif_chara_uuid(notif_chara_uuid),
        _data_chara_uuid(data_chara_uuid)
{

}

BLEManager::~BLEManager() {
    // unique_ptr析构时自动销毁，不需要手动管理内存
}

void BLEManager::init(OnWriteCallBackFunction write_callback) {
    // 回调函数初始化
    server_callbacks = std::make_unique<ServerCallbacks>();
    chara_callbacks = std::make_unique<CharacteristicCallbacks>();
    chara_callbacks->setOnWriteCallback(write_callback);

    // 服务端初始化
    NimBLEDevice::init("NimBLE");
    NimBLEDevice::setSecurityAuth(true, true, false); // Bonding, MITM, Secure Connection
    NimBLEDevice::setSecurityPasskey(123456);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);

    // 建立GATT Server
    server = NimBLEDevice::createServer();
    // 设置与核心设备连接的回调函数
    server->setCallbacks(server_callbacks.get());  // 使用 .get() 获取原始指针

    // 添加服务
    NimBLEService* service_ptr = server->createService(_service_uuid);

    // 添加特征
    // 特征0x1234，可写入（带响应与不带响应）
    write_chara = service_ptr->createCharacteristic(
        _write_chara_uuid, 
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
    );
    // 设置特征被读写时的回调函数
    write_chara->setCallbacks(chara_callbacks.get());
    // 类型为UTF8
    NimBLE2904* write_description_2904_ptr = write_chara->create2904();
    write_description_2904_ptr->setFormat(NimBLE2904::FORMAT_UTF8);
    // 描述特征
    NimBLEDescriptor* write_description_2901_ptr = write_chara->createDescriptor("2901", NIMBLE_PROPERTY::READ);
    write_description_2901_ptr->setValue("Command string");

    // 特征0x5678，可读和订阅
    notif_chara = service_ptr->createCharacteristic(
        _notif_chara_uuid, 
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );
    // 设置特征被读写时的回调函数
    notif_chara->setCallbacks(chara_callbacks.get());
    // 类型为UTF8
    NimBLE2904* notify_description_2904_ptr = notif_chara->create2904();
    notify_description_2904_ptr->setFormat(NimBLE2904::FORMAT_UTF8);
    // 描述特征
    NimBLEDescriptor* notify_description_2901_ptr = notif_chara->createDescriptor("2901", NIMBLE_PROPERTY::READ);
    notify_description_2901_ptr->setValue("Control Board Message Notification");

    // 特征0xFACE，可读和订阅
    data_chara = service_ptr->createCharacteristic(
        _data_chara_uuid, 
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );
    // 设置特征被读写时的回调函数
    data_chara->setCallbacks(chara_callbacks.get());
    // 类型为UTF8
    NimBLE2904* data_description_2904_ptr = data_chara->create2904();
    data_description_2904_ptr->setFormat(NimBLE2904::FORMAT_SINT128);
    // 描述特征
    NimBLEDescriptor* data_description_2901_ptr = data_chara->createDescriptor("2901", NIMBLE_PROPERTY::READ);
    data_description_2901_ptr->setValue("Control Board Status Notification");

    // 开启服务
    service_ptr->start();
    notif_chara->setValue("Control Board BLE Activated.");
    data_chara->setValue("");

    // 开启广播
    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(_service_uuid);  // advertise the UUID of our service
    pAdvertising->setName("NimBLE");       // advertise the device name
    pAdvertising->start();

    Serial.println("BLE Server Started");
}

bool BLEManager::isConnected() {
    return (server && server->getConnectedCount() > 0);
}

void BLEManager::notify(std::string_view message) {
    if (isConnected() && notif_chara) {
        notif_chara->setValue(message);
        notif_chara->notify();
        // Serial.printf("BLE Manager notified: %.*s\n", message.length(), message.data());
    } else {
        Serial.println("BLE Manager fails to notify!");
    }
}

void BLEManager::notify_data(const std::array<uint8_t, PACKET_SIZE>& buffer) {
    if (isConnected() && data_chara) {
        data_chara->setValue(buffer);
        data_chara->notify();
    } else {
        Serial.println("BLE Manager fails to notify data!");
    }
}