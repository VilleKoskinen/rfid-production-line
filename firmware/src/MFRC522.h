#ifndef MFRC522_H
#define MFRC522_H

#include <zephyr/kernel.h>

/* Status Enumerations */
typedef enum {
    MI_OK = 0,
    MI_NOTAGERR,
    MI_ERR
} TM_MFRC522_STS_T;

/* MFRC522 Registers */
#define PCD_IDLE              0x00
#define PCD_AUTHENT           0x0E
#define PCD_RECEIVE           0x08
#define PCD_TRANSMIT          0x04
#define PCD_TRANSCEIVE        0x0C
#define PCD_RESETPHASE        0x0F
#define PCD_CALCCRC           0x03

#define PICC_REQA             0x26
#define PICC_ANTICOLL         0x93
#define PICC_HALT             0x50

#define MFRC522_REG_COMMAND       0x01
#define MFRC522_REG_COMM_IE_N     0x02
#define MFRC522_REG_COMM_IRQ      0x04
#define MFRC522_REG_DIV_IRQ       0x05
#define MFRC522_REG_ERROR         0x06
#define MFRC522_REG_FIFO_DATA     0x09
#define MFRC522_REG_FIFO_LEVEL    0x0A
#define MFRC522_REG_CONTROL       0x0C
#define MFRC522_REG_BIT_FRAMING   0x0D

#define MFRC522_REG_MODE          0x11
#define MFRC522_REG_TX_MODE       0x12
#define MFRC522_REG_RX_MODE       0x13
#define MFRC522_REG_TX_CONTROL    0x14
#define MFRC522_REG_TX_AUTO       0x15

#define MFRC522_REG_CRC_RESULT_M  0x21
#define MFRC522_REG_CRC_RESULT_L  0x22
#define MFRC522_REG_T_MODE        0x2A
#define MFRC522_REG_T_PRESCALER   0x2B
#define MFRC522_REG_T_RELOAD_H    0x2C
#define MFRC522_REG_T_RELOAD_L    0x2D

/* Public Functions */
int MFRC522_Init(void);
TM_MFRC522_STS_T MFRC522_Check(uint8_t *id);
void MFRC522_Halt(void);

#endif