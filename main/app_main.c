//
//--------------------------------- INCLUDES ----------------------------------
#include "user_interface.h"
#include "driver/gpio.h"
#include "driver/ledc.h"

#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include "esp_system.h"
#include <esp_wifi.h>
#include "senzor_temp/sht31.h"
#include "driver/i2c.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include <nvs_flash.h>
#include "mqtt_client.h"

#include <stdio.h>
#include <string.h>
#include "cJSON.h"

//---------------------------------- MACROS -----------------------------------
#define EXAMPLE_ESP_WIFI_SSID ("ZICER-guest")
#define EXAMPLE_ESP_WIFI_PASS ("Z1c3r.10020")

#define DELAY_TIME_MS (5000U)
#define BOUNCING_MS (20U)

#define CONFIG_BROKER_URL "mqtt://4gpc.l.time4vps.cloud"
#define MQTT_TOPIC "WES/Saturn/sensors"

#if CONFIG_ESP_WIFI_AUTH_OPEN
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN
#elif CONFIG_ESP_WIFI_AUTH_WEP
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WEP
#elif CONFIG_ESP_WIFI_AUTH_WPA_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WAPI_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WAPI_PSK
#endif

#define BUZZER_GPIO 2
#define BUZZER_CHANNEL LEDC_CHANNEL_0
#define BUZZER_FREQ_HZ 1000

#define GPIO_BUTTON_1 (36U)
#define GPIO_BUTTON_2 (32U)

#define GPIO_BIT_MASK(X) ((1ULL << (X)))
#define GPIO_LED_BLUE (14U)
#define GPIO_LED_RED (26U)
#define GPIO_LED_GREEN (27U)

#define BUZZ_SHORT (100U)
#define BUZZ_LONG (300U)

//-------------------------------- DATA TYPES ---------------------------------

//---------------------- PRIVATE FUNCTION PROTOTYPES --------------------------

static void _event_handler(void *p_arg, esp_event_base_t event_base, int32_t event_id, void *p_event_data);
static void _wifi_init_sta(void);
static void _wifi_init(void);
static esp_err_t _nvs_init(void);

static void _hello_task(void *p_parameter);
static void _buzzer_task(void *p_parameter);
static void _senzor_task(void *p_parameter);

static void mqtt_event_handler();
static void mqtt_app_start(void);

typedef void (*button_pressed_isr_t)(void *p_arg);
static esp_err_t _button_init(uint8_t pin, button_pressed_isr_t p_isr);
static void _btn_1_isr(void *p_arg);
static void _btn_2_isr(void *p_arg);

static void _sos_sequence();
static void turn_on_buzzer();
static void turn_off_buzzer();

static void _led_init(uint8_t pin);
static void _led_on(uint8_t pin);
static void _led_off(uint8_t pin);

//------------------------- STATIC DATA & CONSTANTS ---------------------------
static const char *MQTT_TAG = "MQTT";
static const char *WIFI_TAG = "WIFI";

static EventGroupHandle_t s_wifi_event_group;
static SemaphoreHandle_t semafor;

esp_mqtt_client_handle_t client;
esp_mqtt_client_config_t mqtt_cfg;

TaskHandle_t xHandle = NULL;

//------------------------------- GLOBAL DATA ---------------------------------

//------------------------------ PUBLIC FUNCTIONS -----------------------------

void app_main(void)
{
    _button_init(GPIO_BUTTON_1, _btn_1_isr);
    _button_init(GPIO_BUTTON_2, _btn_2_isr);

    _led_init(GPIO_LED_BLUE);
    _led_init(GPIO_LED_RED);
    _led_init(GPIO_LED_GREEN);

    semafor = xSemaphoreCreateBinary();
    user_interface_init();
    vTaskDelay(DELAY_TIME_MS / 5 / portTICK_PERIOD_MS);
    _wifi_init();

    int index = 0;

    xTaskCreate(&_hello_task, "hello_task", 4 * 1024, NULL, 5, NULL);
    xTaskCreate(&_senzor_task, "senzor_task", 2 * 1024, NULL, 5, NULL);

    for (;;)
    {
        printf("[%d] WiFi Example!\n", index);
        index++;
        vTaskDelay(DELAY_TIME_MS / portTICK_PERIOD_MS);
    }
}

