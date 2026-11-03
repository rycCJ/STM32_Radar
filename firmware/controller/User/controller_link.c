#include "bsp.h"
#include "controller_link.h"

#define RADAR_RING_SIZE 16384U
#define HOST_RING_SIZE 256U
static uint8_t radar_ring[RADAR_RING_SIZE],host_ring[HOST_RING_SIZE];
static volatile uint32_t radar_write,radar_read,host_write,host_read;
static volatile uint32_t radar_overflow,host_overflow,uart_errors;
static uint8_t bridge,discard_command;
static char command[64];
static uint32_t command_length;

static void host_text(const char *s) {
    while (*s) {
        while (USART_GetFlagStatus(USART1,USART_FLAG_TXE)==RESET) {}
        USART_SendData(USART1,(uint8_t)*s++);
    }
}
static void radar_pin(int enabled) {
    GPIO_InitTypeDef io;
    io.GPIO_Pin=GPIO_Pin_2;
    io.GPIO_Mode=enabled?GPIO_Mode_AF:GPIO_Mode_IN;
    io.GPIO_Speed=GPIO_Speed_100MHz;
    io.GPIO_OType=GPIO_OType_PP;
    io.GPIO_PuPd=GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOA,&io);
}
static void reset_pin(int enabled) {
    GPIO_InitTypeDef io;
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC,ENABLE);
    GPIO_SetBits(GPIOC,GPIO_Pin_3);
    io.GPIO_Pin=GPIO_Pin_3; io.GPIO_Mode=enabled?GPIO_Mode_OUT:GPIO_Mode_IN;
    io.GPIO_Speed=GPIO_Speed_50MHz; io.GPIO_OType=GPIO_OType_PP;
    io.GPIO_PuPd=GPIO_PuPd_NOPULL; GPIO_Init(GPIOC,&io);
}
static void select_link(int enabled) {
    USART_ITConfig(USART2,USART_IT_RXNE,DISABLE);
    if (bridge) {
        GPIO_ResetBits(GPIOC,GPIO_Pin_3); bsp_DelayMS(200);
        GPIO_SetBits(GPIOC,GPIO_Pin_3);
    }
    __disable_irq(); radar_write=radar_read=0; __enable_irq();
    ControllerPoints_Reset();
    radar_pin(enabled); reset_pin(enabled); bridge=(uint8_t)enabled;
    (void)USART2->SR; (void)USART2->DR;
    if (enabled) USART_ITConfig(USART2,USART_IT_RXNE,ENABLE);
}
int ControllerLink_Quiet(void) { return bridge; }
void ControllerLink_Init(void) {
    NVIC_InitTypeDef irq;
    extern void bsp_init_uart2(void);
    bsp_init_uart2();
    USART_DMACmd(USART2,USART_DMAReq_Rx|USART_DMAReq_Tx,DISABLE);
    irq.NVIC_IRQChannel=USART2_IRQn; irq.NVIC_IRQChannelPreemptionPriority=0;
    irq.NVIC_IRQChannelSubPriority=0; irq.NVIC_IRQChannelCmd=ENABLE;
    NVIC_Init(&irq);
    select_link(0);
    host_text("AT+LINK=DIRECT\n");
}
void USART1_IRQHandler(void) {
    uint32_t sr=USART1->SR;
    uint8_t v=(uint8_t)USART1->DR;
    uint32_t next=(host_write+1U)%HOST_RING_SIZE;
    if (sr&(USART_FLAG_ORE|USART_FLAG_FE|USART_FLAG_NE)) uart_errors++;
    if (sr&USART_FLAG_RXNE) {
        if (next==host_read) host_overflow++;
        else { host_ring[host_write]=v; host_write=next; }
    }
}
void USART2_IRQHandler(void) {
    uint32_t sr=USART2->SR;
    uint8_t v=(uint8_t)USART2->DR;
    uint32_t next=(radar_write+1U)%RADAR_RING_SIZE;
    if (sr&(USART_FLAG_ORE|USART_FLAG_FE|USART_FLAG_NE)) uart_errors++;
    if ((sr&USART_FLAG_RXNE)&&bridge) {
        if (next==radar_read) radar_overflow++;
        else { radar_ring[radar_write]=v; radar_write=next; }
    }
}
static void handle_command(void) {
    if (!strcmp(command,"AT+LINK=DIRECT")) {
        select_link(0); host_text("AT+LINK=DIRECT\n");
    } else if (!strcmp(command,"AT+LINK=BRIDGE")) {
        select_link(1); host_text("AT+LINK=BRIDGE\n");
    } else if (!strcmp(command,"AT+LINK?")) {
        host_text(bridge?"AT+LINK=BRIDGE\n":"AT+LINK=DIRECT\n");
    } else if (!strcmp(command,"AT+STATUS?")) {
        char result[120];
        sprintf(result,"AT+STATUS=%lu,%lu,%lu\n",(unsigned long)radar_overflow,
                (unsigned long)host_overflow,(unsigned long)uart_errors);
        host_text(result);
    } else if (bridge) {
        Usart_SendString(USART2,(uint8_t *)command);
        Usart_SendString(USART2,(uint8_t *)"\n");
    } else host_text("AT+ERROR=DIRECT\n");
}
void ControllerLink_Poll(void) {
    uint8_t v;
    static uint32_t previous_host_overflow;
    if (previous_host_overflow!=host_overflow) {
        previous_host_overflow=host_overflow; discard_command=1; command_length=0;
    }
    while (host_read!=host_write) {
        v=host_ring[host_read]; host_read=(host_read+1U)%HOST_RING_SIZE;
        if (v=='\r') continue;
        if (v=='\n') {
            if (!discard_command && command_length) {
                command[command_length]=0; handle_command();
            }
            command_length=0; discard_command=0;
        } else if (command_length<sizeof(command)-1 && !discard_command) command[command_length++]=(char)v;
        else discard_command=1;
    }
    /* Nonblocking drain: equal-baud UARTs must transmit a continuous stream. */
    if (bridge && radar_read!=radar_write && USART_GetFlagStatus(USART1,USART_FLAG_TXE)!=RESET) {
        v=radar_ring[radar_read]; radar_read=(radar_read+1U)%RADAR_RING_SIZE;
        USART_SendData(USART1,v); ControllerPoints_Byte(v);
    }
}
