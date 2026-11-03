/*
*********************************************************************************************************
*	                                  
*	模块名称 : 串口驱动模块    
*	文件名称 : bsp_uart.c
*	版    本 : V1.0
*	说    明 : 
*
*********************************************************************************************************
*/
#include "bsp.h"

#define TIMEOUT_MAX              10000 /* 定义最大的溢出数值 */

uint8_t uart2_RX_DMA_start = 0;         
uint8_t uart2_RX_DMA_finish = 0; //代替原来的flag 
uint8_t uart3_rx_finish = 0;

volatile uint32_t uart2_rx_data_length = 0;
uint32_t uart3_rx_data_length = 0;

uint8_t aRxBuffer[uart2_rx_buffer_size];
uint8_t uart2_rx_buffer[uart2_rx_buffer_size];
uint8_t uart3_rx_buffer[uart3_rx_buffer_size];

/* 供本文件使用的函数 */
static void TimeOut_UserCallback(void);

/*
*********************************************************************************************************
*	函 数 名: 超时处理措施
*	功能说明: 初始化CPU的USART1串口硬件设备。未启用中断。
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/
static void TimeOut_UserCallback(void)
{
	while (1)
	{ 
		
	}
}


/*
*********************************************************************************************************
*	函 数 名: bsp_init_uart2
*	功能说明: 初始化CPU的USART2串口硬件设备。
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/

void bsp_init_uart2(void)
{
	USART_InitTypeDef USART_InitStructure;
	GPIO_InitTypeDef GPIO_InitStructure;
	RCC_AHB1PeriphClockCmd(USART2_TX_GPIO_CLK | USART2_RX_GPIO_CLK, ENABLE);/* 使能 GPIO clock */
	USART2_CLK_INIT(USART2_CLK, ENABLE);/* 使能 USART clock */
	RCC_AHB1PeriphClockCmd(USART2_DMAx_CLK, ENABLE);/* 使能  DMA clock */

	GPIO_PinAFConfig(USART2_TX_GPIO_PORT, USART2_TX_SOURCE, USART2_TX_AF);
	GPIO_PinAFConfig(USART2_RX_GPIO_PORT, USART2_RX_SOURCE, USART2_RX_AF);

	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;

	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_InitStructure.GPIO_Pin = USART2_TX_PIN;
	GPIO_Init(USART2_TX_GPIO_PORT, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_InitStructure.GPIO_Pin = USART2_RX_PIN;
	GPIO_Init(USART2_RX_GPIO_PORT, &GPIO_InitStructure);

	USART_InitStructure.USART_BaudRate = 921600;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;

	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(USART2, &USART_InitStructure);
		 
	USART_Cmd(USART2, ENABLE);
	USART_DMACmd(USART2, USART_DMAReq_Rx, ENABLE);	/* 使能 USART DMA RX 请求 */
	USART_DMACmd(USART2, USART_DMAReq_Tx, ENABLE);/* 使能 USART DMA TX 请求 */
	
}

/*
*********************************************************************************************************
*	函 数 名: USART2_DMA_RxConfig
*	功能说明: 串口的DMA接受配置
*	形    参：BufferDST  目标地址指针
*             BufferSize 缓冲区大小
*	返 回 值: 无
*********************************************************************************************************
*/
void USART2_DMA_RxConfig(uint32_t *BufferDST, uint32_t BufferSize)
{
	DMA_InitTypeDef  DMA_InitStructure;
	__IO uint32_t    Timeout = TIMEOUT_MAX;
	/* 
	  使能前检测DMA是否被禁止。
	  请注意，在使用相同数据流的多次传输时，这一步很有用：使能，然后禁止，然后使能
	  ..... 在这种情况下，只有在数据传输结束的时候，DMA的禁止才有效，在确保使能位被
	  硬件清除之前做确认是不可能的，如果DMA传输只做一次，这一步可以忽略。
	*/
	DMA_Cmd(USART2_RX_DMA_STREAM, DISABLE); 
	while (DMA_GetCmdStatus(USART2_RX_DMA_STREAM) != DISABLE)
	{
	}
	
	DMA_InitStructure.DMA_BufferSize = BufferSize;                 /* 配置DMA大小 */
	DMA_InitStructure.DMA_FIFOMode = DMA_FIFOMode_Enable;          /* 在这个程序里面使能或者禁止都可以的 */
	DMA_InitStructure.DMA_FIFOThreshold = DMA_FIFOThreshold_Full;  /* 设置阀值 */
	DMA_InitStructure.DMA_MemoryBurst = DMA_MemoryBurst_Single ;   /* 设置内存为单字节突发模式 */
	DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;/* 设置内存数据的位宽是字节 */
	DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;        /* 使能地址自增 */
	DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;                  /* 设置DMA是正常模式 */
	DMA_InitStructure.DMA_PeripheralBaseAddr =(uint32_t) (&(USART2->DR)) ;  /* 设置外设地址 */
	DMA_InitStructure.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;     /* 设置外设为单字节突发模式 */
	DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte; /* 设置外设数据的位宽是字节 */
	DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;      /* 禁止外设地址自增 */
	DMA_InitStructure.DMA_Priority = DMA_Priority_High;                   /* 设置优先级 */
	
	/* 配置 RX DMA */
	DMA_InitStructure.DMA_Channel = USART2_RX_DMA_CHANNEL ;      /* 配置接收通道 */
	DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralToMemory ;     /* 设置从外设到内存 */
	DMA_InitStructure.DMA_Memory0BaseAddr =(uint32_t)BufferDST ; /* 设置内存地址 */
	DMA_Init(USART2_RX_DMA_STREAM,&DMA_InitStructure);
	
	/* 使能 DMA USART TX Stream */
	DMA_Cmd(USART2_RX_DMA_STREAM, ENABLE); 
	
	/* 
	   检测DMA Stream 是否被正确的使能.如果DMA的参数配置错误了，那么DMA Stream
	   的使能位会被硬件清除掉，从而使得传输停止比如FIFO的阀值配置错误
	*/
	Timeout = TIMEOUT_MAX;
	while ((DMA_GetCmdStatus(USART2_RX_DMA_STREAM) != ENABLE) && (Timeout-- > 0))
	{
		
	}
  //printf("timeout:%d\n",Timeout);
	/* 检测是否超时 */
	if (Timeout <= 0)
	{
		printf("UART2_RX_DMA开启失败，进入死循环\n");
		/* 处理错误 */
		TimeOut_UserCallback();
	}
}

