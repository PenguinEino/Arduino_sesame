#define LOG_LOCAL_LEVEL  ESP_LOG_DEBUG  // this overrides CONFIG_LOG_MAXIMUM_LEVEL setting in menuconfig
#include "esp_log.h"
#include <NimBLEDevice.h>
#include "candy.h"
#include "ssm.h"
#include "ssm_cmd.h"

static const char * TAG = "sesame5_ble.ino";

uint8_t ssm_address_u[] = {0xC7, 0x83, 0x84, 0x39, 0x61, 0xBA}; //{0xba, 0x61, 0x39, 0x84, 0x83, 0xc7}
uint8_t ssm_chr_uuid_u[] = {0x3e, 0x99, 0x76, 0xc6, 0xb4, 0xdb, 0xd3, 0xb6, 0x56, 0x98, 0xae, 0xa5, 0x02, 0x00, 0x86, 0x16};
uint8_t ssm_ntf_uuid_u[] = {0x3e, 0x99, 0x76, 0xc6, 0xb4, 0xdb, 0xd3, 0xb6, 0x56, 0x98, 0xae, 0xa5, 0x03, 0x00, 0x86, 0x16};
uint8_t ssm_target_addr[] = {0xC7, 0x83, 0x84, 0x39, 0x61, 0xBA}; //{0xba, 0x61, 0x39, 0x84, 0x83, 0xc7}; // 接続する ssm のアドレスを逆順に記述
uint8_t ssm_target_secret[] = {0xa7, 0x5d, 0xbc, 0xcf, 0xcd, 0xcc, 0xed, 0xdd, 0x39, 0xbd, 0x82, 0x81, 0xf1, 0x14, 0xc9, 0x9d}; // 接続する ssm のシークレットキーを記述


static NimBLEAddress ssm_address(ssm_address_u);
static NimBLEUUID ssm_svc_uuid((uint16_t)0xFD81); // https://github.com/CANDY-HOUSE/Sesame_BluetoothAPI_document/blob/master/SesameOS3/1_advertising.md
static NimBLEUUID ssm_chr_uuid(ssm_chr_uuid_u, sizeof(ssm_chr_uuid_u)/sizeof(ssm_chr_uuid_u[0]), false);
static NimBLEUUID ssm_ntf_uuid(ssm_ntf_uuid_u, sizeof(ssm_ntf_uuid_u)/sizeof(ssm_ntf_uuid_u[0]), false);
static NimBLEUUID ssm_dsc_uuid((uint16_t)0x2902);

void scanEndedCB(NimBLEScanResults results);

// 接続するアドバタイズ中の sesame 5 を登録する
static NimBLEAdvertisedDevice* advDevice;
// 接続した sesame 5 を登録する
static NimBLEClient* pClient = nullptr;

static bool doConnect = false;
static uint32_t scanTime = 0; /** 0 = scan forever */


// アドバタイズが受信されたときに処理するコールバック。NimBLEAdvertisedDeviceCallbacksの onResult は新しいスキャン結果が検出されたときに呼び出される。
// https://h2zero.github.io/esp-nimble-cpp/class_nim_b_l_e_advertised_device_callbacks.html
class AdvertisedDeviceCallbacks: public NimBLEAdvertisedDeviceCallbacks {

    // SesameSDKではアドバタイズの受信を一度パースする関数を挟むが、ArduinoIDEは不要。直接パース済みの内容を渡してくれる。（NimBLEAdvertisedDevice）
    void onResult(NimBLEAdvertisedDevice* advertisedDevice) {
        Serial.println(advertisedDevice->toString().c_str());

        NimBLEAddress addr = advertisedDevice->getAddress(); // addr https://h2zero.github.io/esp-nimble-cpp/class_nim_b_l_e_address.html
        const char* mfg_data = (advertisedDevice->getManufacturerData()).c_str();

        // manufacturedata を見て sesame か判別する。
        // https://github.com/CANDY-HOUSE/Sesame_BluetoothAPI_document/blob/master/SesameOS3/1_advertising.md
        if(mfg_data[0] == 0x5A && mfg_data[1] == 0x05){ // is sesame
          Serial.printf("Find Sesame");
          if(mfg_data[2] == 0x05){ // is sesame 5
            Serial.printf("5\n");
            if(addr.equals(ssm_address)){ // is target sesame
              Serial.printf("that is target\n");
              if (mfg_data[4] == 0x00) {                            // is unregistered
                  Serial.printf("find unregistered SSM[%d] : %s\n", mfg_data[2], addr.toString().c_str());
              } else { // registered SSM
                  Serial.printf("find registered SSM[%d]\n", mfg_data[2]);

                  NimBLEDevice::getScan()->stop();
                  advDevice = advertisedDevice;
                  doConnect = true;
              }
            }
          } else {
            Serial.printf("\n");
          }
        }
        return;
    };
};


