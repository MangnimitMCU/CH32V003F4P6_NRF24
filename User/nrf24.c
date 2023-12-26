
#include "nrf24.h"

#define NRF24_IRQ_PORT      GPIOC
#define NRF24_IRQ_PIN       GPIO_Pin_2

#define NRF24_CE_PORT       GPIOC
#define NRF24_CE_PIN        GPIO_Pin_3

#define NRF24_CSN_PORT      GPIOC
#define NRF24_CSN_PIN       GPIO_Pin_4

u8 nrf24_buf[32];

const u8 TX_ADDRESS[TX_ADR_WIDTH] = {0xFF,0xFF,0xFF,0xFF,0xFF};
const u8 RX_ADDRESS[RX_ADR_WIDTH] = {0xFF,0xFF,0xFF,0xFF,0xFF};

/* MODE        CPOL             CPHA
 *  0      SPI_CPOL_Low     SPI_CPHA_1Edge
 *  1      SPI_CPOL_Low     SPI_CPHA_2Edge
 *  2      SPI_CPOL_High    SPI_CPHA_1Edge
 *  3      SPI_CPOL_High    SPI_CPHA_2Edge
 */
/////////////////////////////////////////////////////////////////////
void SPI_Config(void) {
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    SPI_InitTypeDef SPI_InitStructure = {0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC|RCC_APB2Periph_SPI1, ENABLE);

    GPIO_InitStructure.GPIO_Pin = NRF24_IRQ_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(NRF24_IRQ_PORT, &GPIO_InitStructure);     // PC2  IRQ

    GPIO_InitStructure.GPIO_Pin = NRF24_CE_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(NRF24_CE_PORT, &GPIO_InitStructure);      // PC3  CE

    GPIO_InitStructure.GPIO_Pin = NRF24_CSN_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(NRF24_CSN_PORT, &GPIO_InitStructure);     // PC4  CSN

    // PC5-->SCK    PC6-->MOSI-out   PC7-->MISO-in
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5|GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;                           // SPI host
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_16;      // SPI clock=Fsys/8
    SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;      // FullDuplex
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;                            // Mode 0
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;                              // ------
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;                       // 8Bit data mode
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;                               // Software CS
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;                      // High bit first
    SPI_Init(SPI1, &SPI_InitStructure);

    SPI_Cmd(SPI1, ENABLE); //Enable SPI
}

/////////////////////////////////////////////////////////////////////
u8 SPI_Transfer(u8 byte) {
    u16 timeout = 0;
    while(SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE) == RESET) {
        timeout++;
        if(timeout >= 500) {
            timeout = 0;
            return 0xFF;
        }
    }
    SPI_I2S_SendData(SPI1, byte);

    timeout = 0;
    while(SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE) == RESET) {
        timeout++;
        if(timeout >= 500) {
            timeout = 0;
            return 0xFF;
        }
    }
    return SPI_I2S_ReceiveData(SPI1);
}

/////////////////////////////////////////////////////////////////////
void CS_Select(void) {
    GPIO_WriteBit(NRF24_CSN_PORT, NRF24_CSN_PIN, Bit_RESET);
}

/////////////////////////////////////////////////////////////////////
void CS_UnSelect(void) {
    GPIO_WriteBit(NRF24_CSN_PORT, NRF24_CSN_PIN, Bit_SET);
}

/////////////////////////////////////////////////////////////////////
void CE_Enable(void) {
    GPIO_WriteBit(NRF24_CE_PORT, NRF24_CE_PIN, Bit_SET);
}

/////////////////////////////////////////////////////////////////////
void CE_Disable(void) {
    GPIO_WriteBit(NRF24_CE_PORT, NRF24_CE_PIN, Bit_RESET);
}

/////////////////////////////////////////////////////////////////////
u8 NRF24_WriteReg(u8 reg, u8 value) {
    u8 status;

    CS_Select();
    status = SPI_Transfer(reg);
    SPI_Transfer(value);
    CS_UnSelect();

    return status;
}

/////////////////////////////////////////////////////////////////////
u8 NRF24_ReadReg(u8 reg) {
    u8 value;

    CS_Select();
    SPI_Transfer(reg);
    value = SPI_Transfer(0);
    CS_UnSelect();

    return value;
}

