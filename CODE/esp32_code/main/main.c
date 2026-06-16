#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_spp_api.h"

#define LED_PIN 15
#define SPP_SERVER_NAME "RoboRI"

// Defonire pini driver motoare
#define PIN_MOTOR_A_1A 2
#define PIN_MOTOR_A_1B 4
#define PIN_MOTOR_B_1A 13
#define PIN_MOTOR_B_1B 12

// Defonire pini citire encodere
#define PIN_ENCODER_A 14
#define PIN_ENCODER_B 27

// Configurare PWM (LEDC)
#define LEDC_MODE               LEDC_LOW_SPEED_MODE
#define LEDC_TIMER              LEDC_TIMER_0
#define LEDC_DUTY_RES           LEDC_TIMER_10_BIT  // Rezolutie de la 0 la 1023
#define LEDC_FREQUENCY          5000               // Frecventa de 5 kHz pentru motoare

// Canalele PWM alocate fiecarui pin
#define CH_MOTOR_A_1A           LEDC_CHANNEL_0
#define CH_MOTOR_A_1B           LEDC_CHANNEL_1
#define CH_MOTOR_B_1A           LEDC_CHANNEL_2
#define CH_MOTOR_B_1B           LEDC_CHANNEL_3

static const char *TAG = "ROBOT_BT";

typedef struct {
    float rotatie;
    float miscare;
} ComandaTraseu;

static QueueHandle_t comenzi_queue = NULL;

static volatile int pulse_count_A = 0;
static volatile int pulse_count_B = 0;

static void IRAM_ATTR encoder_A_isr_handler(void* arg) {
    pulse_count_A++;
}

static void IRAM_ATTR encoder_B_isr_handler(void* arg) {
    pulse_count_B++;
}

// Functie auxiliara pentru controlul fin al vitezei (-1023 -> 1023)
void seteaza_viteza_motoare(int viteza_A, int viteza_B) {
    // Control Motor A
    if (viteza_A > 0) {
        ledc_set_duty(LEDC_MODE, CH_MOTOR_A_1A, viteza_A);
        ledc_set_duty(LEDC_MODE, CH_MOTOR_A_1B, 0);
    } else {
        ledc_set_duty(LEDC_MODE, CH_MOTOR_A_1A, 0);
        ledc_set_duty(LEDC_MODE, CH_MOTOR_A_1B, -viteza_A);
    }
    ledc_update_duty(LEDC_MODE, CH_MOTOR_A_1A);
    ledc_update_duty(LEDC_MODE, CH_MOTOR_A_1B);

    // Control Motor B
    if (viteza_B > 0) {
        ledc_set_duty(LEDC_MODE, CH_MOTOR_B_1A, viteza_B);
        ledc_set_duty(LEDC_MODE, CH_MOTOR_B_1B, 0);
    } else {
        ledc_set_duty(LEDC_MODE, CH_MOTOR_B_1A, 0);
        ledc_set_duty(LEDC_MODE, CH_MOTOR_B_1B, -viteza_B);
    }
    ledc_update_duty(LEDC_MODE, CH_MOTOR_B_1A);
    ledc_update_duty(LEDC_MODE, CH_MOTOR_B_1B);
}

