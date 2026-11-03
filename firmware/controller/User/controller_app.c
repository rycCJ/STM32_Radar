#include "controller_link.h"
#include "bsp.h"			

__IO uint32_t TimeOut = 0x00; 

#define FFT_DATA_LEN  1204 /*1D FFT 1chirp 数据长度*/
uint8_t trans_target[6]={0};
static uint16_t pointcloud_data_length = 0;
static uint16_t pointcloud_pointnum = 0;
static uint8_t* g_Raw_data = &uart2_rx_buffer[FFT_DATA_LEN + 0];
static float range = 0;
static float velocity = 0;
static float azimuth_angle = 0;
static float elevation_angle = 0;
	
//	int32_t iRunTime = 0; //用于打印代码运行时间
	
void ControllerLegacy_Init(void)
{
	bsp_Init();	/*所有硬件外设的初始化*/
	
	/*STM32上电时，硬复位雷达*/
	GPIO_SetBits(GPIOF,GPIO_Pin_10);
	printf(" hard reset start\n");
	GPIO_ResetBits(GPIOC,GPIO_Pin_3);
	bsp_DelayMS(200);
	GPIO_SetBits(GPIOC,GPIO_Pin_3);
	
	bsp_DelayMS(2000); // 等待雷达系统启动
	Usart_SendString(USART2, (uint8_t*)"AT+RESET\n");
	bsp_DelayMS(500);
//	// 选择ADC采集输出模式
//	Usart_SendString(USART2, (uint8_t*)"AT+PROG=01\n");
//	bsp_DelayMS(500);
	Usart_SendString(USART2, (uint8_t*)"AT+START\n"); //发送开始指令
	bsp_StartAutoTimer(1, 100);//定时器	
	
}
static void Controller_ProcessRadar(void)
{	
		
		if(uart2_RX_DMA_finish){ //串口2接收到一帧数据（来自雷达）
			/*数据帧帧头校验*/
			if(uart2_rx_buffer[0] == 0x66 && uart2_rx_buffer[1] == 0xBB 
				&& uart2_rx_buffer[FFT_DATA_LEN-2] == 0xBB && uart2_rx_buffer[FFT_DATA_LEN-1] == 0x66
				&& uart2_rx_buffer[FFT_DATA_LEN +0] == 0x55 && uart2_rx_buffer[FFT_DATA_LEN +1] == 0xaa){
					
				printf("length:%d,framenum:%d,framehead:%02X,%02X,%02X,%02X,%02X,%02X,length:%d,%d\n"\
					,uart2_rx_data_length,uart2_rx_buffer[FFT_DATA_LEN +4] + uart2_rx_buffer[FFT_DATA_LEN +5]*256\
					,uart2_rx_buffer[0],uart2_rx_buffer[1],uart2_rx_buffer[FFT_DATA_LEN-2],uart2_rx_buffer[FFT_DATA_LEN-1],uart2_rx_buffer[FFT_DATA_LEN +0],uart2_rx_buffer[FFT_DATA_LEN +1]\
					,uart2_rx_buffer[FFT_DATA_LEN +2] + uart2_rx_buffer[FFT_DATA_LEN +3]*256 +6 ,uart2_rx_data_length-1204
				);	
				pointcloud_data_length = uart2_rx_buffer[FFT_DATA_LEN +2] + uart2_rx_buffer[FFT_DATA_LEN +3]*256 + uart2_rx_buffer[FFT_DATA_LEN +4]*256*256 + uart2_rx_buffer[FFT_DATA_LEN +5]*256*256*256; //点云数据：数据长度
				pointcloud_pointnum = uart2_rx_buffer[FFT_DATA_LEN +10] + uart2_rx_buffer[FFT_DATA_LEN +11]*256;//点云数据-点数
					printf("[debug]:pointnum:%d\n",pointcloud_pointnum);
				if(pointcloud_data_length + 6 + FFT_DATA_LEN == uart2_rx_data_length){ //长度校验
					for (int i = 0; i < pointcloud_pointnum; i++){
						range = ((g_Raw_data[12 + i * 9] + (g_Raw_data[13 + i * 9] << 8)) * 0.05004);    // 解析距离
						velocity = (g_Raw_data[14 + i * 9] - 32) * 0.104; //解析速度
						if (g_Raw_data[15 + i * 9] <= 63){ //方位角
							azimuth_angle = asin(g_Raw_data[15 + i * 9] / 64.0); 
						}
						else if (g_Raw_data[15 + i * 9] <= 127){
							azimuth_angle = asin((g_Raw_data[15 + i * 9] - 128) / 64.0);
						}		
						if (g_Raw_data[16 + i * 9] <= 63){ //俯仰角
							elevation_angle = asin(g_Raw_data[16 + i * 9] / 64.0);
						}
						else if (g_Raw_data[16 + i * 9] <= 127){
							elevation_angle = asin((g_Raw_data[16 + i * 9] - 128) / 64.0); 
						}
							printf("[debug]:range:%.3f,velocity:%.3f,azimuth_angle:%.3f,elevation_angle:%.3f\n",range,velocity,azimuth_angle,elevation_angle);
					}
					
					/*此处可以放置相应算法*/	
					/*运行结果可通过串口1（可连接电脑串口助手）和串口3（连接通信模块）输出*/
				
				}
			}
			/*该帧数据处理完后，再执行缓冲和标志位的清空*/
			memset(uart2_rx_buffer,0,uart2_rx_buffer_size);                        
			uart2_RX_DMA_finish = 0;
		}
		
}
static void Controller_ProcessComm(void)
{
		if(uart3_rx_finish){ //串口3接收到一帧数据（来自通信模块）。可进行远程下发参数等处理
			printf("data:[%s]\n",uart3_rx_buffer);    
			
			/*这里放置串口3对应的处理*/
			
			/*最后清空缓存和标志位*/
			memset(uart3_rx_buffer,0,uart3_rx_buffer_size);
			uart3_rx_finish = 0;
		}
		
		
}
static void Controller_Report(void)
{
		if(bsp_CheckTimer(1) == 1){ // 100ms定时器，即每100ms进入一次该条件
			/*可用来定时上报结果给通信模块*/
			trans_target[0] = 0x55;
			trans_target[1] = 0xAA;
			trans_target[2] = pointcloud_pointnum & 0xFF;         // 低字节
			trans_target[3] = (pointcloud_pointnum >> 8) & 0xFF;  // 高字节
			trans_target[4] = 0xAA;
			trans_target[5] = 0x55;
			Usart_SendStr_length(USART3,(uint8_t *)trans_target,sizeof(trans_target));
		}
		
	
}
void ControllerLegacy_Poll(void)
{
 Controller_ProcessRadar();
 Controller_ProcessComm();
 Controller_Report();
}

void Controller_Init(void) {
    bsp_Init();
    ControllerLink_Init();
    bsp_StartAutoTimer(1,100);
}
void Controller_Poll(void) {
    ControllerLink_Poll();
    if (uart3_rx_finish) {
        memset(uart3_rx_buffer,0,uart3_rx_buffer_size); uart3_rx_finish=0;
    }
    if (bsp_CheckTimer(1)==1) {
        uint16_t count=ControllerPoints_Count();
        uint8_t report[6]={0x55,0xAA,0,0,0xAA,0x55};
        report[2]=(uint8_t)count; report[3]=(uint8_t)(count>>8);
        Usart_SendStr_length(USART3,report,6);
    }
}
