// #define LOG_LOCAL_LEVEL  ESP_LOG_INFO  // this overrides CONFIG_LOG_MAXIMUM_LEVEL setting in menuconfig
#include "esp_log.h"
#include <NimBLEDevice.h>
#include "candy.h"
#include "ssm.h"
#include "ssm_cmd.h"

static const char * TAG = "sesame5_ble.ino";


uint8_t ssm_address_u[] = {0xC7, 0x83, 0x84, 0x39, 0x61, 0xBA}; // 接続する sesame のアドレスを記述
uint8_t ssm_target_secret[] = {0xa7, 0x5d, 0xbc, 0xcf, 0xcd, 0xcc, 0xed, 0xdd, 0x39, 0xbd, 0x82, 0x81, 0xf1, 0x14, 0xc9, 0x9d}; // 接続する sesame のシークレットキーを記述
uint8_t ssm_chr_uuid_u[] = {0x3e, 0x99, 0x76, 0xc6, 0xb4, 0xdb, 0xd3, 0xb6, 0x56, 0x98, 0xae, 0xa5, 0x02, 0x00, 0x86, 0x16};
uint8_t ssm_ntf_uuid_u[] = {0x3e, 0x99, 0x76, 0xc6, 0xb4, 0xdb, 0xd3, 0xb6, 0x56, 0x98, 0xae, 0xa5, 0x03, 0x00, 0x86, 0x16};

// https://github.com/CANDY-HOUSE/Sesame_BluetoothAPI_document/blob/master/SesameOS3/1_advertising.md
static NimBLEAddress ssm_address(ssm_address_u);
static NimBLEUUID ssm_svc_uuid((uint16_t)0xFD81); 
static NimBLEUUID ssm_chr_uuid(ssm_chr_uuid_u, sizeof(ssm_chr_uuid_u)/sizeof(ssm_chr_uuid_u[0]), false);
static NimBLEUUID ssm_ntf_uuid(ssm_ntf_uuid_u, sizeof(ssm_ntf_uuid_u)/sizeof(ssm_ntf_uuid_u[0]), false);
static NimBLEUUID ssm_dsc_uuid((uint16_t)0x2902);

void scanEndedCB(NimBLEScanResults results);

// 接続するアドバタイズ中の sesame 5 を登録する
static NimBLEAdvertisedDevice* advDevice;
// 接続した sesame 5 を登録する
static NimBLEClient* pClient = nullptr;
uint16_t ssm_chr_handle;

static bool doConnect = false;
static uint32_t scanTime = 0; /** 0 = scan forever */


// アドバタイズが受信されたときに処理するコールバック。NimBLEAdvertisedDeviceCallbacksの onResult は新しいスキャン結果が検出されたときに呼び出される。
// https://h2zero.github.io/esp-nimble-cpp/class_nim_b_l_e_advertised_device_callbacks.html
class AdvertisedDeviceCallbacks: public NimBLEAdvertisedDeviceCallbacks {

    // SesameSDKではアドバタイズの受信を一度パースする関数を挟むが、ArduinoIDEは不要。直接パース済みの内容を渡してくれる。（NimBLEAdvertisedDevice）
    void onResult(NimBLEAdvertisedDevice* advertisedDevice) {
        ESP_LOGI(TAG, "%s", advertisedDevice->toString().c_str());

        NimBLEAddress addr = advertisedDevice->getAddress(); // addr https://h2zero.github.io/esp-nimble-cpp/class_nim_b_l_e_address.html
        const char* mfg_data = (advertisedDevice->getManufacturerData()).c_str();

        // manufacturedata を見て sesame か判別する。
        // https://github.com/CANDY-HOUSE/Sesame_BluetoothAPI_document/blob/master/SesameOS3/1_advertising.md
        if(mfg_data[0] == 0x5A && mfg_data[1] == 0x05){ // is sesame
          if(mfg_data[2] == 0x05){ // is sesame 5
            ESP_LOGD(TAG, "Find Sesame 5");
            if(addr.equals(ssm_address)){ // is target sesame
              ESP_LOGD(TAG, "Match the registered address");
              if (mfg_data[4] == 0x00) {                            // is unregistered
                  ESP_LOGD(TAG, "find unregistered SSM[%d] : %s", mfg_data[2], addr.toString().c_str());
              } else { // registered SSM
                  ESP_LOGD(TAG, "find registered SSM[%d]", mfg_data[2]);

                  NimBLEDevice::getScan()->stop();
                  advDevice = advertisedDevice;
                  doConnect = true;
              }
            }
          } else {
            ESP_LOGD(TAG, "Find a Sesame device (not 5)");
          }
        }
        return;
    };
};