/**
  * @brief  初始化基本定时器定时，1ms产生一次中断
  * @param  无
  * @retval 无 
  */
/*
 * 注意：TIM_TimeBaseInitTypeDef结构体里面有5个成员，TIM6和TIM7的寄存器里面只有

 * TIM_Prescaler和TIM_Period，所以使用TIM6和TIM7的时候只需初始化这两个成员即可，

 * 另外三个成员是通用定时器和高级定时器才有. 
 *-----------------------------------------------------------------------------

 * TIM_Prescaler         都有 
 * TIM_CounterMode			 TIMx,x[6,7]没有，其他都有（基本定时器） 
 * TIM_Period            都有 
 * TIM_ClockDivision     TIMx,x[6,7]没有，其他都有(基本定时器)

 * TIM_RepetitionCounter TIMx,x[1,8]才有(高级定时器) 
 *-----------------------------------------------------------------------------

 */
void TIMx_Configuration(void) 
{
	NVIC_InitTypeDef NVIC_InitStructure;  
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_0);		// 设置中断组为0
	NVIC_InitStructure.NVIC_IRQChannel = BASIC_TIM_IRQn; 	// 设置中断来源
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;	 // 设置抢占优先级
	// 设置子优先级  
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;	
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;  
	NVIC_Init(&NVIC_InitStructure);  
	TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;   
	RCC_APB1PeriphClockCmd(BASIC_TIM_CLK, ENABLE); // 开启TIMx_CLK,x[6,7]

  /* 累计 TIM_Period个后产生一个更新或者中断*/		   
  //当定时器从0计数到4999，即为5000次，为一个定时周期   
	TIM_TimeBaseStructure.TIM_Period = 20-1;        //10:对应1ms ;20 对应2ms
	
	//定时器时钟源TIMxCLK = 2 * PCLK1      
  //				PCLK1 = HCLK / 4     
  //				=> TIMxCLK=HCLK/2=SystemCoreClock/2=84MHz    
	// 设定定时器频率为=TIMxCLK/(TIM_Prescaler+1)=10000Hz    
	TIM_TimeBaseStructure.TIM_Prescaler = 8400-1;	// 8400 对应8400分频，即0.1ms
	TIM_TimeBaseInit(BASIC_TIM, &TIM_TimeBaseStructure);// 初始化定时器TIMx, x[2,3,4,5]
	TIM_ClearFlag(BASIC_TIM, TIM_FLAG_Update);// 清除定时器更新中断标志位
	TIM_ITConfig(BASIC_TIM,TIM_IT_Update,ENABLE);// 开启定时器更新中断
	TIM_Cmd(BASIC_TIM, ENABLE);	// 使能定时器   
}

