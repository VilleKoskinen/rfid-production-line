#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <string.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>

#include "MFRC522.h"

#define LED0_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

/* Bluetooth Data Structures */
#define COMPANY_ID_CODE 0xFFFF 
uint8_t ID[5];
uint8_t lastID[5];
bool card_present = false;

/* 
 * This buffer holds the data we send over the air.
 * Byte 0-1: Company ID
 * Byte 2:   Status (1 = Card Present, 0 = No Card)
 * Byte 3-6: Card UID
 */
static uint8_t mfg_data[7] = { 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00 };

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, 11), // Name "RFID_Reader"
    BT_DATA(BT_DATA_MANUFACTURER_DATA, mfg_data, 7)            // Our custom data
};

static const struct bt_le_adv_param *adv_param = BT_LE_ADV_PARAM(
    BT_LE_ADV_OPT_USE_IDENTITY | BT_LE_ADV_OPT_SCANNABLE, 
    BT_GAP_ADV_FAST_INT_MIN_2, 
    BT_GAP_ADV_FAST_INT_MAX_2, 
    NULL
);


/* Function to update advertising data dynamically */
void update_advertising(bool present, uint8_t *uid)
{
    int err;

    // Update Manufacturer Data
    // Byte 2 is status flag
    mfg_data[2] = present ? 0x01 : 0x00; 

    if (present && uid) {
        // Copy 4 bytes of UID into the packet
        memcpy(&mfg_data[3], uid, 4);
    } else {
        // Clear UID data if no card
        memset(&mfg_data[3], 0, 4);
    }

    // Update the advertisement
    err = bt_le_adv_update_data(ad, ARRAY_SIZE(ad), NULL, 0);
    if (err) {
        printk("Failed to update advertising data (err %d)\n", err);
    }
}

void bt_ready(int err)
{
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return;
    }

    printk("Bluetooth initialized\n");

    /* Start advertising */
    err = bt_le_adv_start(adv_param, ad, ARRAY_SIZE(ad), NULL, 0);

    if (err) {
        printk("Advertising failed to start (err %d)\n", err);
        return;
    }

    printk("Advertising started\n");
}

int main(void)
{
    int ret;

    printk("Starting RFID Reader on nRF52840 Dongle...\n");

    if (!gpio_is_ready_dt(&led)) return 0;
    gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);

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

    memset(lastID, 0, 5);

    while (1) {
        // 1. Check for Card
        if (MFRC522_Check(ID) == MI_OK) {
            
            // Only update Bluetooth if it's a new card or first read
            if (!card_present || memcmp(ID, lastID, 4) != 0) {
                
                gpio_pin_toggle_dt(&led);
                printk("Broadcasting UID: [%02X %02X %02X %02X]\n", 
                       ID[0], ID[1], ID[2], ID[3]);
                
                // UPDATE BLUETOOTH PACKET HERE
                update_advertising(true, ID);
                
                memcpy(lastID, ID, 5);
            }
            
            card_present = true;
            MFRC522_Halt();
        } 
        else {
            if (card_present) {
                printk("Card Removed. Clearing Broadcast.\n");
                
                // Clear the Bluetooth Packet (Optional: or keep broadcasting last card)
                update_advertising(false, NULL);
                
                memset(lastID, 0, 5);
                card_present = false;
            }
        }
        k_msleep(100);
    }
    return 0;
}