/** Notification / Indication receiving handler callback */
void notifyCB(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify){
    std::string str = (isNotify == true) ? "Notification" : "Indication";
    str += " from ";
    /** NimBLEAddress and NimBLEUUID have std::string operators */
    str += std::string(pRemoteCharacteristic->getRemoteService()->getClient()->getPeerAddress());
    str += ": Service = " + std::string(pRemoteCharacteristic->getRemoteService()->getUUID());
    str += ", Characteristic = " + std::string(pRemoteCharacteristic->getUUID());
    ESP_LOGD(TAG, "%s", str.c_str());

    ssm_ble_receiver(&p_ssms_env->ssm, pData, length);
}


/** Callback to process the results of the last scan or restart it */
void scanEndedCB(NimBLEScanResults results){
    ESP_LOGD(TAG, "Scan Ended");
}


/**  None of these are required as they will be handled by the library with defaults. **
 **                       Remove as you see fit for your needs                        */
class ClientCallbacks : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient* pClient) {
        ESP_LOGD(TAG, "Connected");
        /** After connection we should change the parameters if we don't need fast response times.
         *  These settings are 150ms interval, 0 latency, 450ms timout.
         *  Timeout should be a multiple of the interval, minimum is 100ms.
         *  I find a multiple of 3-5 * the interval works best for quick response/reconnect.
         *  Min interval: 120 * 1.25ms = 150, Max interval: 120 * 1.25ms = 150, 0 latency, 60 * 10ms = 600ms timeout
         */
         
        pClient->updateConnParams(120,120,0,60);
    };

    void onDisconnect(NimBLEClient* pClient) {
        ESP_LOGD(TAG, "%s", pClient->getPeerAddress().toString().c_str());
        ESP_LOGD(TAG, " Disconnected - Starting scan");
        NimBLEDevice::getScan()->start(scanTime, scanEndedCB);
    };

    /** Called when the peripheral requests a change to the connection parameters.
     *  Return true to accept and apply them or false to reject and keep
     *  the currently used parameters. Default will return true.
     */
    bool onConnParamsUpdateRequest(NimBLEClient* pClient, const ble_gap_upd_params* params) {
        // if(params->itvl_min < 24) { /** 1.25ms units */
        //     return false;
        // } else if(params->itvl_max > 40) { /** 1.25ms units */
        //     return false;
        // } else if(params->latency > 2) { /** Number of intervals allowed to skip */
        //     return false;
        // } else if(params->supervision_timeout > 100) { /** 10ms units */
        //     return false;
        // }

        // 接続パラメーターのリクエストは全て受け入れる
        return true;
    };
};

// 特に使わない
static ClientCallbacks clientCB;

// 接続をする関数
// NimBLEDevice : BLEデバイス全般に関するクラス : https://h2zero.github.io/esp-nimble-cpp/class_nim_b_l_e_device.html
// NimBLEClient : 接続するデバイスに関するクラス。デバイスが上層にある。 : https://h2zero.github.io/esp-nimble-cpp/class_nim_b_l_e_client.html
bool connectToServer() {
    /** Check if we have a client we should reuse first **/
    if(NimBLEDevice::getClientListSize()) {
        /** Special case when we already know this device, we send false as the
         *  second argument in connect() to prevent refreshing the service database.
         *  This saves considerable time and power.
         */
        pClient = NimBLEDevice::getClientByPeerAddress(advDevice->getAddress());
        if(pClient){
            if(!pClient->connect(advDevice, false)) {
                ESP_LOGE(TAG, "Reconnect failed");
                return false;
            }
            ESP_LOGD(TAG, "Reconnected client");
        }
        /** We don't already have a client that knows this device,
         *  we will check for a client that is disconnected that we can use.
         */
        else {
            pClient = NimBLEDevice::getDisconnectedClient();
        }
    }

    if(!pClient) {
        if(NimBLEDevice::getClientListSize() >= NIMBLE_MAX_CONNECTIONS) {
            ESP_LOGD(TAG, "Max clients reached - no more connections available");
            return false;
        }

        pClient = NimBLEDevice::createClient();
        ESP_LOGD(TAG, "New client created");
        // イベントを受信したときに呼び出されるコールバック。SesameSDKでの ble_gap_event_connect_handle に相当
        // https://h2zero.github.io/esp-nimble-cpp/class_nim_b_l_e_client_callbacks.html
        pClient->setClientCallbacks(&clientCB, false);
        // https://h2zero.github.io/esp-nimble-cpp/class_nim_b_l_e_client.html#a17718339f76eb621db0d7919c73b9267
        //  Min interval: 12 * 1.25ms = 15, Max interval: 12 * 1.25ms = 15, 0 latency, 51 * 10ms = 510ms timeout
        pClient->setConnectionParams(240,400,0, 600);
        // 接続完了するまでのタイムアウト[seconds]
        pClient->setConnectTimeout(5);


        if (!pClient->connect(advDevice)) { // 接続失敗
            NimBLEDevice::deleteClient(pClient); // peer から削除
            ESP_LOGE(TAG, "Failed to connect, deleted client");
            return false;
        }
    }

    if(!pClient->isConnected()) {
        if (!pClient->connect(advDevice)) {
            ESP_LOGE(TAG, "Failed to connect");
            return false;
        }
    }

    ESP_LOGD(TAG, "Connected to %s, RSSI:%d", pClient->getPeerAddress().toString().c_str(), pClient->getRssi());
    p_ssms_env->ssm.device_status = SSM_CONNECTED;        // set the device status
    p_ssms_env->ssm.conn_id = pClient->getConnId(); // save the connection handle

    return true;
}


