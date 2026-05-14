#include "ble_server.h"

#include <stdio.h>
#include <string.h>

#include "rgb_led.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"

#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

static const char *TAG = "BLE";

static int ble_gap_event(struct ble_gap_event *event, void *arg);

static uint16_t current_conn_handle = BLE_HS_CONN_HANDLE_NONE;
// UUIDs personalizados
static const ble_uuid128_t service_uuid =
    BLE_UUID128_INIT(
        0x9E,0xCA,0xDC,0x24,
        0x0E,0xE5,
        0xA9,0xE0,
        0x93,0xF3,
        0xA3,0xB5,0x01,0x00,0x40,0x6E
    );
static const ble_uuid128_t char_uuid =
    BLE_UUID128_INIT(
        0x9E,0xCA,0xDC,0x24,
        0x0E,0xE5,
        0xA9,0xE0,
        0x93,0xF3,
        0xA3,0xB5,0x03,0x00,0x40,0x6E
    );

// WRITE callback
static int device_write(uint16_t conn_handle,
                        uint16_t attr_handle,
                        struct ble_gatt_access_ctxt *ctxt,
                        void *arg)
{
    char data[20] = {0};
    uint16_t len = OS_MBUF_PKTLEN(ctxt->om);

    if (len >= sizeof(data)) {
        len = sizeof(data) - 1;
    }

    ble_hs_mbuf_to_flat(ctxt->om, data, len, NULL);

    ESP_LOGI(TAG, "Received: %s", data);

    if (strcmp(data, "1") == 0)
    {
        rgb_led_set(0, 0, 250);
    }
    else if (strcmp(data, "0") == 0)
    {
        rgb_led_off();
    }

    return 0;
}


// GATT definition
static const struct ble_gatt_svc_def gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &service_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[])
        {
            {
                .uuid = &char_uuid.u,
                .access_cb = device_write,
                .flags =
                    BLE_GATT_CHR_F_READ |
                    BLE_GATT_CHR_F_WRITE,
            },

            {0}
        },
    },

    {0}
};


static void ble_app_advertise(void)
{
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;

    memset(&fields, 0, sizeof(fields));

    fields.flags = BLE_HS_ADV_F_DISC_GEN |
                   BLE_HS_ADV_F_BREDR_UNSUP;

    fields.name = (uint8_t *)"ESP32C6_RGB";
    fields.name_len = strlen("ESP32C6_RGB");
    fields.name_is_complete = 1;

    ble_gap_adv_set_fields(&fields);

    memset(&adv_params, 0, sizeof(adv_params));

    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    ble_gap_adv_start(
        BLE_OWN_ADDR_PUBLIC,
        NULL,
        BLE_HS_FOREVER,
        &adv_params,
        ble_gap_event,
        NULL
    );
}


static int ble_gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type)
    {
        case BLE_GAP_EVENT_CONNECT:

        if (event->connect.status == 0)
        {
            current_conn_handle = event->connect.conn_handle;

            ESP_LOGI(TAG, "Client connected");
        }

        break;

        case BLE_GAP_EVENT_DISCONNECT:

        current_conn_handle = BLE_HS_CONN_HANDLE_NONE;

        ESP_LOGI(TAG, "Client disconnected");

        ble_app_advertise();

        break;
        default:
            break;
    }

    return 0;
}


static void ble_host_task(void *param)
{
    nimble_port_run();
}


void ble_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(nimble_port_init());

    ble_svc_gap_init();
    ble_svc_gatt_init();

    ble_svc_gap_device_name_set("ESP32C6_RGB");

    ble_gatts_count_cfg(gatt_svcs);
    ble_gatts_add_svcs(gatt_svcs);

    ble_hs_cfg.sync_cb = ble_app_advertise;
    nimble_port_freertos_init(ble_host_task);
}