//---------------------------- PRIVATE FUNCTIONS ------------------------------

static void mqtt_app_start(void)
{
    mqtt_cfg = (esp_mqtt_client_config_t){
        .broker.address.uri = CONFIG_BROKER_URL,
    };
    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}

static void _hello_task(void *p_parameter)
{
    xSemaphoreTake(semafor, portMAX_DELAY);
    mqtt_app_start();
    for (;;)
    {
        vTaskDelay(DELAY_TIME_MS / portTICK_PERIOD_MS);
    }
}

static void _buzzer_task(void *p_parameter)
{
    esp_rom_gpio_pad_select_gpio(BUZZER_GPIO);
    gpio_set_direction(BUZZER_GPIO, GPIO_MODE_OUTPUT);

    ledc_timer_config_t ledc_timer = {.speed_mode = LEDC_HIGH_SPEED_MODE, .duty_resolution = LEDC_TIMER_8_BIT, .timer_num = LEDC_TIMER_0, .freq_hz = BUZZER_FREQ_HZ};
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {
        .gpio_num = BUZZER_GPIO,
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .channel = BUZZER_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0 // Duty cycle (0-255), adjust as needed
    };
    ledc_channel_config(&ledc_channel);

    for (;;)
    {
        _sos_sequence();
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

static void _senzor_task(void *p_parameter)
{
    sht31_init();
    float temp, humi;
    for (;;)
    {
        vTaskDelay(DELAY_TIME_MS / portTICK_PERIOD_MS);
        sht31_read_temp_humi(&temp, &humi);
        printf("temp: %f, humi: %f\n", temp, humi);

        cJSON *root = cJSON_CreateObject();
        cJSON_AddNumberToObject(root, "temp", (int)temp);
        cJSON_AddNumberToObject(root, "hum", (int)humi);
        cJSON *acc = cJSON_CreateObject();
        cJSON_AddNumberToObject(acc, "x", 1.0);
        cJSON_AddNumberToObject(acc, "y", 1.0);
        cJSON_AddNumberToObject(acc, "z", 1.0);
        cJSON_AddItemToObject(root, "acc", acc);
        char *json_str = cJSON_Print(root);
        cJSON_Delete(root);
        esp_mqtt_client_publish(client, MQTT_TOPIC, json_str, 0, 1, 0);
        free(json_str);
    }
}

static void _wifi_init(void)
{
    _nvs_init();
    _wifi_init_sta();
}

static esp_err_t _nvs_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if ((ESP_ERR_NVS_NO_FREE_PAGES == ret) || (ESP_ERR_NVS_NEW_VERSION_FOUND == ret))
    {
        ret = nvs_flash_erase();
        ret |= nvs_flash_init();
    }
    return ret;
}

static void _wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &_event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &_event_handler, NULL, &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(WIFI_TAG, "wifi_init_sta finished.");
}

//---------------------------- INTERRUPT HANDLERS -----------------------------

