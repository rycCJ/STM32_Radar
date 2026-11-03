/*
*********************************************************************************************************
*	                                  
*	模块名称 : 串口驱动模块    
*	文件名称 : bsp_uart.h
*	版    本 : 
*	说    明 : 头文件
*
*********************************************************************************************************
*/

#ifndef __BSP_UART_H
#define __BSP_UART_H

#include "bsp.h"

/*****************************定时器配置***********************************/
#define BASIC_TIM           		TIM6
#define BASIC_TIM_CLK       		RCC_APB1Periph_TIM6
#define BASIC_TIM_IRQn					TIM6_DAC_IRQn
#define BASIC_TIM_IRQHandler    TIM6_DAC_IRQHandler


/******************************* USART2 **********************************/
#define USART2_CLK                       RCC_APB1Periph_USART2
#define USART2_CLK_INIT                  RCC_APB1PeriphClockCmd
#define USART2_IRQn                      USART2_IRQn
#define USART2_IRQHandler                USART2_IRQHandler

#define USART2_TX_PIN                    GPIO_Pin_2               
#define USART2_TX_GPIO_PORT              GPIOA                       
#define USART2_TX_GPIO_CLK               RCC_AHB1Periph_GPIOA
#define USART2_TX_SOURCE                 GPIO_PinSource2
#define USART2_TX_AF                     GPIO_AF_USART2

#define USART2_RX_PIN                    GPIO_Pin_3               
#define USART2_RX_GPIO_PORT              GPIOA                    
#define USART2_RX_GPIO_CLK               RCC_AHB1Periph_GPIOA
#define USART2_RX_SOURCE                 GPIO_PinSource3
#define USART2_RX_AF                     GPIO_AF_USART2

#define USART2_DMA                       DMA1
#define USART2_DMAx_CLK                  RCC_AHB1Periph_DMA1
 
#define USART2_TX_DMA_CHANNEL            DMA_Channel_4
#define USART2_TX_DMA_STREAM             DMA1_Stream6
#define USART2_TX_DMA_FLAG_FEIF          DMA_FLAG_FEIF6
#define USART2_TX_DMA_FLAG_DMEIF         DMA_FLAG_DMEIF6
#define USART2_TX_DMA_FLAG_TEIF          DMA_FLAG_TEIF6
#define USART2_TX_DMA_FLAG_HTIF          DMA_FLAG_HTIF6
#define USART2_TX_DMA_FLAG_TCIF          DMA_FLAG_TCIF6
		  
#define USART2_RX_DMA_CHANNEL            DMA_Channel_4
#define USART2_RX_DMA_STREAM             DMA1_Stream5
#define USART2_RX_DMA_FLAG_FEIF          DMA_FLAG_FEIF5
#define USART2_RX_DMA_FLAG_DMEIF         DMA_FLAG_DMEIF5
#define USART2_RX_DMA_FLAG_TEIF          DMA_FLAG_TEIF5
#define USART2_RX_DMA_FLAG_HTIF          DMA_FLAG_HTIF5
#define USART2_RX_DMA_FLAG_TCIF          DMA_FLAG_TCIF5

#define USART2_DMA_TX_IRQn               DMA1_Stream6_IRQn
#define USART2_DMA_RX_IRQn               DMA1_Stream5_IRQn
#define USART2_DMA_TX_IRQHandler         DMA1_Stream6_IRQHandler
#define USART2_DMA_RX_IRQHandler         DMA1_Stream5_IRQHandler


/******************************* USART3 **********************************/
#define USART3_CLK												RCC_APB1Periph_USART3
#define USART3_CLK_INIT                  	RCC_AHB1PeriphClockCmd
#define USART3_IRQn                      	USART3_IRQn
#define USART3_IRQHandler            			USART3_IRQHandler

#define USART3_RX_GPIO_PORT               GPIOB
#define USART3_RX_GPIO_CLK                RCC_AHB1Periph_GPIOB
#define USART3_RX_PIN                     GPIO_Pin_11
#define USART3_RX_AF                      GPIO_AF_USART3
#define USART3_RX_SOURCE                  GPIO_PinSource11

#define USART3_TX_GPIO_PORT               GPIOB
#define USART3_TX_GPIO_CLK                RCC_AHB1Periph_GPIOB
#define USART3_TX_PIN                     GPIO_Pin_10
#define USART3_TX_AF                      GPIO_AF_USART3
#define USART3_TX_SOURCE                  GPIO_PinSource10

/******************************* USART1 **********************************/
#define USART1_CLK                        RCC_APB2Periph_USART1
#define USART1_CLK_INIT 									RCC_AHB1PeriphClockCmd
#define USART1_IRQHandler                 USART1_IRQHandler
#define USART1_IRQ                 				USART1_IRQn

#define USART1_RX_GPIO_PORT               GPIOA
#define USART1_RX_GPIO_CLK                RCC_AHB1Periph_GPIOA
#define USART1_RX_PIN                     GPIO_Pin_10
#define USART1_RX_AF                      GPIO_AF_USART1
#define USART1_RX_SOURCE                  GPIO_PinSource10

#define USART1_TX_GPIO_PORT               GPIOA
#define USART1_TX_GPIO_CLK                RCC_AHB1Periph_GPIOA
#define USART1_TX_PIN                     GPIO_Pin_9
#define USART1_TX_AF                      GPIO_AF_USART1
#define USART1_TX_SOURCE                  GPIO_PinSource9



#define		uart2_rx_buffer_size                      4000
#define		uart3_rx_buffer_size                      300
extern uint8_t uart2_RX_DMA_start;         
extern uint8_t uart2_RX_DMA_finish; 
extern uint8_t uart3_rx_finish;
extern volatile uint32_t uart2_rx_data_length;
extern uint32_t uart3_rx_data_length;

extern uint8_t aRxBuffer[uart2_rx_buffer_size];
extern uint8_t uart2_rx_buffer[uart2_rx_buffer_size];
extern uint8_t uart3_rx_buffer[uart3_rx_buffer_size];


void bsp_init_uart2_dma(void);    
void USART2_DMA_RxConfig(uint32_t *BufferDST, uint32_t BufferSize);
void TIMx_Configuration(void);   
void bsp_init_uart1(void);
void bsp_init_uart3(void);

void Usart_SendStr_length( USART_TypeDef * pUSARTx, uint8_t *str,uint32_t strlen );
void Usart_SendString( USART_TypeDef * pUSARTx, uint8_t *str);

#endif


