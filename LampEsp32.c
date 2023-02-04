#include stdio.h
#include string.h
#include freertosFreeRTOS.h
#include freertostask.h
#include drivergpio.h
#include driveradc.h
#include esp_log.h
#include led_strip.h
#include sdkconfig.h
#include esp_adc_cal.h
#include freertostimers.h
#include esp_event_loop.h
#include esp_wifi.h
#include nvs_flash.h
#include esp_http_server.h
#include sysparam.h

 Configuração WIFI
#define WIFI_SSID GUNA_SMART_LAMP
#define WIFI_PASSWORD gunasmartlamp123
#define WIFI_CHANNEL 1
#define WIFI_MAX_STA 4
 Flag de interrupção
#define ESP_INTR_FLAG_DEFAULT 0

 Leitura analógica
static esp_adc_cal_characteristics_t adc1_chars;

TaskHandle_t ISR = NULL;

TaskHandle_t xHandleSensors = NULL;

 Var de controle
int lampValue = 0;
int enablePirSensor = 1;
int enableLdrSensor = 1;
int enableSoundSensor = 1;

int manualSet = 0;

long long int lastTime = 0;
long long int actualTime = 0;

void IRAM_ATTR sensor_isr_handler(void arg){
    xTaskResumeFromISR(ISR);
}

void sensorTask(void arg){
    while(1){
        vTaskSuspend(NULL);
        actualTime = esp_timer_get_time()1000;

        if(enableSoundSensor){
            if(lastTime == 0  actualTime - lastTime  800 ){
                ESP_LOGI(Teste, Turn on (Sound detected) %lld, actualTime);
                lastTime = actualTime;
                lampValue = 1;
                manualSet = 1;
            } else {
                ESP_LOGI(Teste, Turn off (Sound detected) %lld, actualTime);
                lampValue = 0;
                manualSet = 0;
                lastTime = 0;
            }
        }
        gpio_set_level(23, lampValue);
        vTaskDelay(300portTICK_PERIOD_MS);
    }
}

 Define entradas de sensores nas portas digitais 
void configureGpio() {
    gpio_pad_select_gpio(15);  Sensor PIR
    gpio_pad_select_gpio(19);  Sensor Som
    gpio_pad_select_gpio(23);  Saída LED

     Define direções das portas digitais
    gpio_set_direction(15 ,GPIO_MODE_INPUT);
    gpio_set_direction(19 ,GPIO_MODE_INPUT);
    gpio_set_direction(23 ,GPIO_MODE_OUTPUT);
    gpio_set_level(23, 0);  Inicia com LED desligado

     Define interrupção para o sensor de som (falling)
    int taskCore = 0;
    gpio_set_intr_type(19, GPIO_INTR_POSEDGE);
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    gpio_isr_handler_add(19, sensor_isr_handler, NULL);
    xTaskCreatePinnedToCore(sensorTask, sensorTask, 4096, NULL, 10, &ISR);
    xTaskCreatePinnedToCore(sensorTask, sensorTask, 4096, NULL, 5, &ISR, taskCore);

     Define porta analógica para o sensor LDR
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_DEFAULT, 0, &adc1_chars);
    ESP_ERROR_CHECK(adc1_config_width(ADC_WIDTH_BIT_DEFAULT));
    ESP_ERROR_CHECK(adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11));
}

