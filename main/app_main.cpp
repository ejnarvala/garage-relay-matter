#include <esp_err.h>
#include <esp_log.h>
#include <esp_matter.h>
#include <nvs_flash.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <iot_button.h>
#include <button_gpio.h>

static const char *TAG = "garage_relay";

#define RELAY_GPIO GPIO_NUM_4
#define BOOT_BUTTON_GPIO GPIO_NUM_9
#define RELAY_PULSE_MS 500

using namespace esp_matter;
using namespace esp_matter::attribute;
using namespace esp_matter::endpoint;
using namespace chip::app::Clusters;

static uint16_t garage_endpoint_id = 0;

static void pulse_relay(void *arg)
{
    ESP_LOGI(TAG, "Pulsing relay on GPIO%d for %dms", RELAY_GPIO, RELAY_PULSE_MS);
    gpio_set_level(RELAY_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(RELAY_PULSE_MS));
    gpio_set_level(RELAY_GPIO, 0);
    ESP_LOGI(TAG, "Relay pulse complete");

    esp_matter_attr_val_t val = esp_matter_bool(false);
    attribute::update(garage_endpoint_id, OnOff::Id, OnOff::Attributes::OnOff::Id, &val);

    vTaskDelete(NULL);
}

static void trigger_relay()
{
    xTaskCreate(pulse_relay, "pulse_relay", 2048, NULL, 5, NULL);
}

static esp_err_t app_attribute_update_cb(attribute::callback_type_t type, uint16_t endpoint_id,
                                         uint32_t cluster_id, uint32_t attribute_id,
                                         esp_matter_attr_val_t *val, void *priv_data)
{
    if (type == POST_UPDATE && endpoint_id == garage_endpoint_id &&
        cluster_id == OnOff::Id && attribute_id == OnOff::Attributes::OnOff::Id) {
        if (val->val.b) {
            trigger_relay();
        }
    }
    return ESP_OK;
}

static esp_err_t app_identification_cb(identification::callback_type_t type, uint16_t endpoint_id,
                                       uint8_t effect_id, uint8_t effect_variant, void *priv_data)
{
    ESP_LOGI(TAG, "Identify callback: endpoint=%d, effect=%d", endpoint_id, effect_id);
    return ESP_OK;
}

static void boot_button_cb(void *arg, void *data)
{
    ESP_LOGI(TAG, "Boot button pressed - triggering relay");
    trigger_relay();
}

static void boot_button_long_press_cb(void *arg, void *data)
{
    ESP_LOGI(TAG, "Boot button long press - factory reset");
    esp_matter::factory_reset();
}

static void app_event_cb(const ChipDeviceEvent *event, intptr_t arg)
{
    switch (event->Type) {
    case chip::DeviceLayer::DeviceEventType::kCommissioningComplete:
        ESP_LOGI(TAG, "Commissioning complete");
        break;
    case chip::DeviceLayer::DeviceEventType::kFabricRemoved:
        ESP_LOGI(TAG, "Fabric removed, opening commissioning window");
        break;
    case chip::DeviceLayer::DeviceEventType::kFabricCommitted:
        ESP_LOGI(TAG, "Fabric committed");
        break;
    default:
        break;
    }
}

static void init_relay_gpio()
{
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << RELAY_GPIO);
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io_conf);
    gpio_set_level(RELAY_GPIO, 0);
}

static void init_boot_button()
{
    button_config_t btn_cfg = {};
    button_gpio_config_t gpio_cfg = {};
    gpio_cfg.gpio_num = BOOT_BUTTON_GPIO;
    gpio_cfg.active_level = 0;

    button_handle_t btn = NULL;
    iot_button_new_gpio_device(&btn_cfg, &gpio_cfg, &btn);
    iot_button_register_cb(btn, BUTTON_PRESS_DOWN, NULL, boot_button_cb, NULL);
    iot_button_register_cb(btn, BUTTON_LONG_PRESS_START, NULL, boot_button_long_press_cb, NULL);
}

extern "C" void app_main()
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    init_relay_gpio();
    init_boot_button();

    node::config_t node_config;
    node_t *node = node::create(&node_config, app_attribute_update_cb, app_identification_cb);

    on_off_plug_in_unit::config_t plugin_config;
    plugin_config.on_off.on_off = false;
    endpoint_t *endpoint = on_off_plug_in_unit::create(node, &plugin_config, ENDPOINT_FLAG_NONE, NULL);
    garage_endpoint_id = endpoint::get_id(endpoint);

    ESP_LOGI(TAG, "Garage relay on GPIO%d, endpoint=%d", RELAY_GPIO, garage_endpoint_id);
    ESP_LOGI(TAG, "Boot button (GPIO%d) triggers relay, long press = factory reset", BOOT_BUTTON_GPIO);

    esp_matter::start(app_event_cb);

    ESP_LOGI(TAG, "Matter started successfully - pairing info printed above by CHIP stack");
}