int ssm_enable_notify(void){
    NimBLERemoteService* pSvc = nullptr;
    NimBLERemoteCharacteristic* pChr = nullptr;
    NimBLERemoteCharacteristic* pChr_w = nullptr;

    int rc;

    pSvc = pClient->getService(ssm_svc_uuid);
    if(!pSvc){
      ESP_LOGE(TAG, "failed to get service");
      goto err;
    }
    
    pChr = pSvc->getCharacteristic(ssm_ntf_uuid);
    if(!pChr){
      ESP_LOGE(TAG, "failed to get characteristic");
      goto err;
    }

    // write 用の characteristic の handle を取得しておく
    pChr_w = pSvc->getCharacteristic(ssm_chr_uuid);
    if(!pChr_w){
      ESP_LOGE(TAG, "failed to get characteristic to write");
      goto err;
    }
    ssm_chr_handle = pChr_w->getHandle();
    ESP_LOGD(TAG, "Characteristic handle: %d\n", ssm_chr_handle);

    rc = pChr->subscribe(true, notifyCB, true);
    if(!rc){
      ESP_LOGE(TAG, "failed to subscribe to characteristic");
      goto err;
    }

    ESP_LOGD(TAG, "Enable notify Succesfully!");


    return 1;
err:
    ESP_LOGE(TAG, "Disconnect from sesame");
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
    pScan->setActiveScan(false);
    pScan->setInterval(0);
    pScan->setWindow(0);
    pScan->setFilterPolicy(0);
    pScan->setLimitedOnly(false);

    // sesameSDKではデバイス検出とスキャン終了のコールバックは統合されているが、ArduinoIDEは違う模様
    pScan->start(scanTime, scanEndedCB);
}


void esp_ble_gatt_write(sesame * ssm, uint8_t * value, uint16_t length) {
    NimBLERemoteCharacteristic* pChr = nullptr;

    if (!pClient || !pClient->isConnected()) {
        ESP_LOGE(TAG, "Client not connected");
        goto err;
    }

    ESP_LOGD(TAG, "Attempting to get characteristic with handle: %d\n", ssm_chr_handle);
    pChr = pClient->getCharacteristic(ssm_chr_handle);
    if(!pChr){
      ESP_LOGE(TAG, "failed to get characteristic");
      goto err;
    }
    ESP_LOGD(TAG, "%s", pChr->toString().c_str());

    // write charasteristic への書き込みは、responseがされない。戻り値は必ず1になるので、エラーと処理してはいけない。（たぶん）
    pChr->writeValue(value, length, false);
    return;
err:
    ESP_LOGE(TAG, "Disconnect from sesame");
    pClient->disconnect();
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
    // esp_log_level_set(TAG, ESP_LOG_);
    ssm_init(ssm_action_handle);
    memcpy(p_ssms_env->ssm.addr, ssm_address_u, 6);
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
    ESP_LOGD(TAG, "Connect SSM addr=%s addrType=%d\n", advDevice->getAddress().toString().c_str(), advDevice->getAddressType());
    if(connectToServer()) {
        ssm_enable_notify();
    } else {
        ESP_LOGE(TAG, "Failed to connect, starting scan");
        NimBLEDevice::getScan()->start(scanTime, scanEndedCB);
    }

}