esp_err_t get_handler(httpd_req_t req){
    
    char s1[20];
    char s2[20];
    char s3[20];
    char s4[20];

    sprintf(s1, %d-, lampValue);
    sprintf(s2, %d-, enablePirSensor);
    sprintf(s3, %d-, enableLdrSensor);
    sprintf(s4, %d, enableSoundSensor);
 
     Concatenate both strings
    strcat(s1, s2);
    strcat(s1, s3);
    strcat(s1, s4);
    httpd_resp_set_hdr(req, Access-Control-Allow-Origin, );
	httpd_resp_send(req, s1, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t on_handler(httpd_req_t req){
    ESP_LOGI(Teste, On Handler);
    gpio_set_level(23, 1);
	lampValue = 1;
    manualSet = 1;
    const char resp[] = Turned On;
    httpd_resp_set_hdr(req, Access-Control-Allow-Origin, );
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t off_handler(httpd_req_t req){
    ESP_LOGI(Teste, Off Handler);
    gpio_set_level(23, 0);
	lampValue = 0;
    manualSet = 0;
    const char resp[] = Turned Off;
    httpd_resp_set_hdr(req, Access-Control-Allow-Origin, );
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t setSensor_handler(httpd_req_t req){

     Pega os dados obtidos do método post e salva na variável content
    char content[100];
    memset (content, 0, 100);
    size_t recv_size = MIN(req-content_len, sizeof(content));
    int ret = httpd_req_recv(req, content, recv_size);

    enablePirSensor = content[0] - 48;
    enableLdrSensor = content[2] - 48;
    enableSoundSensor = content[4] - 48;

    ESP_LOGI(Teste, Post received %s, content);

    char s1[20];
    char s2[20]; 
    char s3[20];

    sprintf(s1, %d-, enablePirSensor);
    sprintf(s2, %d-, enableLdrSensor);
    sprintf(s3, %d, enableSoundSensor);
 
     Concatenate both strings
    strcat(s1, s2);
    strcat(s1, s3);

    httpd_resp_set_hdr(req, Access-Control-Allow-Origin, );
    httpd_resp_send(req, s1, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

httpd_uri_t uri_get = {
    .uri      = ,
    .method   = HTTP_GET,
    .handler  = get_handler,
    .user_ctx = NULL
};

httpd_uri_t uri_on = {
    .uri      = on,
    .method   = HTTP_GET,
    .handler  = on_handler,
    .user_ctx = NULL
};

httpd_uri_t uri_off = {
    .uri      = off,
    .method   = HTTP_GET,
    .handler  = off_handler,
    .user_ctx = NULL
};

httpd_uri_t uri_setSensor = {
    .uri      = setSensor,
    .method   = HTTP_POST,
    .handler  = setSensor_handler,
    .user_ctx = NULL
};

httpd_handle_t setup_server(void){
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &uri_get);
		httpd_register_uri_handler(server, &uri_on);
		httpd_register_uri_handler(server, &uri_off);
        httpd_register_uri_handler(server, &uri_setSensor);
    }
    return server;
}

static void wifi_event_handler(void arg, esp_event_base_t event_base, int32_t event_id, void event_data){
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        ESP_LOGI(Teste, New station joined);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        ESP_LOGI(Teste', A station left);
    }
}

void setup_wifi(){
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES  ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
   
	ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t  p_netif = esp_netif_create_default_wifi_ap();

	esp_netif_ip_info_t ipInfo;
	IP4_ADDR(&ipInfo.ip, 192,168,1,1);
	IP4_ADDR(&ipInfo.gw, 192,168,1,1);
	IP4_ADDR(&ipInfo.netmask, 255,255,255,0);
	esp_netif_dhcps_stop(p_netif);
	esp_netif_set_ip_info(p_netif, &ipInfo);
	esp_netif_dhcps_start(p_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = WIFI_SSID,
            .ssid_len = strlen(WIFI_SSID),
            .channel = WIFI_CHANNEL,
            .password = WIFI_PASSWORD,
            .max_connection = WIFI_MAX_STA,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(Teste, WiFi init done.);

	esp_netif_ip_info_t if_info;
	ESP_ERROR_CHECK(esp_netif_get_ip_info(p_netif, &if_info));
	ESP_LOGI(Teste, ESP32 IP IPSTR, IP2STR(&if_info.ip));
}

 Retorna a leitura da porta digital 
bool verifySensor(int port){
    return gpio_get_level(port); 
}

 Verifica nível de tensão da porta analógica
bool verifyAnalogicValue(int minValue){
    int ldrValue = esp_adc_cal_raw_to_voltage(adc1_get_raw(ADC1_CHANNEL_6), &adc1_chars);  Verifica nivel de tensão obtido no LDR
    ESP_LOGI(Teste, Turn on (Analogic value) %d, ldrValue);
    return ldrValue  minValue;  Verifica se o valor é menor do que o valor informado na call (Return true caso esteja escuro)
}

void handleSensors(void  pvParameters){
    while (1) {
        if(verifyAnalogicValue(450) && enableLdrSensor){
            ESP_LOGI(Teste, Turn on (Analogic value));
            lampValue = 1;
        } else if(verifySensor(15) && enablePirSensor){
            ESP_LOGI(Teste, Turn on (Presence detected));
            lampValue = 1;
        } else if(!manualSet) {
            lampValue = 0;
        }
        gpio_set_level(23, lampValue);
        vTaskDelay(200portTICK_PERIOD_MS);
    }
}

void app_main(void){
    configureGpio();
    setup_wifi();
	setup_server();
    lastTime = 0;
    int taskCore = 1;
    xTaskCreatePinnedToCore(handleSensors, handleSensors, 10000, NULL, 5, &xHandleSensors, taskCore);
}
