#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_sleep.h"
#include "protocol_examples_common.h"

#include "driver/adc.h"
#include "driver/gpio.h"
#include "esp_adc_cal.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"
#include "cJSON.h"

#define URI ""                      // ThingsBoard server URI
#define TOKEN ""                    // ThingsBoard token

#define DEFAULT_VREF 1100
#define SENSITIVITY 3.16
#define REF_SPL 80
#define NO_OF_SAMPLES   64          // Multisampling
#define uS_TO_S_FACTOR 1000000		// Conversion factor for micro seconds to seconds
#define SLEEP_TIME 1 * 60			// Time to sleep in seconds

static esp_adc_cal_characteristics_t *adc_chars;

static const char *TAG = "SBC_G8_STATION";
const TickType_t xDelay = 1000 / portTICK_PERIOD_MS;
unsigned long sampletime_ms = 500;

void setup_adc() {
    // Inicializa la ADC1    
    adc1_config_width(ADC_WIDTH_BIT_10);                        // Configura los canales    
    adc1_config_channel_atten(ADC1_CHANNEL_0,ADC_ATTEN_DB_0);   // Sensor de calidad del aire GPIO 36    
    adc1_config_channel_atten(ADC1_CHANNEL_3,ADC_ATTEN_DB_0);   // Sensor de sonido GPIO 39    
    adc1_config_channel_atten(ADC1_CHANNEL_6,ADC_ATTEN_DB_0);   // Sensor de uv GPIO 34
}

uint32_t read_air_quality(void) {

    uint32_t adc_readingAir = 0;

    // Sampling
    for (int i = 0; i < NO_OF_SAMPLES; i++)
        adc_readingAir += adc1_get_raw((adc1_channel_t)ADC1_CHANNEL_0);
    
    adc_readingAir /= NO_OF_SAMPLES;
    
    return adc_readingAir;

}

double read_noise(void) {

    uint32_t adc_readingSound = 0;
    uint32_t voltageSound = 0;
    double db = 0.00;
    double dB_current = 0.0;
    double voltageConver = 0.0;

    // Sampling
    for (int i = 0; i < NO_OF_SAMPLES; i++)
        adc_readingSound += adc1_get_raw((adc1_channel_t)ADC1_CHANNEL_3);

    adc_readingSound /= NO_OF_SAMPLES;
    
    // Convert adc_reading to voltage in mV
    voltageSound = esp_adc_cal_raw_to_voltage(adc_readingSound, adc_chars);

    voltageConver = (double)voltageSound / 1000;
    dB_current = voltageConver / SENSITIVITY;
    db = REF_SPL + (20 * log10(dB_current));

    return db;

}

double read_ultraviolet(void) {

    uint32_t adc_readingUV = 0;
    uint32_t voltageUV = 0;
    double voltageUVCon = 0.0;
    double uvr = 0.0;

    // Sampling
    for (int i = 0; i < NO_OF_SAMPLES; i++)
        adc_readingUV += adc1_get_raw((adc1_channel_t)ADC1_CHANNEL_6);

    adc_readingUV /= NO_OF_SAMPLES;

    // Convert adc_reading to voltage in mV
    voltageUV = esp_adc_cal_raw_to_voltage(adc_readingUV, adc_chars);

    voltageUVCon = (double)voltageUV / 1000;

    uvr = voltageUVCon / .1;
    if (uvr <= 0.750)
        uvr = 0;

    return uvr;

}

static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event) {

    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED");
            break;

        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED");
            break;

        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED");
            break;

        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            printf("DATA=%.*s\r\n", event->data_len, event->data);
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            break;

        default:
            ESP_LOGI(TAG, "Other event id:%d", event->event_id);
            break;

    }

    return ESP_OK;

}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    mqtt_event_handler_cb(event_data);
}

static esp_mqtt_client_handle_t mqtt_app_register(void) {

    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = URI,
        .event_handle = mqtt_event_handler,
        .port = 1883,
        .username = TOKEN,
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);

    return client;

}

static void mqtt_publish_tb(void) {

    uint32_t air_quality_val;
    double noise_val;
    double uvr_val;
    char *json_air, *json_noise, *json_uv;

    // Crea JSON
    cJSON *uv = cJSON_CreateObject();
    cJSON *air = cJSON_CreateObject();
    cJSON *sound = cJSON_CreateObject();

    esp_mqtt_client_handle_t client = mqtt_app_register();

    // Configure ADC
    setup_adc();

    //Characterize ADC
    adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_0, ADC_WIDTH_BIT_10, DEFAULT_VREF, adc_chars);

    esp_mqtt_client_start(client);
    vTaskDelay(xDelay);

    air_quality_val = read_air_quality();
    noise_val = read_noise();
    uvr_val = read_ultraviolet();

    // Añade los datos al JSON        
    cJSON_AddNumberToObject(air, "air_qual", air_quality_val);        
    cJSON_AddNumberToObject(sound, "noise", noise_val);        
    cJSON_AddNumberToObject(uv, "uv", uvr_val);
        
    // Asigna los JSON sin formatear a un string
    json_air = cJSON_PrintUnformatted(air);
    json_noise = cJSON_PrintUnformatted(sound);
    json_uv = cJSON_PrintUnformatted(uv);
       
    // Envia los datos al TB        
    esp_mqtt_client_publish(client, "v1/devices/me/telemetry", json_air, 0, 1, 0);      
    cJSON_Delete(air);
    free(json_air);
    vTaskDelay(xDelay);

    esp_mqtt_client_publish(client, "v1/devices/me/telemetry", json_noise, 0, 1, 0);        
    cJSON_Delete(sound);
    free(json_noise);
    vTaskDelay(xDelay);

    esp_mqtt_client_publish(client, "v1/devices/me/telemetry", json_uv, 0, 1, 0);
    cJSON_Delete(uv);
    free(json_uv);
    vTaskDelay(xDelay);

    esp_mqtt_client_disconnect(client);
    vTaskDelay(xDelay);
    esp_mqtt_client_destroy(client);

}

void app_main(void) {

    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
    esp_log_level_set("MQTT_EXAMPLE", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_TCP", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_SSL", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
    esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_ERROR_CHECK(example_connect());

    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_OFF);     // Disable fast memory
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_OFF);     // Disable slow memory
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);		// Disable RTC peripherals
    esp_sleep_enable_timer_wakeup(uS_TO_S_FACTOR * SLEEP_TIME);			    // Just the internal Real Time Clock must be enabled

    mqtt_publish_tb();
    esp_deep_sleep_start();		// Hibernate

}
