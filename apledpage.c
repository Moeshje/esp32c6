#include <string.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

wifi_config_t wifi_config = {
    .ap = {
        .ssid = "------",
        .ssid_len = strlen("------"),
        .password = "randomnetwork", //  PASSWORD MUST BE 8 CHARACTERS TO WORK!!!
        .max_connection = 1,
        .authmode = WIFI_AUTH_WPA2_PSK, // password protocol
    },
};

TaskHandle_t led_task_handle = NULL;

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        printf("Connection established\n");
        vTaskSuspend(led_task_handle);
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 8191);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        printf("Connection disbanded\n");
        vTaskResume(led_task_handle);
    }
}

void ledtask(void *arg)  {
    while(1) {
        for(int i = 0; i <= 8191; i++) {
            ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, i);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
            vTaskDelay(pdMS_TO_TICKS(9));
        }
        for(int i = 8191; i >= 0; i--) {
            ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, i);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
            vTaskDelay(pdMS_TO_TICKS(9));
        }
    }
}

esp_err_t root_get_handler(httpd_req_t *req)
{
    const char* resp =
    "<!DOCTYPE html>"
    "<html>"
    "<body>"
    "<form action=\"/set_ssid\" method=\"POST\">"
    "<label for=\"ssid\">Enter preffered SSID</label><br>"
    "<input type=\"text\" id=\"ssid\" name=\"ssid\" maxlength=\"23\">"
    "<br>"
    "<input type=\"submit\" value=\"Enter\">"
    "</form>"
    "</body></html>";
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t ssid_handler(httpd_req_t *req) {
    char buf[100];
    int ret = httpd_req_recv(req, buf, sizeof(buf));
    buf[ret] = '\0';
    char *new_ssid = strstr(buf, "ssid=");
    new_ssid += 5;
    for (int i = 0; new_ssid[i]; i++) {
        if (new_ssid[i] == '+') new_ssid[i] = ' ';
    }
    esp_wifi_stop();
    strcpy((char *)wifi_config.ap.ssid, new_ssid);
    wifi_config.ap.ssid_len = strlen(new_ssid);
    esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    esp_wifi_start();
    httpd_resp_send(req, "SSID updated succesfully.", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

httpd_handle_t start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK) {

        httpd_uri_t uri_get = {
            .uri      = "/",
            .method   = HTTP_GET,
            .handler  = root_get_handler,
            .user_ctx = NULL
        };

        httpd_uri_t uri_post = {
            .uri      = "/set_ssid",
            .method   = HTTP_POST,
            .handler  = ssid_handler,
            .user_ctx = NULL
        };

        httpd_register_uri_handler(server, &uri_get);
        httpd_register_uri_handler(server, &uri_post);
    }

    return server;
}

void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    esp_netif_init();
    esp_event_loop_create_default();
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL, NULL);
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    esp_wifi_start();

    ledc_timer_config_t time = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .timer_num       = LEDC_TIMER_0,
        .freq_hz         = 5000,
        .duty_resolution = LEDC_TIMER_13_BIT,
        .clk_cfg         = LEDC_AUTO_CLK,
        .deconfigure     = false,
    };
    ledc_timer_config(&time);

    ledc_channel_config_t easy = {
        .gpio_num   = 1,
        .timer_sel  = LEDC_TIMER_0,
        .channel    = LEDC_CHANNEL_0,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty       = 0,
        .hpoint     = 0,
    };
    ledc_channel_config(&easy);
    xTaskCreate(ledtask, "ledtask", 8192, NULL, 1, &led_task_handle);
    start_webserver();
}