static void _event_handler(void *p_arg, esp_event_base_t event_base, int32_t event_id, void *p_event_data)
{
    if ((WIFI_EVENT == event_base) && (WIFI_EVENT_STA_START == event_id))
    {
        ESP_LOGI(WIFI_TAG, "Try to connect.");
        esp_wifi_connect();
    }
    else if ((IP_EVENT == event_base) && (IP_EVENT_STA_GOT_IP == event_id))
    {
        ip_event_got_ip_t *p_event = (ip_event_got_ip_t *)p_event_data;
        ESP_LOGI(WIFI_TAG, "CONNECTED with IP Address:" IPSTR, IP2STR(&p_event->ip_info.ip));
        xSemaphoreGive(semafor);
    }
    else if ((IP_EVENT == event_base))
    {
        printf("eventid: %ld\n", event_id);
    }
    else if ((WIFI_EVENT == event_base) && (WIFI_EVENT_STA_DISCONNECTED == event_id))
    {
        ESP_LOGI(WIFI_TAG, "DISCONNECTED. \nReconnecting to the AP again...");
        esp_wifi_connect();
    }
    else if ((WIFI_EVENT == event_base) && (WIFI_EVENT_STA_CONNECTED == event_id))
    {
        ESP_LOGI(WIFI_TAG, "CONNECTED.\n");
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;

    switch (event->event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(MQTT_TAG, "Connected to MQTT broker");
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(MQTT_TAG, "Disconnected from MQTT broker");
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
        ESP_LOGI(MQTT_TAG, "sent publish successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        break;
    case MQTT_EVENT_ANY:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_ANY");
        break;
    case MQTT_EVENT_BEFORE_CONNECT:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_BEFORE_CONNECT");
        break;
    case MQTT_EVENT_DELETED:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_DELETED");
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
        {
            ESP_LOGI(MQTT_TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
        }
        break;
    default:
        ESP_LOGI(MQTT_TAG, "Other event id:%d", event->event_id);
        break;
    }
}

static void IRAM_ATTR _btn_1_isr(void *p_arg)
{
    (void)p_arg;
    xTaskCreate(&_buzzer_task, "buzzer_task", 2 * 1024, NULL, 5, &xHandle);
}

static void IRAM_ATTR _btn_2_isr(void *p_arg)
{
    (void)p_arg;
    vTaskDelete(xHandle);
    turn_off_buzzer();
    _led_off(GPIO_LED_BLUE);
    _led_off(GPIO_LED_RED);
    _led_off(GPIO_LED_GREEN);
}

static esp_err_t _button_init(uint8_t pin, button_pressed_isr_t p_isr)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << pin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = (GPIO_INTR_POSEDGE),
    };

    esp_err_t esp_err = gpio_config(&io_conf);

    if (ESP_OK == esp_err)
    {
        esp_err = gpio_set_intr_type(io_conf.pin_bit_mask, io_conf.intr_type);
    }

    if (ESP_OK == esp_err)
    {
        gpio_install_isr_service(ESP_INTR_FLAG_LEVEL3);
    }

    if (ESP_OK == esp_err)
    {
        esp_err = gpio_isr_handler_add(pin, p_isr, (void *)&pin);
    }

    return esp_err;
}

//------------------------------ BUZZER FUNCTIONS --------------------------------

void turn_on_buzzer()
{
    ledc_set_duty(LEDC_HIGH_SPEED_MODE, BUZZER_CHANNEL, 128);
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, BUZZER_CHANNEL);
}

void turn_off_buzzer()
{
    ledc_set_duty(LEDC_HIGH_SPEED_MODE, BUZZER_CHANNEL, 0);
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, BUZZER_CHANNEL);
}