static void robot_task(void *pvParameters) {
    ComandaTraseu comanda;
    
    while (1) {
        if (xQueueReceive(comenzi_queue, &comanda, portMAX_DELAY) == pdTRUE) {
            
            if (comanda.miscare == -1.0f) {
                ESP_LOGI(TAG, "========================================");
                ESP_LOGI(TAG, ">>> SUCCES: Executia traseului s-a terminat complet! <<<");
                ESP_LOGI(TAG, "========================================");
                continue;
            }

            // ========================================================
            // CONFIGURARE CONSTANTE SI VITEZA
            // ========================================================
            int PUTERE_BAZA = 750;          
            float IMPULSURI_PER_CM = 6.0f;  

            // Am urcat valoarea de la 6.5 la 18.0 ca sa se roteasca mai mult.
            // Daca nu se intoarce complet "cu totul" inapoi, mai urca valoarea asta (ex: 22.0f)
            float IMPULSURI_PER_RADIAN = 9.0f; 

            int target_pulses_rotire = abs((int)(comanda.rotatie * IMPULSURI_PER_RADIAN));
            int target_pulses_miscare = (int)(comanda.miscare * IMPULSURI_PER_CM);

            ESP_LOGI(TAG, "Execut -> R: %.2f rad (%d p), M: %.2f cm (%d p)", 
                     comanda.rotatie, target_pulses_rotire, comanda.miscare, target_pulses_miscare);
            
            // --- ETAPA 1: ROTIRE ROBOT (DIRECȚIE INVERSATĂ SOFTWARE) ---
            if (target_pulses_rotire > 0) {
                pulse_count_A = 0;
                pulse_count_B = 0;
                bool motorA_activ = true;
                bool motorB_activ = true;

                // Am inversat semnele de viteza aici pentru a repara stanga/dreapta
                if (comanda.rotatie > 0) {
                    seteaza_viteza_motoare(-PUTERE_BAZA, PUTERE_BAZA); 
                } else {
                    seteaza_viteza_motoare(PUTERE_BAZA, -PUTERE_BAZA); 
                }

                int timeout_rotire = 0;
                while ((motorA_activ || motorB_activ) && timeout_rotire < 300) {
                    
                    if (pulse_count_A >= target_pulses_rotire && motorA_activ) {
                        if (comanda.rotatie > 0) {
                            ledc_set_duty(LEDC_MODE, CH_MOTOR_A_1B, 0);
                        } else {
                            ledc_set_duty(LEDC_MODE, CH_MOTOR_A_1A, 0);
                        }
                        ledc_update_duty(LEDC_MODE, CH_MOTOR_A_1A); 
                        ledc_update_duty(LEDC_MODE, CH_MOTOR_A_1B);
                        motorA_activ = false;
                    }

                    if (pulse_count_B >= target_pulses_rotire && motorB_activ) {
                        if (comanda.rotatie > 0) {
                            ledc_set_duty(LEDC_MODE, CH_MOTOR_B_1A, 0);
                        } else {
                            ledc_set_duty(LEDC_MODE, CH_MOTOR_B_1B, 0);
                        }
                        ledc_update_duty(LEDC_MODE, CH_MOTOR_B_1A); 
                        ledc_update_duty(LEDC_MODE, CH_MOTOR_B_1B);
                        motorB_activ = false;
                    }
                    vTaskDelay(pdMS_TO_TICKS(10));
                    timeout_rotire++;
                }
                seteaza_viteza_motoare(0, 0);
                vTaskDelay(pdMS_TO_TICKS(300));
            }

            // --- ETAPA 2: MERS ÎNAINTE DREPT CU SINCRONIZARE ---
            if (target_pulses_miscare > 0) {
                pulse_count_A = 0;
                pulse_count_B = 0;
                bool motorA_activ = true;
                bool motorB_activ = true;

                int v_A = PUTERE_BAZA;
                int v_B = PUTERE_BAZA;
                seteaza_viteza_motoare(v_A, v_B);

                int timeout_miscare = 0;
                float Kp = 8.0f; 

                while ((motorA_activ || motorB_activ) && timeout_miscare < 500) {
                    
                    if (motorA_activ && motorB_activ) {
                        int eroare = pulse_count_A - pulse_count_B;
                        int corectie = (int)(eroare * Kp);

                        v_A = PUTERE_BAZA - corectie;
                        v_B = PUTERE_BAZA + corectie;

                        if (v_A > 1023) { v_A = 1023; } 
                        if (v_A < 200)  { v_A = 200; }
                        if (v_B > 1023) { v_B = 1023; } 
                        if (v_B < 200)  { v_B = 200; }

                        seteaza_viteza_motoare(v_A, v_B);
                    }

                    if (pulse_count_A >= target_pulses_miscare && motorA_activ) {
                        ledc_set_duty(LEDC_MODE, CH_MOTOR_A_1A, 0);
                        ledc_update_duty(LEDC_MODE, CH_MOTOR_A_1A);
                        motorA_activ = false;
                        if (motorB_activ) {
                            seteaza_viteza_motoare(0, PUTERE_BAZA);
                        }
                    }

                    if (pulse_count_B >= target_pulses_miscare && motorB_activ) {
                        ledc_set_duty(LEDC_MODE, CH_MOTOR_B_1A, 0);
                        ledc_update_duty(LEDC_MODE, CH_MOTOR_B_1A);
                        motorB_activ = false;
                        if (motorA_activ) {
                            seteaza_viteza_motoare(PUTERE_BAZA, 0);
                        }
                    }

                    vTaskDelay(pdMS_TO_TICKS(10));
                    timeout_miscare++;
                }
            }

            seteaza_viteza_motoare(0, 0);
            ESP_LOGI(TAG, "Punct atins. Pulsuri: A=%d, B=%d", pulse_count_A, pulse_count_B);
            vTaskDelay(pdMS_TO_TICKS(400));
        }
    }
}
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
            esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, true);
            break;
        default:
            break;
    }
}

