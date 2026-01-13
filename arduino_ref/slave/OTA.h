#ifndef OTA_H
#define OTA_H

#include <Update.h>
#include "FS.h"
#include "FFat.h"
#include "SPIFFS.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h> 

// ZForce Plate
// GPB100R 100kg H1031534V : 207.70
// DYX-301 100kg : 2231.19
#define FACTOR 2231.19
#define bleServerName "Slave"

#define FORMAT_SPIFFS_IF_FAILED true
#define FORMAT_FFAT_IF_FAILED true

#define USE_SPIFFS  //comment to use FFat

#ifdef USE_SPIFFS
#define FLASH SPIFFS
#define FASTMODE false    //SPIFFS write is slow
#else
#define FLASH FFat
#define FASTMODE true    //FFat is faster
#endif

#define NORMAL_MODE   0   // normal
#define UPDATE_MODE   1   // receiving firmware
#define OTA_MODE      2   // installing firmware

static uint8_t updater[16384];
static uint8_t updater2[16384];

#define SERVICE_UUID              "fb1e4001-54ae-4a28-9f74-dfccb248601d"
#define CHARACTERISTIC_UUID_RX    "fb1e4002-54ae-4a28-9f74-dfccb248601d"
#define CHARACTERISTIC_UUID_TX    "fb1e4003-54ae-4a28-9f74-dfccb248601d"

static BLECharacteristic* pCharacteristicTX;
static BLECharacteristic* pCharacteristicRX;

static bool deviceConnected = false, sendMode = false, sendSize = true;
static bool writeFile = false, request = false;
static int writeLen = 0, writeLen2 = 0;
static bool current = true;
static int parts = 0, next = 0, cur = 0, MTU = 0;
static int MODE = NORMAL_MODE;
static unsigned long rParts, tParts;

static void rebootEspWithReason(String reason);

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer);
    void onDisconnect(BLEServer* pServer);
};

class MyCallbacks: public BLECharacteristicCallbacks {
    void onNotify(BLECharacteristic *pCharacteristic);
    void onWrite(BLECharacteristic *pCharacteristic);
};

static void writeBinary(fs::FS &fs, const char * path, uint8_t *dat, int len);

void sendOtaResult(String result);

void performUpdate(Stream &updateSource, size_t updateSize);

void updateFromFS(fs::FS &fs);

void initBLE();

void fun_NORMAL_MODE();

void fun_UPDATE_MODE();

void fun_OTA_MODE();

void choseSpiffsOrFfat();

#endif