#include <zephyr/drivers/spi.h>
#include <zephyr/sys/printk.h>
#include "MFRC522.h"

/* Get the SPI device from the Device Tree Overlay */
#define SPI_OP  SPI_OP_MODE_MASTER | SPI_WORD_SET(8) | SPI_TRANSFER_MSB
static const struct spi_dt_spec dev_spi = SPI_DT_SPEC_GET(DT_NODELABEL(rfid_reader), SPI_OP, 0);


void Write_MFRC522(uint8_t addr, uint8_t val) {
    uint8_t tx_data[2] = { (addr << 1) & 0x7E, val };
    struct spi_buf tx_buf = {.buf = tx_data, .len = 2};
    struct spi_buf_set tx = {.buffers = &tx_buf, .count = 1};
    spi_write_dt(&dev_spi, &tx);
}

uint8_t Read_MFRC522(uint8_t addr) {
    uint8_t tx_data[2] = { ((addr << 1) & 0x7E) | 0x80, 0x00 };
    uint8_t rx_data[2];
    struct spi_buf tx_buf = {.buf = tx_data, .len = 2};
    struct spi_buf_set tx = {.buffers = &tx_buf, .count = 1};
    struct spi_buf rx_buf = {.buf = rx_data, .len = 2};
    struct spi_buf_set rx = {.buffers = &rx_buf, .count = 1};
    spi_transceive_dt(&dev_spi, &tx, &rx);
    return rx_data[1];
}

void MFRC522_SetBitMask(uint8_t reg, uint8_t mask) {
    Write_MFRC522(reg, Read_MFRC522(reg) | mask);
}

void MFRC522_ClearBitMask(uint8_t reg, uint8_t mask) {
    Write_MFRC522(reg, Read_MFRC522(reg) & (~mask));
}

/* --- Core Functions --- */

int MFRC522_Init(void) {
    if (!spi_is_ready_dt(&dev_spi)) {
        printk("SPI Device not ready\n");
        return -1;
    }

    /* Soft Reset */
    Write_MFRC522(MFRC522_REG_COMMAND, PCD_RESETPHASE);
    k_msleep(50);

    /* Timer Init */
    Write_MFRC522(MFRC522_REG_T_MODE, 0x8D);
    Write_MFRC522(MFRC522_REG_T_PRESCALER, 0x3E);
    Write_MFRC522(MFRC522_REG_T_RELOAD_L, 30);
    Write_MFRC522(MFRC522_REG_T_RELOAD_H, 0);
    Write_MFRC522(MFRC522_REG_TX_AUTO, 0x40);
    Write_MFRC522(MFRC522_REG_MODE, 0x3D);

    /* Antenna On */
    uint8_t temp = Read_MFRC522(MFRC522_REG_TX_CONTROL);
    if (!(temp & 0x03)) {
        MFRC522_SetBitMask(MFRC522_REG_TX_CONTROL, 0x03);
    }
    
    printk("MFRC522 Init Done\n");
    return 0;
}

TM_MFRC522_STS_T MFRC522_ToCard(uint8_t command, uint8_t *sendData, uint8_t sendLen, uint8_t *backData, uint16_t *backLen) {
    TM_MFRC522_STS_T status = MI_ERR;
    uint8_t irqEn = 0x00;
    uint8_t waitIRq = 0x00;
    uint8_t n;
    uint16_t i;

    if (command == PCD_AUTHENT) { irqEn = 0x12; waitIRq = 0x10; }
    if (command == PCD_TRANSCEIVE) { irqEn = 0x77; waitIRq = 0x30; }

    Write_MFRC522(MFRC522_REG_COMM_IE_N, irqEn | 0x80);
    MFRC522_ClearBitMask(MFRC522_REG_COMM_IRQ, 0x80);
    MFRC522_SetBitMask(MFRC522_REG_FIFO_LEVEL, 0x80);
    Write_MFRC522(MFRC522_REG_COMMAND, PCD_IDLE);

    for (i = 0; i < sendLen; i++) Write_MFRC522(MFRC522_REG_FIFO_DATA, sendData[i]);

    Write_MFRC522(MFRC522_REG_COMMAND, command);
    if (command == PCD_TRANSCEIVE) MFRC522_SetBitMask(MFRC522_REG_BIT_FRAMING, 0x80);

    i = 2000;
    do {
        n = Read_MFRC522(MFRC522_REG_COMM_IRQ);
        i--;
        k_busy_wait(10);
    } while ((i != 0) && !(n & 0x01) && !(n & waitIRq));

    MFRC522_ClearBitMask(MFRC522_REG_BIT_FRAMING, 0x80);

    if (i != 0) {
        if (!(Read_MFRC522(MFRC522_REG_ERROR) & 0x1B)) {
            status = MI_OK;
            if (n & irqEn & 0x01) status = MI_NOTAGERR;
            if (command == PCD_TRANSCEIVE) {
                n = Read_MFRC522(MFRC522_REG_FIFO_LEVEL);
                uint8_t lastBits = Read_MFRC522(MFRC522_REG_CONTROL) & 0x07;
                if (lastBits) *backLen = (n - 1) * 8 + lastBits;
                else *backLen = n * 8;
                if (n == 0) n = 1;
                if (n > 16) n = 16;
                for (i = 0; i < n; i++) backData[i] = Read_MFRC522(MFRC522_REG_FIFO_DATA);
            }
        } else { status = MI_ERR; }
    }
    return status;
}

TM_MFRC522_STS_T MFRC522_Check(uint8_t *id) {
    TM_MFRC522_STS_T status;
    uint8_t buffer[2];
    uint16_t len;
    
    // 1. Request
    Write_MFRC522(MFRC522_REG_BIT_FRAMING, 0x07);
    buffer[0] = PICC_REQA;
    status = MFRC522_ToCard(PCD_TRANSCEIVE, buffer, 1, buffer, &len);
    if (status != MI_OK || len != 0x10) return MI_ERR;

    // 2. Anticollision
    Write_MFRC522(MFRC522_REG_BIT_FRAMING, 0x00);
    id[0] = PICC_ANTICOLL;
    id[1] = 0x20;
    status = MFRC522_ToCard(PCD_TRANSCEIVE, id, 2, id, &len);
    
    if (status == MI_OK) {
        uint8_t serNumCheck = 0;
        for (int i = 0; i < 4; i++) serNumCheck ^= id[i];
        if (serNumCheck != id[4]) status = MI_ERR;
    }
    return status;
}

void MFRC522_Halt(void) {
    uint16_t unLen;
    uint8_t buff[4] = {PICC_HALT, 0};
    // Simple Halt without CRC calculation for brevity
    MFRC522_ToCard(PCD_TRANSCEIVE, buff, 2, buff, &unLen);
}