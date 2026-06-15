#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_spp_api.h"

#define LED_PIN 2
#define SPP_SERVER_NAME "RoboRI"

static const char *TAG = "ROBOT_BT";

// Callback pentru managementul securitatii si vizibilitatii (GAP)
static void esp_bt_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
    switch (event) {
        case ESP_BT_GAP_AUTH_CMPL_EVT:
            if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
                ESP_LOGI(TAG, "Autentificare reusita cu succes!");
            } else {
                ESP_LOGE(TAG, "Autentificare esuata, status: %d", param->auth_cmpl.stat);
            }
            break;

        case ESP_BT_GAP_CFM_REQ_EVT:
            // !!! AICI E CHEIA: Windows cere confirmarea codului. Ii raspundem automat cu TRUE.
            ESP_LOGI(TAG, "Cerere imperechere de la Windows. Acceptam automat...");
            esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, true);
            break;

        case ESP_BT_GAP_KEY_NOTIF_EVT:
            ESP_LOGI(TAG, "Cod cheie generat: %lu", (unsigned long)param->key_notif.passkey);
            break;

        default:
            break;
    }
}

// Callback-ul principal pentru serverul SPP (Date si Conexiuni)
static void esp_spp_cb(esp_spp_cb_event_t event, esp_spp_cb_param_t *param) {
    switch (event) {
        case ESP_SPP_INIT_EVT:
            // Pornim serverul cu criptare activa (OBLIGATORIU pentru Windows 11)
            esp_spp_start_srv(ESP_SPP_SEC_AUTHENTICATE, ESP_SPP_ROLE_SLAVE, 0, SPP_SERVER_NAME);
            ESP_LOGI(TAG, "Serverul SPP este gata si asteapta conexiuni.");
            break;

        case ESP_SPP_SRV_OPEN_EVT:
            ESP_LOGI(TAG, ">>> Windows s-a conectat cu succes la portul serial virtual!");
            break;

        case ESP_SPP_CLOSE_EVT:
            ESP_LOGW(TAG, ">>> Dispozitiv deconectat. Portul COM a fost inchis.");
            break;

        case ESP_SPP_DATA_IND_EVT:
            if (param->data_ind.len > 0) {
                ESP_LOGI(TAG, "Date primite (%d bytes): %.*s", 
                         param->data_ind.len, param->data_ind.len, param->data_ind.data);
                
                if (strncmp((char*)param->data_ind.data, "TEST", 4) == 0) {
                    ESP_LOGI(TAG, "Comanda primita! Aprind LED-ul.");
                    gpio_set_level(LED_PIN, 1);
                    // NOTA: NU punem vTaskDelay direct in callback-ul de BT deoarece blocheaza stiva de Bluetooth.
                    // Controlul LED-ului se face rapid. Pentru delay-uri lungi se foloseste un task separat.
                }
            }
            break;

        default:
            break;
    }
}

void app_main(void) {
    // 1. Initializare NVS (Memorarea profilelor BT)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. Configurare GPIO LED
    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_PIN, 0);

    // 3. Pornire Controller BT hardware
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT));

    // 4. Pornire Bluedroid (Stiva software)
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    // 5. Inregistrare Callback GAP si setare identitate hardware
    ESP_ERROR_CHECK(esp_bt_gap_register_callback(esp_bt_gap_cb));
    ESP_ERROR_CHECK(esp_bt_gap_set_device_name(SPP_SERVER_NAME));

    // Seteaza Clasa Dispozitivului ca fiind "Periferic / Port de Date" (Ajuta Windows 11 sa nu-l creada casti audio)
    esp_bt_cod_t cod;
    cod.major = 0b00111; // Capturing / Peripheral major class
    cod.minor = 0b000100;
    cod.service = 0b00000010000;
    esp_bt_gap_set_cod(cod, ESP_BT_SET_COD_ALL);

    // 6. Configurare parametri de securitate SSP (Secure Simple Pairing)
    esp_bt_sp_param_t param_type = ESP_BT_SP_IOCAP_MODE;
    esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_NONE; // Permite confirmarea handshake-ului
    esp_bt_gap_set_security_param(param_type, &iocap, sizeof(iocap));

    // 7. Initializare si inregistrare SPP
    ESP_ERROR_CHECK(esp_spp_register_callback(esp_spp_cb));

    esp_spp_cfg_t spp_cfg = {
        .mode = ESP_SPP_MODE_CB,
        .enable_l2cap_ertm = false,
        .tx_buffer_size = 0
    };
    ESP_ERROR_CHECK(esp_spp_enhanced_init(&spp_cfg));

    // Setam vizibilitatea BT
    ESP_ERROR_CHECK(esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE));

    ESP_LOGI(TAG, "Sistemul a pornit complet.");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}