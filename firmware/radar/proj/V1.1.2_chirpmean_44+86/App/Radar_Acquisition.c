#include "Radar_Internal.h"
void Func01_Handle(void)
{
    static uint8_t scmd = 0;
    switch (scmd)
    {
    case 0: //开启数据采集
//          TEST_WritePin = 1;
//          firstChirpCnt = 0;
#if 1
        rampCnt = 0;
        bb_regb_str->PREP_CFG0 = bb_regb_str->PREP_CFG0 & ((uint64_t)0xFF000FFFFFFFFFFF);
        bb_regb_str->PREP_CFG0 = bb_regb_str->PREP_CFG0 | ((uint64_t)NUM_CHIRP1_TX << BB_BIT44_POS);
        RF_SPI_Init();
//          SYS_CLK_PLL_Init();
//          memset((uint8_t *)ADC_SAMPLE_ADDR,0,10*1024);
        BB_ISR_CLEAR(BB_ISR_PREP_FRAME_END | BB_ISR_PREP_RAMP_END);
        BB_OPGATE_EN(BB_GATE_PREP);
        waveGenStart();
        BB_waitISR(BB_ISR_PREP_FRAME_END);
        BB_OPGATE_DIS(BB_GATE_PREP);
        BB_OPCLEAR(BB_CLEAR_PREP);
//          SYS_CLK_HRC_Init();
        RF_CHIPEN_Disable();
#endif
//          TEST_WritePin = 0;
        scmd = 1;
        break;
    case 1: //数据处理
//          printf("SampOver=%d\n",ADC_MAX_LEN*NUM_ANT*NUM_CHIRP*ADC_BYTE);
#if 0  //current test
        Write_M32(0x50000000, 0xFFFFFFFF);
        BB_CLOCK_DIS;
        BB_CLOCK_EN;
        BB_OPGATE_DIS(0xFF);
        printf("bb dis\n");
        BB_OPGATE_EN(0xFF);
        printf("bb en\n");
#endif

//          USART_Transmit_Bytes(&USART0,(uint8_t *)&head[0],8);
//          for(uint32_t chirpNow = 0 ;chirpNow<NUM_CHIRP;chirpNow++){
//              for(uint32_t antNow=0;antNow<NUM_ANT;antNow++){
//                      USART_Transmit_Bytes(&USART0,(uint8_t *)ADC_SAMPLE_ADDR+chirpNow*NUM_BIG_CHIRP_BYTES+antNow*USE_RANGE*NUM_BYTE32,ADC_MAX_LEN*ADC_BYTE);
//              }

//          }
//          for(uint32_t i = 0 ;i<256*1024;i++){
//              abuf32[i]=i;
//          }
//          for(uint32_t i=0;i<16;i++){
//              OSPI_Trans64((uint64_t *)(ADC_SAMPLE_ADDR),1048576);
////                ospiTx((uint64_t *)(ADC_SAMPLE_ADDR));
////                Cnt_Delay_ms(500);
//          }
//          for(uint32_t i=0; i<1024*256; i++){
//                  abuf32[i] = i;
//          }
//          for(uint32_t i=0;i<16;i++){
//              OSPI_Trans64((uint64_t *)(ADC_SAMPLE_ADDR),1048576);
//          }
//          OSPI_Trans64((uint64_t *)(ADC_SAMPLE_ADDR),1048576);
        USART_Transmit_Bytes(&USART0, (uint8_t *)&head[0], 8);
		// USART_Transmit_Bytes(&USART0, (uint8_t *)ADC_SAMPLE_ADDR, 1048576);
		// 输出缓存起始位置的512个采样，即1024字节
				USART_Transmit_Bytes(
						&USART0,
						(uint8_t *)ADC_SAMPLE_ADDR,
						ADC_MAX_LEN * ADC_BYTE
				);
//				for (uint32_t offset = 0; offset < total; )
//				{
//						uint32_t len = total - offset;

//						if (len > 1024)
//								len = 1024;

//						WDT_CLEAR_COUNTER;

//						USART_Transmit_Bytes(
//								&USART0,
//								(uint8_t *)ADC_SAMPLE_ADDR + offset,
//								len
//						);

//						offset += len;
//				}
//		USART_Transmit_Bytes(&USART0, (uint8_t *)ADC_SAMPLE_ADDR, ADC_MAX_LEN * ADC_BYTE);
        Cnt_Delay_ms(200);
//          printf("over\n");
//          while(1);
        scmd = 0;
        break;
    }
}

/**********************************FUNC*****************************************/
