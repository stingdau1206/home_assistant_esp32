/* MQTT (over TCP) Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"
#include "esp_mac.h"
#include "cJSON.h"

static const char *TAG = "mqtt_example";

uint8_t mac_num[6] = {0};
char mac_str[18] = {0};

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0)
    {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        char topic[64] = {0};
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        cJSON *switchConfig = cJSON_CreateObject();
        cJSON_AddStringToObject(switchConfig, "unique_id", mac_str);
        cJSON_AddStringToObject(switchConfig, "name", "Khoatn9 MQTT Switch");
        cJSON_AddStringToObject(switchConfig, "state_topic", "khoatn9/switch/state"); // topic to sub
        cJSON_AddStringToObject(switchConfig, "command_topic", "khoatn9/switch/command");
        cJSON_AddStringToObject(switchConfig, "payload_on", "ON");
        cJSON_AddStringToObject(switchConfig, "payload_off", "OFF");
        cJSON_AddStringToObject(switchConfig, "state_on", "ON");
        cJSON_AddStringToObject(switchConfig, "state_off", "OFF");
        cJSON_AddNumberToObject(switchConfig, "qos", 1);
        cJSON_AddBoolToObject(switchConfig, "optimistic", false);
        cJSON_AddBoolToObject(switchConfig, "retain", true);

        cJSON *switchDevice = cJSON_AddObjectToObject(switchConfig, "device");
        cJSON_AddStringToObject(switchDevice, "identifiers", mac_str);
        cJSON_AddStringToObject(switchDevice, "name", "Khoatn9 MQTT Device");
        cJSON_AddStringToObject(switchDevice, "manufacturer", "Khoatn9");
        cJSON_AddStringToObject(switchDevice, "model", "ESP32");
        // print_user_property(event->property->user_property);
        // esp_mqtt5_client_set_user_property(&publish_property.user_property, user_property_arr, USE_PROPERTY_ARR_SIZE);
        // esp_mqtt5_client_set_publish_property(client, &publish_property);
        char *jsonPayLoad = cJSON_Print(switchConfig);
        ESP_LOGI(TAG, "jsonPayLoad: %s", jsonPayLoad);
        sprintf(topic, "homeassistant/switch/%s/config", mac_str);
        msg_id = esp_mqtt_client_publish(client, topic, jsonPayLoad, 0, 1, 1);
        ESP_LOGI(TAG, "sent publish to %s successful, msg_id=%d", topic, msg_id);

        msg_id = esp_mqtt_client_subscribe(client, "khoatn9/switch/command", 1);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        ESP_LOGI(TAG, "free json payload");
        free(jsonPayLoad);
        ESP_LOGI(TAG, "free json config");
        cJSON_Delete(switchConfig);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        msg_id = esp_mqtt_client_publish(client, "khoatn9/switch/state", "ON", 0, 1, 1);
        ESP_LOGI(TAG, "sent publish to %s successful, msg_id=%d", "khoatn9/switch/state", msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        msg_id = esp_mqtt_client_publish(client, "khoatn9/switch/state", event->data, 0, 1, 1);
        ESP_LOGI(TAG, "sent publish to %s successful, msg_id=%d", "khoatn9/switch/state", msg_id);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
        {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno", event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

static void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = CONFIG_BROKER_URL,
        .broker.address.port = 1883,
        .session.protocol_ver = MQTT_PROTOCOL_V_3_1_1,
        .credentials.username = "homeassistant",
        .credentials.authentication.password = "uhaet2loh2oothutheeghaet5He5Cai9bameiJeekah7eenaengo1igha4ooniag",
    };
#if CONFIG_BROKER_URL_FROM_STDIN
    char line[128];

    if (strcmp(mqtt_cfg.broker.address.uri, "FROM_STDIN") == 0)
    {
        int count = 0;
        printf("Please enter url of mqtt broker\n");
        while (count < 128)
        {
            int c = fgetc(stdin);
            if (c == '\n')
            {
                line[count] = '\0';
                break;
            }
            else if (c > 0 && c < 127)
            {
                line[count] = c;
                ++count;
            }
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
        mqtt_cfg.broker.address.uri = line;
        printf("Broker url: %s\n", line);
    }
    else
    {
        ESP_LOGE(TAG, "Configuration mismatch: wrong broker url");
        abort();
    }
#endif /* CONFIG_BROKER_URL_FROM_STDIN */

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}

void app_main(void)
{
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("mqtt_client", ESP_LOG_VERBOSE);
    esp_log_level_set("mqtt_example", ESP_LOG_VERBOSE);
    esp_log_level_set("transport_base", ESP_LOG_VERBOSE);
    esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
    esp_log_level_set("transport", ESP_LOG_VERBOSE);
    esp_log_level_set("outbox", ESP_LOG_VERBOSE);

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_efuse_mac_get_default(mac_num);
    sprintf(mac_str, "%02X%02X%02X%02X%02X%02X",
            mac_num[0], mac_num[1], mac_num[2], mac_num[3], mac_num[4], mac_num[5]);
    ESP_LOGI(TAG, "%s", mac_str);
    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());

    mqtt_app_start();
}