/** Notification / Indication receiving handler callback */
void notifyCB(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify){
    Serial.printf("notify received\n");
    // std::string str = (isNotify == true) ? "Notification" : "Indication";
    // str += " from ";
    // /** NimBLEAddress and NimBLEUUID have std::string operators */
    // str += std::string(pRemoteCharacteristic->getRemoteService()->getClient()->getPeerAddress());
    // str += ": Service = " + std::string(pRemoteCharacteristic->getRemoteService()->getUUID());
    // str += ", Characteristic = " + std::string(pRemoteCharacteristic->getUUID());
    // str += ", Value = " + std::string((char*)pData, length);
    // Serial.println(str.c_str());

    Serial.printf("\n");
    for(int i=0; i<length; i++){
      Serial.printf("%X", pData[i]);
    }
    Serial.printf("\n");
    

    ssm_ble_receiver(&p_ssms_env->ssm, pData, length);
}


/** Callback to process the results of the last scan or restart it */
void scanEndedCB(NimBLEScanResults results){
    Serial.println("Scan Ended");
}


/**  None of these are required as they will be handled by the library with defaults. **
 **                       Remove as you see fit for your needs                        */
class ClientCallbacks : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient* pClient) {
        Serial.println("Connected");
        /** After connection we should change the parameters if we don't need fast response times.
         *  These settings are 150ms interval, 0 latency, 450ms timout.
         *  Timeout should be a multiple of the interval, minimum is 100ms.
         *  I find a multiple of 3-5 * the interval works best for quick response/reconnect.
         *  Min interval: 120 * 1.25ms = 150, Max interval: 120 * 1.25ms = 150, 0 latency, 60 * 10ms = 600ms timeout
         */
        pClient->updateConnParams(120,120,0,60);
    };

    void onDisconnect(NimBLEClient* pClient) {
        Serial.print(pClient->getPeerAddress().toString().c_str());
        Serial.println(" Disconnected - Starting scan");
        NimBLEDevice::getScan()->start(scanTime, scanEndedCB);
    };

    /** Called when the peripheral requests a change to the connection parameters.
     *  Return true to accept and apply them or false to reject and keep
     *  the currently used parameters. Default will return true.
     */
    bool onConnParamsUpdateRequest(NimBLEClient* pClient, const ble_gap_upd_params* params) {
        if(params->itvl_min < 24) { /** 1.25ms units */
            return false;
        } else if(params->itvl_max > 40) { /** 1.25ms units */
            return false;
        } else if(params->latency > 2) { /** Number of intervals allowed to skip */
            return false;
        } else if(params->supervision_timeout > 100) { /** 10ms units */
            return false;
        }

        return true;
    };
};


static ClientCallbacks clientCB;


// 接続をする関数
// NimBLEDevice : BLEデバイス全般に関するクラス : https://h2zero.github.io/esp-nimble-cpp/class_nim_b_l_e_device.html
// NimBLEClient : 接続するデバイスに関するクラス。デバイスが上層にあるイメージ？ : https://h2zero.github.io/esp-nimble-cpp/class_nim_b_l_e_client.html
bool connectToServer() {
    if(!pClient) {
        if(NimBLEDevice::getClientListSize() >= NIMBLE_MAX_CONNECTIONS) {
            Serial.println("Max clients reached - no more connections available");
            return false;
        }

        pClient = NimBLEDevice::createClient();

        Serial.println("New client created");

        // イベントを受信したときに呼び出されるコールバック。SesameSDKでの ble_gap_event_connect_handle に相当
        // https://h2zero.github.io/esp-nimble-cpp/class_nim_b_l_e_client_callbacks.html

        pClient->setClientCallbacks(&clientCB, false);

        // https://h2zero.github.io/esp-nimble-cpp/class_nim_b_l_e_client.html#a17718339f76eb621db0d7919c73b9267
        //  Min interval: 12 * 1.25ms = 15, Max interval: 12 * 1.25ms = 15, 0 latency, 51 * 10ms = 510ms timeout
        pClient->setConnectionParams(12,12,0, 300);
        // 接続完了するまでのタイムアウト[seconds]
        pClient->setConnectTimeout(5);


        if (!pClient->connect(advDevice)) { // 接続失敗
            NimBLEDevice::deleteClient(pClient); // peer から削除
            Serial.println("Failed to connect, deleted client");
            return false;
        }
    }

    if(!pClient->isConnected()) { // 接続に失敗している。（切断された）
        if (!pClient->connect(advDevice)) {
            Serial.println("Failed to connect");
            return false;
        }
    }


    Serial.print("Connected to: ");
    Serial.println(pClient->getPeerAddress().toString().c_str());
    Serial.print("RSSI: ");
    Serial.println(pClient->getRssi());

    p_ssms_env->ssm.device_status = SSM_CONNECTED;        // set the device status
    p_ssms_env->ssm.conn_id = pClient->getConnId(); // save the connection handle

    return true;
}