/*
*********************************************************************************************************
*	函 数 名: UART2_DMA_init
*	功能说明: DMA、串口、定时器初始化
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/
void bsp_init_uart2_dma(void)
{
	bsp_init_uart2();
	TIMx_Configuration();
	USART2_DMA_RxConfig((uint32_t *)aRxBuffer, uart2_rx_buffer_size);
	uart2_RX_DMA_start = 1;
}


void bsp_init_uart1(void)
{
  GPIO_InitTypeDef GPIO_InitStructure;
  USART_InitTypeDef USART_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;
	
  USART1_CLK_INIT( USART1_RX_GPIO_CLK|USART1_TX_GPIO_CLK, ENABLE);
  RCC_APB2PeriphClockCmd(USART1_CLK, ENABLE);
  
  GPIO_PinAFConfig(USART1_RX_GPIO_PORT,USART1_RX_SOURCE, USART1_RX_AF);
  GPIO_PinAFConfig(USART1_TX_GPIO_PORT,USART1_TX_SOURCE,USART1_TX_AF);

  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
  GPIO_InitStructure.GPIO_Pin = USART1_TX_PIN  ;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(USART1_TX_GPIO_PORT, &GPIO_InitStructure);

  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
  GPIO_InitStructure.GPIO_Pin = USART1_RX_PIN;
  GPIO_Init(USART1_RX_GPIO_PORT, &GPIO_InitStructure);
			
  USART_InitStructure.USART_BaudRate = 921600;
  USART_InitStructure.USART_WordLength = USART_WordLength_8b;
  USART_InitStructure.USART_StopBits = USART_StopBits_1;
  USART_InitStructure.USART_Parity = USART_Parity_No ;
  USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
  USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
  USART_Init(USART1, &USART_InitStructure); 
	
  NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
  
  /* 配置中断源 */
  NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQ;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  NVIC_Init(&NVIC_InitStructure);
	
	/*配置串口接收中断*/
	USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
  USART_Cmd(USART1, ENABLE);

//	/* CPU的小缺陷：串口配置好，如果直接Send，则第1个字节发送不出去
//	如下语句解决第1个字节无法正确发送出去的问题 */
//	/* 清除串口传输完成标志 */
//	USART_ClearFlag(USARTx,USART_FLAG_TC); 
//	while(USART_GetFlagStatus(USARTx,USART_FLAG_TC)==RESET);

}

void bsp_init_uart3(void)
{
  GPIO_InitTypeDef GPIO_InitStructure;
  USART_InitTypeDef USART_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;
	
  USART3_CLK_INIT( USART3_RX_GPIO_CLK|USART3_TX_GPIO_CLK, ENABLE);
  RCC_APB1PeriphClockCmd(USART3_CLK, ENABLE);
  
  GPIO_PinAFConfig(USART3_RX_GPIO_PORT,USART3_RX_SOURCE, USART3_RX_AF);
  GPIO_PinAFConfig(USART3_TX_GPIO_PORT,USART3_TX_SOURCE,USART3_TX_AF);
	
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
  GPIO_InitStructure.GPIO_Pin = USART3_TX_PIN  ;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(USART3_TX_GPIO_PORT, &GPIO_InitStructure);

  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
  GPIO_InitStructure.GPIO_Pin = USART3_RX_PIN;
  GPIO_Init(USART3_RX_GPIO_PORT, &GPIO_InitStructure);
			
  USART_InitStructure.USART_BaudRate = 921600;
  USART_InitStructure.USART_WordLength = USART_WordLength_8b;
  USART_InitStructure.USART_StopBits = USART_StopBits_1;
  USART_InitStructure.USART_Parity = USART_Parity_No ;
  USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
  USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
  USART_Init(USART3, &USART_InitStructure); 
	
  NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
  
  /* 配置中断源 */
  NVIC_InitStructure.NVIC_IRQChannel = USART3_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  NVIC_Init(&NVIC_InitStructure);
	
	/*配置串口接收中断*/
	USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);
	USART_ITConfig(USART3, USART_IT_IDLE, ENABLE);	//串口空闲中断
	
  USART_Cmd(USART3, ENABLE);
}

/*****************  发送一个字符 **********************/
static void Usart_SendByte( USART_TypeDef * pUSARTx, uint8_t ch )
{
	/* 发送一个字节数据到USART1 */
	USART_SendData(pUSARTx,ch);
		
	/* 等待发送完毕 */
	while (USART_GetFlagStatus(pUSARTx, USART_FLAG_TXE) == RESET);	
}
/*****************  指定长度的发送字符串 **********************/
void Usart_SendStr_length( USART_TypeDef * pUSARTx, uint8_t *str,uint32_t strlen )
{
	unsigned int k=0;
    do 
    {
        Usart_SendByte( pUSARTx, *(str + k) );
        k++;
    } while(k < strlen);
}

/*****************  发送字符串 **********************/
void Usart_SendString( USART_TypeDef * pUSARTx, uint8_t *str)
{
	unsigned int k=0;
    do 
    {
        Usart_SendByte( pUSARTx, *(str + k) );
        k++;
    } while(*(str + k)!='\0');
}

