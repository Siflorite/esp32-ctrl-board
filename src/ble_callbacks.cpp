#include "ble_callbacks.hpp"

// Server Callbacks:
void ServerCallbacks::onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) {
    Serial.printf("Client connected, addr: %s\n", connInfo.getAddress().toString().c_str());
    /**
     *  We can use the connection handle here to ask for different connection parameters.
     *  Args: connection handle, min connection interval, max connection interval
     *  latency, supervision timeout.
     *  Units; Min/Max Intervals: 1.25 millisecond increments.
     *  Latency: number of intervals allowed to skip.
     *  Timeout: 10 millisecond increments.
     */
    pServer->updateConnParams(connInfo.getConnHandle(), 24, 48, 0, 180);
}

void ServerCallbacks::onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) {
    Serial.printf("Client disconnected - start advertising\n");
    NimBLEDevice::startAdvertising();
}

void ServerCallbacks::onMTUChange(uint16_t MTU, NimBLEConnInfo& connInfo) {
    Serial.printf("MTU updated: %d for connection ID: %d\n", MTU, connInfo.getConnHandle());
}

uint32_t ServerCallbacks::onPassKeyDisplay() {
    Serial.printf("Server Passkey Display\n");
    return 123456;
}

void ServerCallbacks::onConfirmPassKey(NimBLEConnInfo& connInfo, uint32_t pin) {
    Serial.printf("The passkey YES/NO number: %" PRIu32 "\n", pin);
    /** Inject false if passkeys don't match. */
    NimBLEDevice::injectConfirmPasskey(connInfo, true);
}

void ServerCallbacks::onAuthenticationComplete(NimBLEConnInfo& connInfo) {
    Serial.println("Authentication pairing complete");
}


// Characteristic Callbacks:
void CharacteristicCallbacks::onRead(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) {
    Serial.printf("%s : onRead(), value: %s\n", 
        pCharacteristic->getUUID().toString().c_str(),
        pCharacteristic->getValue().c_str());
}

void CharacteristicCallbacks::onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) {
    if (_on_write_callback) {
        _on_write_callback(pCharacteristic);
    }
}

void CharacteristicCallbacks::onSubscribe(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo, uint16_t subValue) {
    std::string str  = "Client ID: ";
    str             += connInfo.getConnHandle();
    str             += " Address: ";
    str             += connInfo.getAddress().toString();
    if (subValue == 0) {
        str += " Unsubscribed to ";
    } else if (subValue == 1) {
        str += " Subscribed to notifications for ";
    } else if (subValue == 2) {
        str += " Subscribed to indications for ";
    } else if (subValue == 3) {
        str += " Subscribed to notifications and indications for ";
    }
    str += pCharacteristic->getUUID().toString();

    Serial.printf("%s\n", str.c_str());
}

void CharacteristicCallbacks::setOnWriteCallback(OnWriteCallBackFunction callback) {
    _on_write_callback = callback;
}