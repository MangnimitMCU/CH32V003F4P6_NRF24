#include "debug.h"
#include "nrf24.h"

#define TX_MODE

/*
 *                     _________
 *              PD0 --|  |usb|  |-- PC7     MISO
 * DEBUG        PD1 --|         |-- PC6     MOSI
 *              PD2 --|         |-- PC5     SCK
 *              PD3 --|         |-- PC4     CSN
 *              PD4 --|         |-- PC3     CE
 * USART-TX     PD5 --|         |-- PC2     IRQ
 * USART-RX     PD6 --|         |-- PC1
 * NRST         PD7 --|         |-- PC0
 * EXT-OSCI     PA2 --|_________|-- PA1     EXT-OSCO
 */

u8 data[32];

/////////////////////////////////////////////////////////////////////
void GPIO_Config(void) {
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD, ENABLE);
#ifdef TX_MODE
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0|GPIO_Pin_2|GPIO_Pin_3|GPIO_Pin_4;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOD, &GPIO_InitStructure);
#else
    GPIO_WriteBit(GPIOD, GPIO_Pin_0, Bit_SET);
    GPIO_WriteBit(GPIOD, GPIO_Pin_2, Bit_SET);
    GPIO_WriteBit(GPIOD, GPIO_Pin_3, Bit_SET);
    GPIO_WriteBit(GPIOD, GPIO_Pin_4, Bit_SET);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0|GPIO_Pin_2|GPIO_Pin_3|GPIO_Pin_4;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOD, &GPIO_InitStructure);
#endif
}

/////////////////////////////////////////////////////////////////////
void USART_Config(void) {
    GPIO_InitTypeDef  GPIO_InitStructure = {0};
    USART_InitTypeDef USART_InitStructure = {0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD | RCC_APB2Periph_USART1, ENABLE);

    // USART1 TX-->PD5   RX-->PD6
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOD, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOD, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate = 9600;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;

    USART_Init(USART1, &USART_InitStructure);

    USART_Cmd(USART1, ENABLE);
}

/////////////////////////////////////////////////////////////////////
void USART_SendByte(u8 tx) {
    USART_SendData(USART1, tx);
    // waiting for sending finish
    while(USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
}

/////////////////////////////////////////////////////////////////////
void NRF24_USART_Transmit(u8 *data) {
    u8 cnt, size = *data++;
    for(cnt=0; cnt<size-1; cnt++) {
        USART_SendByte(*data++);
    }
}
#ifdef TX_MODE
/////////////////////////////////////////////////////////////////////
#define HEADER      0x0A
#define CMDON       0xFF
#define CMDOFF      0x00
void SendCmd(u8 cmd, u8 ch) {
    nrf24_buf[1] = HEADER;          // Header
    nrf24_buf[2] = ch;              // Channel
    nrf24_buf[3] = cmd;             // Cmd 0x00=OFF 0xFF=ON
    nrf24_buf[0] = 4;               // Data size
    NRF24_Transmit(nrf24_buf);
    Delay_Ms(50);
}
#endif

u8 ch1,ch2,ch3,ch4;
/////////////////////////////////////////////////////////////////////
int main(void) {
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    SystemCoreClockUpdate();
    Delay_Init();

    GPIO_Config();

    USART_Config();

    SPI_Config();

    Delay_Ms(1000);

    if(NRF24_Check() == RESET) {
        NRF24_Config();
    }

    while(1) {

#ifdef TX_MODE
        if(GPIO_ReadInputDataBit(GPIOD, GPIO_Pin_0) == Bit_RESET) {
            SendCmd(CMDON, 1);
            while(GPIO_ReadInputDataBit(GPIOD, GPIO_Pin_0) == Bit_RESET);
            ch1=1;
        }else if(ch1){
            ch1=0;
            SendCmd(CMDOFF, 1);
        }

        if(GPIO_ReadInputDataBit(GPIOD, GPIO_Pin_2) == Bit_RESET) {
            SendCmd(CMDON, 2);
            while(GPIO_ReadInputDataBit(GPIOD, GPIO_Pin_2) == Bit_RESET);
            ch2=1;
        }else if(ch2){
            ch2=0;
            SendCmd(CMDOFF, 2);
        }

        if(GPIO_ReadInputDataBit(GPIOD, GPIO_Pin_3) == Bit_RESET) {
            SendCmd(CMDON, 3);
            while(GPIO_ReadInputDataBit(GPIOD, GPIO_Pin_3) == Bit_RESET);
            ch3=1;
        }else if(ch3){
            ch3=0;
            SendCmd(CMDOFF, 3);
        }

        if(GPIO_ReadInputDataBit(GPIOD, GPIO_Pin_4) == Bit_RESET) {
            SendCmd(CMDON, 4);
            while(GPIO_ReadInputDataBit(GPIOD, GPIO_Pin_4) == Bit_RESET);
            ch4=1;
        }else if(ch4){
            ch4=0;
            SendCmd(CMDOFF, 4);
        }
#else
        if(NRF24_ReceivePacket(data) == 0) {
            NRF24_USART_Transmit(data);

            if(data[1] == 0x0A && data[3] == 0xFF) {
                switch(data[2]) {
                    case 1:GPIO_WriteBit(GPIOD, GPIO_Pin_0, Bit_RESET); break;
                    case 2:GPIO_WriteBit(GPIOD, GPIO_Pin_2, Bit_RESET); break;
                    case 3:GPIO_WriteBit(GPIOD, GPIO_Pin_3, Bit_RESET); break;
                    case 4:GPIO_WriteBit(GPIOD, GPIO_Pin_4, Bit_RESET); break;
                }
            }
            else if(data[1] == 0x0A && data[3] == 0x00) {
                switch(data[2]) {
                    case 1:GPIO_WriteBit(GPIOD, GPIO_Pin_0, Bit_SET); break;
                    case 2:GPIO_WriteBit(GPIOD, GPIO_Pin_2, Bit_SET); break;
                    case 3:GPIO_WriteBit(GPIOD, GPIO_Pin_3, Bit_SET); break;
                    case 4:GPIO_WriteBit(GPIOD, GPIO_Pin_4, Bit_SET); break;
                }
            }
        }
#endif
    }
}