int ssm_enable_notify(void){
    NimBLERemoteService* pSvc = nullptr;
    NimBLERemoteCharacteristic* pChr = nullptr;
    NimBLERemoteDescriptor* pDsc = nullptr;

    int rc;
    // descriptor に 以下の値を書き込むと notify が有効化されるらしい。成功すると descriptor の value が Notifications enabled になる。
    uint8_t value[2] = { 0x01, 0x00 };

    pSvc = pClient->getService(ssm_svc_uuid);
    if(!pSvc){
      Serial.printf("failed to get service\n");
      goto err;
    }
    pChr = pSvc->getCharacteristic(ssm_ntf_uuid);
    if(!pChr){
      Serial.printf("failed to get characteristic\n");
      goto err;
    }

    pDsc = pChr->getDescriptor(ssm_dsc_uuid);
    if(!pDsc){
      Serial.printf("failed to get descriptor\n");
      goto err;
    }

    rc = pDsc->writeValue(value, true);
    if(!rc){
      Serial.printf("failed to subscribe to characteristic\n");
      goto err;
    }

    rc = pChr->registerForNotify(notifyCB, true, true);
    if(!rc){
      Serial.printf("failed to subscribe to characteristic\n");
      goto err;
    }

    Serial.printf("Enable notify Succesfully!\n");
    return 1;
err:
    return pClient->disconnect();
}


void esp_ble_init(){
    NimBLEDevice::init("");
    NimBLEDevice::setSecurityAuth(/*BLE_SM_PAIR_AUTHREQ_BOND | BLE_SM_PAIR_AUTHREQ_MITM |*/ BLE_SM_PAIR_AUTHREQ_SC);
    /** Optional: set the transmit power, default is 3db */
#ifdef ESP_PLATFORM
    NimBLEDevice::setPower(ESP_PWR_LVL_P9); /** +9db */
#else
    NimBLEDevice::setPower(9); /** +9db */
#endif

    // スキャンのインスタンスを作成
    NimBLEScan* pScan = NimBLEDevice::getScan();

    // スキャンされたデバイスを処理するコールバックを登録
    pScan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks());

    // sesameSDKと合わせている。blecent_scan(void)
    //https://h2zero.github.io/esp-nimble-cpp/class_nim_b_l_e_scan.html

    pScan->setDuplicateFilter(true);
    pScan->setActiveScan(true);
    pScan->setInterval(45);
    pScan->setWindow(15);
    pScan->setFilterPolicy(0); //ここらへんいじれば接続速度変わりそう
    pScan->setLimitedOnly(false);

    // sesameSDKではデバイス検出とスキャン終了のコールバックは統合されているが、ArduinoIDEは違う模様
    Serial.printf("Starting Scan\n");
    pScan->start(scanTime, scanEndedCB);
}


void esp_ble_gatt_write(sesame * ssm, uint8_t * value, uint16_t length) {
    NimBLERemoteService* pSvc = nullptr;
    NimBLERemoteCharacteristic* pChr = nullptr;

    if (!pClient || !pClient->isConnected()) {
        Serial.println("Client not connected");
        return;
    }

    pSvc = pClient->getService(ssm_svc_uuid);
    if(!pSvc){
      Serial.printf("failed to get service\n");
      return;
    }
    Serial.printf("got service\n");

    pChr = pSvc->getCharacteristic(ssm_chr_uuid);
    Serial.printf("ここ\n");
    if(!pChr){
      Serial.printf("failed to get characteristic\n");
      return;
    }
    Serial.printf("got chr");

    Serial.printf("write\n");
    int rc = pChr->writeValue(value, true);
    if (rc != 0) {
        Serial.printf("Error: Failed to write to the characteristic; rc=%d\n", rc);
        return;
    }

    Serial.printf("write succeded\n");
    Serial.printf("\n");
    for(int i=0; i<length; i++){
      Serial.printf("%X", value[i]);
    }
    Serial.printf("\n");
    return;
}


static void ssm_action_handle(sesame * ssm) {
    if (ssm->device_status == SSM_UNLOCKED) {
        send_read_history_cmd_to_ssm(ssm);
        ssm_lock(NULL, 0);
    }
}


void setup (){
    Serial.begin(115200);
    Serial.println("Starting NimBLE Client");
    esp_log_level_set(TAG, ESP_LOG_VERBOSE);
    ssm_init(ssm_action_handle);
    memcpy(p_ssms_env->ssm.addr, ssm_target_addr, 6);
    memcpy(p_ssms_env->ssm.device_secret, ssm_target_secret, 16);
    esp_ble_init();
}


void loop (){
    // 接続フラグが立つまで待機
    while(!doConnect){
        delay(1);
    }

    doConnect = false;

    // 接続する
    Serial.printf("Connect SSM addr=%s addrType=%d\n", advDevice->getAddress().toString().c_str(), advDevice->getAddressType());
    if(connectToServer()) {
        if(ssm_enable_notify()){
            send_login_cmd_to_ssm(&p_ssms_env->ssm);
        }
    } else {
        Serial.println("Failed to connect, starting scan");
        NimBLEDevice::getScan()->start(scanTime, scanEndedCB);
    }

}
