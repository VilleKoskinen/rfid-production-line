#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <string.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/drivers/watchdog.h>

#include "MFRC522.h"

#define LED0_NODE DT_ALIAS(led0)
#define MIN_JOB_TIME_MS 5000

/* --- Recovery Settings --- */
#define MAX_READ_FAILURES 300  /

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

/* --- Watchdog Variables --- */
const struct device *wdt = DEVICE_DT_GET(DT_NODELABEL(wdt0));
int wdt_channel_id;

/* Bluetooth Data Structures */
#define COMPANY_ID_CODE 0xFFFF 
uint8_t ID[5];
uint8_t lastID[5];
bool card_present = false;

static uint8_t mfg_data[7] = { 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00 };

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, 11),
    BT_DATA(BT_DATA_MANUFACTURER_DATA, mfg_data, 7)
};

static const struct bt_le_adv_param *adv_param = BT_LE_ADV_PARAM(
    BT_LE_ADV_OPT_USE_IDENTITY | BT_LE_ADV_OPT_SCANNABLE, 
    BT_GAP_ADV_FAST_INT_MIN_2, 
    BT_GAP_ADV_FAST_INT_MAX_2, 
    NULL
);

void update_advertising(bool present, uint8_t *uid)
{
    int err;
    mfg_data[2] = present ? 0x01 : 0x00; 

    if (present && uid) {
        memcpy(&mfg_data[3], uid, 4);
    } else {
        memset(&mfg_data[3], 0, 4);
    }

    err = bt_le_adv_update_data(ad, ARRAY_SIZE(ad), NULL, 0);
}

void bt_ready(int err)
{
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return;
    }
    bt_le_adv_start(adv_param, ad, ARRAY_SIZE(ad), NULL, 0);
    printk("Advertising started\n");
}

/* ---  Watchdog Setup Function --- */
void watchdog_init(void) {
    if (!device_is_ready(wdt)) {
        printk("Watchdog not ready!\n");
        return;
    }

    struct wdt_timeout_cfg wdt_config = {
        /* Reboot if stuck for 5 seconds */
        .window.max = 5000,
        .callback = NULL, 
        .flags = WDT_FLAG_RESET_SOC
    };

    wdt_channel_id = wdt_install_timeout(wdt, &wdt_config);
    wdt_setup(wdt, WDT_OPT_PAUSE_HALTED_BY_DBG);
}

int main(void)
{
    int ret;

    printk("Starting RFID Reader on nRF52840 Dongle...\n");

    if (!gpio_is_ready_dt(&led)) return 0;
    gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);

    /* Init Hardware */
    if (MFRC522_Init() != 0) {
        printk("MFRC522 Init Failed!\n");
        return 0;
    }

    /* Init Bluetooth */
    ret = bt_enable(bt_ready);
    if (ret) {
        printk("Bluetooth init failed\n");
    }
    
    /* --- Init Watchdog --- */
    watchdog_init();

    memset(lastID, 0, 5);

    bool led_state = false;       
    bool tag_is_present = false;  
    int missing_tag_count = 0;
    int64_t last_toggle_time = 0; 
    
   
    int failure_count = 0;

    while (1) {
        
        wdt_feed(wdt, wdt_channel_id);

        if (MFRC522_Check(ID) == MI_OK) {
            
            missing_tag_count = 0;
            failure_count = 0; 

            if (!tag_is_present) {
                
                int64_t now = k_uptime_get();
                bool allow_toggle = true;

                if (led_state == true) {
                    if ((now - last_toggle_time) < MIN_JOB_TIME_MS) {
                        printk("Ignored: Job too short!\n");
                        allow_toggle = false; 
                        
                        
                        gpio_pin_set_dt(&led, 0); k_msleep(100);
                        gpio_pin_set_dt(&led, 1); k_msleep(100);
                        gpio_pin_set_dt(&led, 0); k_msleep(100);
                        gpio_pin_set_dt(&led, 1); 
                    }
                }

                if (allow_toggle) {
                    led_state = !led_state;
                    gpio_pin_set_dt(&led, led_state ? 1 : 0);
                    
                    last_toggle_time = now;

                    printk("Scan Accepted. Job: %s\n", led_state ? "STARTED" : "FINISHED");
                    update_advertising(true, ID);
                }
                
                tag_is_present = true;
            }
            MFRC522_Halt();
        } 
        else {
            
            failure_count++;
            if (failure_count > MAX_READ_FAILURES) {
                MFRC522_Init(); 
                failure_count = 0;
            }
           

            if (tag_is_present) {
                missing_tag_count++;
                if (missing_tag_count > 5) {
                    tag_is_present = false;
                    update_advertising(false, NULL);
                    printk("Tag removed.\n");
                }
            }
        }
        k_msleep(100);
    }
}