/*
*********************************************************************************************************
*
*	模块名称 : 中断模块
*	文件名称 : stm32f4xxx_it.c
*	版    本 : 
*	说    明 : 本文件存放中断服务函数。
*********************************************************************************************************
*/

#include "bsp.h"
#define ERR_INFO "\r\nEnter HardFault_Handler, System Halt.\r\n"

/* 声明外部变量供本文件使用 */
//extern uint8_t aRxBuffer[];
//extern uint8_t uart2_rx_buffer[];
//extern uint32_t true_length ;

//extern uint8_t uart2_RX_DMA_start; 
//extern uint8_t uart2_RX_DMA_finish; 
uint8_t UART2_Timeout; 
uint32_t temp_length = 0; 

/*
*********************************************************************************************************
*	Cortex-M3 内核异常中断服务程序
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*	函 数 名: NMI_Handler
*	功能说明: 不可屏蔽中断服务程序。
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void NMI_Handler(void)
{
}

/*
*********************************************************************************************************
*	函 数 名: HardFault_Handler
*	功能说明: 硬件故障中断服务程序。其他异常处理被关闭，而又发生了异常，则触发。
*			  执行异常处理时，发生了异常，则触发。复位时默认使能。
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void HardFault_Handler(void)
{
#if 1
  const char *pError = ERR_INFO;
  uint8_t i;

  for (i = 0; i < sizeof(ERR_INFO); i++)
  {
     USART1->DR = pError[i];
     /* 等待发送结束 */
     while ((USART1->SR & USART_FLAG_TC) == (uint16_t)RESET);
  }
#endif

  /* 当硬件失效异常发生时进入死循环 */
  while (1)
  {
  }
}

/*
*********************************************************************************************************
*	函 数 名: MemManage_Handler
*	功能说明: 内存管理异常中断服务程序。违反MPU设定的存储器访问规则时触发。 复位时默认未使能
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void MemManage_Handler(void)
{
  /* 当内存管理异常发生时进入死循环 */
  while (1)
  {
  }
}

/*
*********************************************************************************************************
*	函 数 名: BusFault_Handler
*	功能说明: 总线访问异常中断服务程序。取指令、数据读写、堆栈操作出现异常。 复位时默认未使能
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void BusFault_Handler(void)
{
  /* 当总线异常时进入死循环 */
  while (1)
  {
  }
}

/*
*********************************************************************************************************
*	函 数 名: UsageFault_Handler
*	功能说明: 用法错误中断服务程序。执行未定义指令、非对齐操作、除零时触发。 复位时默认未使能
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void UsageFault_Handler(void)
{
  /* 当用法异常时进入死循环 */
  while (1)
  {
  }
}

/*
*********************************************************************************************************
*	函 数 名: SVC_Handler
*	功能说明: 通过SWI指令的系统服务调用中断服务程序。
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void SVC_Handler(void)
{
}

/*
*********************************************************************************************************
*	函 数 名: DebugMon_Handler
*	功能说明: 调试监视器中断服务程序。
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void DebugMon_Handler(void)
{
}

/*
*********************************************************************************************************
*	函 数 名: PendSV_Handler
*	功能说明: 可挂起的系统服务调用中断服务程序。
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void PendSV_Handler(void)
{
}

/*
*********************************************************************************************************
*	函 数 名: USART3_IRQHandler
*	功能说明: 串口3中断服务程序。
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void USART3_IRQHandler(void)
{
	if(USART_GetITStatus(USART3,USART_IT_RXNE)!=RESET){
		uart3_rx_buffer[uart3_rx_data_length++] = USART_ReceiveData(USART3);
		USART_ClearITPendingBit(USART3,USART_IT_RXNE);
		if(uart3_rx_data_length >= uart3_rx_buffer_size){
			//printf("uart3数据溢出\n");
			uart3_rx_data_length = 0;	
			memset(uart3_rx_buffer,0,200);
		}
	}
	else if(USART_GetITStatus(USART3,USART_IT_IDLE)!=RESET){//空闲中断
//		uint8_t clear;
//		clear = USART3->SR;
//		clear = USART3->DR;//读取SR DR数据可清除 空闲中断 标志
		(void)USART3->SR;
		(void)USART3->DR;//读取SR DR数据可清除 空闲中断 标志
		uart3_rx_finish = 1; //串口3接收到一帧数据标志位
		
		/*在此放置处理函数*/
		//printf("uart3_data_length:[%d]\n",uart3_rx_data_length);
		uart3_rx_data_length = 0;	
	}
}


void BASIC_TIM_IRQHandler(void)
{
	volatile int length = 0;
	if(TIM_GetITStatus( BASIC_TIM, TIM_IT_Update) != RESET) {	
		UART2_Timeout++;
		if (USART_GetFlagStatus(USART2, USART_FLAG_ORE) != RESET) {
			(void)USART2->SR;
			(void)USART2->DR; //先读SR再读DR寄存器，可以清除ORE标志位
			printf("inside clear ORE flag:%d\n",USART_GetFlagStatus(USART2, USART_FLAG_ORE));
		}
		if(uart2_RX_DMA_start){
			length = uart2_rx_buffer_size - DMA_GetCurrDataCounter(USART2_RX_DMA_STREAM);
			DMA_ClearITPendingBit(USART2_RX_DMA_STREAM, DMA_IT_TEIF5);	//清除发送错误标志
			if (length != temp_length){ //一个定时器周期内，length发生变化，则认为未完成传输
				UART2_Timeout = 0;
				temp_length = length;
			}
			else if(UART2_Timeout >= 5){ //如果超过10ms没有收到新数据
				UART2_Timeout=0;
				if (length > 0 ){
					//printf("OREflag:[%d],uart2_RX_DMA_finish:%d,length:%d\n",USART_GetFlagStatus(USART2, USART_FLAG_ORE),uart2_RX_DMA_finish,length);	
					uart2_RX_DMA_start = 0;			
					DMA_Cmd(USART2_RX_DMA_STREAM, DISABLE); 					//关闭DMA,防止处理其间有数据
					while(DMA_GetCmdStatus(USART2_RX_DMA_STREAM) != DISABLE);// 等待DMA已经关闭
					DMA_ClearFlag(USART2_RX_DMA_STREAM,USART2_RX_DMA_FLAG_FEIF | USART2_RX_DMA_FLAG_DMEIF | USART2_RX_DMA_FLAG_TEIF | USART2_RX_DMA_FLAG_HTIF | USART2_RX_DMA_FLAG_TCIF);
					DMA_SetCurrDataCounter(USART2_RX_DMA_STREAM,uart2_rx_buffer_size);//重新设置DMA传输长度
			
					/*在此处之后可以访问uart2_rx_buffer，获取串口2数据*/		
					if(uart2_RX_DMA_finish == 0){ //添加限制，只有上一帧处理完成状态，才可以进行新数据的覆盖
						memcpy((char *)uart2_rx_buffer,(char *)aRxBuffer, uart2_rx_buffer_size);//复制数据
						uart2_rx_data_length = length;
						uart2_RX_DMA_finish = 1;	
					}
					else{
						printf("【写入失败，线程锁保护】\n");
					}
					temp_length = 0;
					memset(aRxBuffer,0,uart2_rx_buffer_size);
					DMA_Cmd(USART2_RX_DMA_STREAM, ENABLE); 
					uart2_RX_DMA_start = 1;	
				}
			}
		}
		else {
			UART2_Timeout = 0;
		}
		TIM_ClearITPendingBit(BASIC_TIM , TIM_IT_Update);  		 
	}		 	
}