static void _sos_sequence()
{
    turn_on_buzzer();
    _led_on(GPIO_LED_BLUE);
    _led_on(GPIO_LED_RED);
    _led_on(GPIO_LED_GREEN);
    vTaskDelay(BUZZ_SHORT / portTICK_PERIOD_MS);
    turn_off_buzzer();
    _led_off(GPIO_LED_BLUE);
    _led_off(GPIO_LED_RED);
    _led_off(GPIO_LED_GREEN);
    vTaskDelay(BUZZ_SHORT / portTICK_PERIOD_MS);
    turn_on_buzzer();
    _led_on(GPIO_LED_BLUE);
    _led_on(GPIO_LED_RED);
    _led_on(GPIO_LED_GREEN);
    vTaskDelay(BUZZ_SHORT / portTICK_PERIOD_MS);
    turn_off_buzzer();
    _led_off(GPIO_LED_BLUE);
    _led_off(GPIO_LED_RED);
    _led_off(GPIO_LED_GREEN);
    vTaskDelay(BUZZ_SHORT / portTICK_PERIOD_MS);
    turn_on_buzzer();
    _led_on(GPIO_LED_BLUE);
    _led_on(GPIO_LED_RED);
    _led_on(GPIO_LED_GREEN);
    vTaskDelay(BUZZ_SHORT / portTICK_PERIOD_MS);
    turn_off_buzzer();
    _led_off(GPIO_LED_BLUE);
    _led_off(GPIO_LED_RED);
    _led_off(GPIO_LED_GREEN);
    vTaskDelay(BUZZ_SHORT / portTICK_PERIOD_MS);

    turn_on_buzzer();
    _led_on(GPIO_LED_BLUE);
    _led_on(GPIO_LED_RED);
    _led_on(GPIO_LED_GREEN);
    vTaskDelay(BUZZ_LONG / portTICK_PERIOD_MS);
    turn_off_buzzer();
    _led_off(GPIO_LED_BLUE);
    _led_off(GPIO_LED_RED);
    _led_off(GPIO_LED_GREEN);
    vTaskDelay(BUZZ_LONG / portTICK_PERIOD_MS);
    turn_on_buzzer();
    _led_on(GPIO_LED_BLUE);
    _led_on(GPIO_LED_RED);
    _led_on(GPIO_LED_GREEN);
    vTaskDelay(BUZZ_LONG / portTICK_PERIOD_MS);
    turn_off_buzzer();
    _led_off(GPIO_LED_BLUE);
    _led_off(GPIO_LED_RED);
    _led_off(GPIO_LED_GREEN);
    vTaskDelay(BUZZ_LONG / portTICK_PERIOD_MS);
    turn_on_buzzer();
    _led_on(GPIO_LED_BLUE);
    _led_on(GPIO_LED_RED);
    _led_on(GPIO_LED_GREEN);
    vTaskDelay(BUZZ_LONG / portTICK_PERIOD_MS);
    turn_off_buzzer();
    _led_off(GPIO_LED_BLUE);
    _led_off(GPIO_LED_RED);
    _led_off(GPIO_LED_GREEN);
    vTaskDelay(BUZZ_LONG / portTICK_PERIOD_MS);

    turn_on_buzzer();
    _led_on(GPIO_LED_BLUE);
    _led_on(GPIO_LED_RED);
    _led_on(GPIO_LED_GREEN);
    vTaskDelay(BUZZ_SHORT / portTICK_PERIOD_MS);
    turn_off_buzzer();
    _led_off(GPIO_LED_BLUE);
    _led_off(GPIO_LED_RED);
    _led_off(GPIO_LED_GREEN);
    vTaskDelay(BUZZ_SHORT / portTICK_PERIOD_MS);
    turn_on_buzzer();
    _led_on(GPIO_LED_BLUE);
    _led_on(GPIO_LED_RED);
    _led_on(GPIO_LED_GREEN);
    vTaskDelay(BUZZ_SHORT / portTICK_PERIOD_MS);
    turn_off_buzzer();
    _led_off(GPIO_LED_BLUE);
    _led_off(GPIO_LED_RED);
    _led_off(GPIO_LED_GREEN);
    vTaskDelay(BUZZ_SHORT / portTICK_PERIOD_MS);
    turn_on_buzzer();
    _led_on(GPIO_LED_BLUE);
    _led_on(GPIO_LED_RED);
    _led_on(GPIO_LED_GREEN);
    vTaskDelay(BUZZ_SHORT / portTICK_PERIOD_MS);
    turn_off_buzzer();
    _led_off(GPIO_LED_BLUE);
    _led_off(GPIO_LED_RED);
    _led_off(GPIO_LED_GREEN);
    vTaskDelay(BUZZ_SHORT / portTICK_PERIOD_MS);
}

//------------------------------ LED FUNCIONS --------------------------------

static void _led_init(uint8_t pin)
{
    // zero-initialize the config structure.
    gpio_config_t io_conf = {};
    // disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    // set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    // bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = GPIO_BIT_MASK(pin);
    // disable pull-down mode
    io_conf.pull_down_en = 0;
    // disable pull-up mode
    io_conf.pull_up_en = 0;
    // configure GPIO with the given settings
    gpio_config(&io_conf);
}

static void _led_on(uint8_t pin)
{
    gpio_set_level(pin, 1U);
}

static void _led_off(uint8_t pin)
{
    gpio_set_level(pin, 0U);
}