/////////////////////////////////////////////////////////////////////
u8 NRF24_WriteBuffer(u8 reg, u8 *pBuf, u8 len) {
    u8 status, cnt;
    CS_Select();
    status = SPI_Transfer(reg);

    for(cnt=0; cnt<len; cnt++) {
        SPI_Transfer(*pBuf++);
    }

    CS_UnSelect();
    return status;
}

/////////////////////////////////////////////////////////////////////
u8 NRF24_ReadBuffer(u8 reg, u8 *pBuf, u8 len) {
    u8 status, cnt;
    CS_Select();
    status = SPI_Transfer(reg);

    for(cnt=0; cnt<len; cnt++) {
        pBuf[cnt] = SPI_Transfer(0xFF);
    }

    CS_UnSelect();
    return status;
}

/////////////////////////////////////////////////////////////////////
u8 NRF24_ReceivePacket(u8 *rxbuf) {
    u8 state;
    state = NRF24_ReadReg(STATUS);
    NRF24_WriteReg(WRITE_REG+STATUS, state);

    if(state&RX_OK) {
        CE_Disable();
        NRF24_ReadBuffer(RD_RX_PLOAD, rxbuf, RX_PLOAD_WIDTH);
        NRF24_WriteReg(FLUSH_RX, 0xFF);
        CE_Enable();
        Delay_Us(150);
        return 0;
    }

    return 1;
}

/////////////////////////////////////////////////////////////////////
u8 NRF24_SendPacket(u8 *txbuf) {
    u8 state;

    CE_Disable();
    NRF24_WriteBuffer(WR_TX_PLOAD, txbuf, TX_PLOAD_WIDTH);
    CE_Enable();

    //while(NRF_IRQ_INPUT());
    while(GPIO_ReadInputDataBit(NRF24_IRQ_PORT, NRF24_IRQ_PIN) == SET);

    state = NRF24_ReadReg(STATUS);
    NRF24_WriteReg(WRITE_REG+STATUS, state);

    if(state&MAX_TX) {
        NRF24_WriteReg(FLUSH_TX, 0xFF);
        return MAX_TX;
    }

    if(state&TX_OK) {
        return TX_OK;
    }

    return 0xFF;
}

/////////////////////////////////////////////////////////////////////
u8 NRF24_Check(void) {
    u8 ckin[5] = {0x11, 0x22, 0x33, 0x44, 0x55};
    u8 ckout[5] = {0};

    NRF24_WriteBuffer(WRITE_REG+TX_ADDR, ckin, 5);
    NRF24_ReadBuffer(READ_REG+TX_ADDR, ckout, 5);

    if((ckout[0]==0x11)&&(ckout[1]==0x22)&&(ckout[2]==0x33)&&(ckout[3]==0x44)&&(ckout[4]==0x55)) {
        return 0;
    }
    else {
        return 1;
    }
}

/////////////////////////////////////////////////////////////////////
void NRF24_Transmit(u8 *buf) {
    CE_Disable();
    NRF24_WriteReg(WRITE_REG+CONFIG, 0x0E);
    CE_Enable();
    Delay_Us(15);
    NRF24_SendPacket(buf);
    CE_Disable();
    NRF24_WriteReg(WRITE_REG+CONFIG, 0x0F);
    CE_Enable();
}

/////////////////////////////////////////////////////////////////////
void NRF24_Config(void) {
    CE_Disable();
    NRF24_WriteReg(WRITE_REG+RX_PW_P0, RX_PLOAD_WIDTH);
    NRF24_WriteReg(FLUSH_RX, 0xFF);
    NRF24_WriteBuffer(WRITE_REG+TX_ADDR, (u8*)TX_ADDRESS, TX_ADR_WIDTH);
    NRF24_WriteBuffer(WRITE_REG+RX_ADDR_P0, (u8*)RX_ADDRESS, RX_ADR_WIDTH);
    NRF24_WriteReg(WRITE_REG+EN_AA, 0x01);
    NRF24_WriteReg(WRITE_REG+EN_RXADDR, 0x01);
    NRF24_WriteReg(WRITE_REG+SETUP_RETR, 0x1A);
    NRF24_WriteReg(WRITE_REG+RF_CH, 0);
    NRF24_WriteReg(WRITE_REG+RF_SETUP, 0x0F);
    NRF24_WriteReg(WRITE_REG+CONFIG, 0x0F);
    CE_Enable();
}