static void esp_spp_cb(esp_spp_cb_event_t event, esp_spp_cb_param_t *param) {
    switch (event) {
        case ESP_SPP_INIT_EVT:
            esp_spp_start_srv(ESP_SPP_SEC_AUTHENTICATE, ESP_SPP_ROLE_SLAVE, 0, SPP_SERVER_NAME);
            break;
        case ESP_SPP_DATA_IND_EVT:
            if (param->data_ind.len > 0) {
                char rx_buffer[256];
                int data_len = param->data_ind.len < 255 ? param->data_ind.len : 255;
                memcpy(rx_buffer, param->data_ind.data, data_len);
                rx_buffer[data_len] = '\0';

                char *saveptr1;
                char *token = strtok_r(rx_buffer, "|", &saveptr1);
                
                while (token != NULL) {
                    if (strstr(token, "END") != NULL) {
                        ComandaTraseu comanda_stop = { .rotatie = 0.0f, .miscare = -1.0f };
                        xQueueSend(comenzi_queue, &comanda_stop, portMAX_DELAY);
                        break;
                    }
                    ComandaTraseu noua_comanda;
                    if (sscanf(token, "R:%f;M:%f", &noua_comanda.rotatie, &noua_comanda.miscare) == 2) {
                        xQueueSend(comenzi_queue, &noua_comanda, portMAX_DELAY);
                    }
                    token = strtok_r(NULL, "|", &saveptr1);
                }
            }
            break;
        default:
            break;
    }
}

void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initializare LED incorporat
    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);

    // INITIALIZARE TIMER PWM PENTRU MOTOARE
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_MODE,
        .duty_resolution  = LEDC_DUTY_RES,
        .timer_num        = LEDC_TIMER,
        .freq_hz          = LEDC_FREQUENCY,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    // INITIALIZARE CELE 4 CANALE PWM PENTRU PINII DE MOTOARE
    ledc_channel_config_t ledc_ch[4] = {
        {.channel=CH_MOTOR_A_1A, .duty=0, .gpio_num=PIN_MOTOR_A_1A, .speed_mode=LEDC_MODE, .hpoint=0, .timer_sel=LEDC_TIMER},
        {.channel=CH_MOTOR_A_1B, .duty=0, .gpio_num=PIN_MOTOR_A_1B, .speed_mode=LEDC_MODE, .hpoint=0, .timer_sel=LEDC_TIMER},
        {.channel=CH_MOTOR_B_1A, .duty=0, .gpio_num=PIN_MOTOR_B_1A, .speed_mode=LEDC_MODE, .hpoint=0, .timer_sel=LEDC_TIMER},
        {.channel=CH_MOTOR_B_1B, .duty=0, .gpio_num=PIN_MOTOR_B_1B, .speed_mode=LEDC_MODE, .hpoint=0, .timer_sel=LEDC_TIMER}
    };
    for(int i = 0; i < 4; i++) {
        ledc_channel_config(&ledc_ch[i]);
    }

    // Configurare pini encodere
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_POSEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << PIN_ENCODER_A) | (1ULL << PIN_ENCODER_B),
        .pull_down_en = 0,
        .pull_up_en = 1
    };
    gpio_config(&io_conf);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(PIN_ENCODER_A, encoder_A_isr_handler, (void*) PIN_ENCODER_A);
    gpio_isr_handler_add(PIN_ENCODER_B, encoder_B_isr_handler, (void*) PIN_ENCODER_B);

    comenzi_queue = xQueueCreate(20, sizeof(ComandaTraseu));
    xTaskCreatePinnedToCore(robot_task, "robot_task", 4096, NULL, 5, NULL, 1);

    // Pornire Bluetooth Stack
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT));
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());
    ESP_ERROR_CHECK(esp_bt_gap_register_callback(esp_bt_gap_cb));
    ESP_ERROR_CHECK(esp_bt_gap_set_device_name(SPP_SERVER_NAME));

    esp_bt_cod_t cod = {.major = 0b00111, .minor = 0b000100, .service = 0b00000010000};
    esp_bt_gap_set_cod(cod, ESP_BT_SET_COD_ALL);

    esp_bt_sp_param_t param_type = ESP_BT_SP_IOCAP_MODE;
    esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_NONE; 
    esp_bt_gap_set_security_param(param_type, &iocap, sizeof(iocap));

    ESP_ERROR_CHECK(esp_spp_register_callback(esp_spp_cb));
    esp_spp_cfg_t spp_cfg = {.mode = ESP_SPP_MODE_CB, .enable_l2cap_ertm = false, .tx_buffer_size = 0};
    ESP_ERROR_CHECK(esp_spp_enhanced_init(&spp_cfg));
    ESP_ERROR_CHECK(esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE));

    ESP_LOGI(TAG, "Sistemul PWM si BT a pornit complet.");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}