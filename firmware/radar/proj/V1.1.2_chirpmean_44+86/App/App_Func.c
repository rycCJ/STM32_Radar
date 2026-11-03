#include "Radar_Internal.h"
#include "Capture_Output.h"
void Func02_Handle(void)
{
    uint64_t sDMA_Cfg2 = (uint64_t)ADC_BYTE << BB_BIT32_POS | (uint64_t)(ADC_SAMPLE_ADDR); //srcIntraInc,srcBaseAddr
    uint64_t sFFT_Cfg2 = (uint64_t)NUM_BYTE32 << BB_BIT32_POS | (uint64_t)FFT1D_CACHE_ADDR;
    uint64_t sP2_Cfg2 = 0;
    uint64_t sP2_Cfg4 = 0;
    uint32_t sAnt_Cnt = 0;
    uint32_t sChirp_Cnt = 0;
    uint32_t sQuarter_Cnt = 0;
    uint32_t sDPK_Cfg0 = 0;
    uint64_t rtc_current = 0;
    static uint8_t scmd = 0;
		static float realSUM[RANGE_BIN];
		static float imageSUM[RANGE_BIN];

    switch (scmd) {
    case 0:
#if 1
//              TEST_WritePin = 1;
//              firstChirpCnt = 0;
        rampCnt = 0;
        bb_regb_str->PREP_CFG0 = bb_regb_str->PREP_CFG0 & ((uint64_t)0xFF000FFFFFFFFFFF);
        bb_regb_str->PREP_CFG0 = bb_regb_str->PREP_CFG0 | ((uint64_t)NUM_CHIRP1_TX << BB_BIT44_POS);
        BB_OPCLEAR(BB_CLEAR_DMA);
        //twiceMode,minmaxMode,flt2pf_lsf_en,flt2pf_sfb,jumpCnt,interCnt,intraCnt,wmode,dsize
        BB_DMA_Cfg0(DMA_CFG0_TWICEMODE_NORMAL, DMA_CFG0_MINMAX_MODE_ABS, DMA_CFG0_FLT2PF_LSF_EN_RIGHT, 0, 0, 0, ADC_MAX_LEN - 1, DMA_CFG0_WMODE_ORI, ADC_BYTE - 1); //jumpCnt,interCnt,intraCnt,fix2pf_sel,dsize
        BB_DMA_Cfg1(0, 0); //srcJumpInc,srcInterInc
        BB_DMA_Cfg3(0, 0); //dstJumpInc,dstInterInc
        BB_DMA_Cfg4(0, BB_ZO_BASE); //dstIntraInc,dstBaseAddr
        BB_FFT_Cfg0(0, 0, FFT1D_USE_A, FFT1D_USE_B, ADC_MAX_LEN - 1, FFTPT_512, FFT_UNLOAD_EN, FFT_MODE_FORWARD); //rsfOutput,rsfInput,useA,useB,inPt,fftPt,unloadEn,mode
        BB_FFT_ZP_Clear();
//              bb_regb_str->FFT_SRC_ZP00=0xFFFFFFFFFFFFFFFFULL;
//              bb_regb_str->FFT_SRC_ZP01=0xFFFFFFFFFFFFFFFFULL;
//              bb_regb_str->FFT_SRC_ZP02=0x0ULL;
//              bb_regb_str->FFT_SRC_ZP03=0x0ULL;
//              bb_regb_str->FFT_SRC_ZP04=0x0ULL;
//              bb_regb_str->FFT_SRC_ZP05=0x0ULL;
//              bb_regb_str->FFT_SRC_ZP06=0x0ULL;
//              bb_regb_str->FFT_SRC_ZP07=0x0ULL;
        BB_FFT_Cfg1(WIN_RANGE_ADDR, FFT_CFG1_OUTLSFEN_RIGHT, 0, FFT_CFG1_INLSFEN_RIGHT, 0, FFT_CFG1_WIN_SIZE_14BIT, FFT_CFG1_WIN_EN); //winBaseAddr,winSize,winEn
        BB_FFT_CMD0(FFT_CMD0_CLEAR_IBUF);
        RF_SPI_Init();
        BB_ISR_CLEAR(BB_ISR_PREP_RAMP_END | BB_ISR_PREP_QUARTER_END | BB_ISR_PREP_FRAME_END);
        BB_OPGATE_EN(BB_GATE_PREP | BB_GATE_P1MUL | BB_GATE_DMA | BB_GATE_FFT);
        BB_OPCLEAR(BB_CLEAR_PREP);
        Capture_Begin();
        waveGenStart();
//              while( (bb_regb_str->ISR & (BB_ISR_PREP_FRAME_END))==0);
#endif
#if  0//Timing Data  no use
        bb_regb_str->ISR = BB_ISR_PREP_FRAME_END;
        RF_CHIPEN_Disable();
        BB_OPGATE_DIS(BB_GATE_PREP | BB_GATE_P1MUL | BB_GATE_DMA | BB_GATE_FFT);
        BB_OPCLEAR(BB_CLEAR_PREP);

        USART_Transmit_Bytes(&USART0, (uint8_t *)&head[0], 8);
        for (uint32_t chirpNow = 0 ; chirpNow < NUM_CHIRP; chirpNow++) {
            for (uint32_t antNow = 0; antNow < NUM_ANT; antNow++) {
                USART_Transmit_Bytes(&USART0, (uint8_t *)ADC_SAMPLE_ADDR + chirpNow * NUM_BIG_CHIRP_BYTES + antNow * NUM_LITTLE_CHIRP_BYTES, ADC_MAX_LEN * ADC_BYTE);
            }
        }
        Cnt_Delay_ms(100);
#endif
#if 1
//              USART_Transmit_Bytes(&USART0,(uint8_t *)&head[0],8);
//              USART_Transmit_Bytes(&USART0,(uint8_t *)ADC_SAMPLE_ADDR,ADC_MAX_LEN*NUM_ANT*ADC_BYTE);
//              USART_Transmit_Bytes(&USART0,(uint8_t *)FFT1D_CACHE_ADDR,USE_RANGE*NUM_ANT*NUM_BYTE32);

#if 1   //use
        /********************************************ZO+P1MUL+DMA+FFT1D******************************************************/
//              TEST_WritePin = 0;
        for (uint32_t quarterNow = 0; quarterNow < 4; quarterNow++) {
            while ( (bb_regb_str->ISR & (BB_ISR_PREP_QUARTER_END | BB_ISR_PREP_FRAME_END)) == 0 );
            bb_regb_str->ISR = BB_ISR_PREP_QUARTER_END;
            for (uint32_t antNow = 0; antNow < NUM_ANT; antNow++) {
                bb_regb_str->P1MUL_CFG0 =   (uint64_t)P1MUL_CFG0_NCOMODE_COS_SIN << BB_BIT56_POS | ((uint64_t)nco_fcw[antNow] << BB_BIT32_POS) | (uint64_t)calib_ap[antNow];
                for (uint32_t chirpNow = 0; chirpNow < NUM_CHIRP_QUARTER; chirpNow++) {
                    bb_regb_str->DMA_CFG2   = sDMA_Cfg2;
                    bb_regb_str->FFT_CFG2   = sFFT_Cfg2;
                    BB_OPCLEAR(BB_CLEAR_P1MUL);
                    BB_ZO_Cfg0(BB_ZO_ZLEN_6, 7, BB_ZO_MODE_2, 0xFFFF); //zLen,mulFac,mode,thresSum
                    BB_FFT_CMD0(FFT_CMD0_CLEAR_IBUF);
                    BB_ISR_CLEAR(BB_ISR_FFT_UNLOAD_END);
                    BB_OPTRIG(BB_TRIG_DMA);
                    sChirp_Cnt++;
                    if (sChirp_Cnt == NUM_CHIRP_QUARTER) {
                        sChirp_Cnt = 0;
                        sAnt_Cnt++;
                        if (sAnt_Cnt == NUM_ANT) {
                            sAnt_Cnt = 0;
                            sQuarter_Cnt++;
                        }
                    }
                    sDMA_Cfg2 = (uint64_t)ADC_BYTE << BB_BIT32_POS | (uint64_t)(ADC_SAMPLE_ADDR + sAnt_Cnt * NUM_LITTLE_CHIRP_BYTES + sChirp_Cnt * NUM_BIG_CHIRP_BYTES + sQuarter_Cnt * NUM_CHIRP_QUARTER * NUM_BIG_CHIRP_BYTES); //srcIntraInc,srcBaseAddr
                    sFFT_Cfg2 = (uint64_t)NUM_BYTE32 << BB_BIT32_POS | (uint64_t)(FFT1D_CACHE_ADDR + sAnt_Cnt * USE_RANGE * NUM_BYTE32 + sChirp_Cnt * NUM_BIG_CHIRP_BYTES + sQuarter_Cnt * NUM_CHIRP_QUARTER * NUM_BIG_CHIRP_BYTES); //dstIntraInc,dstBaseAddr
                    BB_waitISR(BB_ISR_FFT_UNLOAD_END);
//                                  if(bb_regb_str->DMA_STA0==0xfffffe7300000000){

//                                      printf("%08X,%08X\n",(uint32_t)(bb_regb_str->ESR>>32),(uint32_t)(bb_regb_str->ESR));
//                                      while(1);
//                                  }
                }
            }
        }
        sAnt_Cnt = 0;
        sChirp_Cnt = 0;
        bb_regb_str->ISR = BB_ISR_PREP_FRAME_END;
        RF_CHIPEN_Disable();
        BB_OPGATE_DIS(BB_GATE_PREP | BB_GATE_P1MUL | BB_GATE_DMA | BB_GATE_FFT);
        BB_OPCLEAR(BB_CLEAR_PREP);
#if 1 //FFT1D
				
					float *realPart = (float *)(FFT1D_CACHE_ADDR + 0x800000);
					float *imagePart = (float *)(FFT1D_CACHE_ADDR + 0x800000 + 4);

					float tempR = 0, tempI = 0;
					memset(realSUM, 0 ,  sizeof(realSUM));
					memset(imageSUM, 0 ,  sizeof(imageSUM));
					int j = 2; // chirp max:0-63
					int m = 6;

					for(uint16_t i = 0; i < RANGE_BIN; i++)
					{
					realSUM[i] = realPart[2 * (i + USE_RANGE * NUM_RX * NUM_TX * j + m * USE_RANGE)];
					imageSUM[i] = imagePart[2 * (i + USE_RANGE * NUM_RX * NUM_TX * j + m * USE_RANGE)];
					}
		
			
#endif
#endif


				
//          TEST_WritePin = 0;
#if 0   //P1MUL          no use
        BB_DMA_Cfg0(DMA_CFG0_MINMAX_MODE_ABS, DMA_CFG0_FLT2PF_LSF_EN_RIGHT, 0, 0, 0, ADC_MAX_LEN - 1, DMA_CFG0_WMODE_ORI, ADC_BYTE - 1); //jumpCnt,interCnt,intraCnt,fix2pf_sel,dsize
        BB_DMA_Cfg1(0, 0); //srcJumpInc,srcInterInc
        BB_DMA_Cfg3(0, 0); //dstJumpInc,dstInterInc
        BB_DMA_Cfg4(0, BB_ZO_BASE); //dstIntraInc,dstBaseAddr
        BB_OPGATE_EN(BB_GATE_DMA | BB_GATE_P1MUL);
        for (uint32_t antNow = 0; antNow < NUM_ANT; antNow++) {
            BB_DMA_Cfg2(ADC_BYTE, ADC_SAMPLE_ADDR + antNow * ADC_MAX_LEN * ADC_BYTE); //srcIntraInc,srcBaseAddr
            BB_P1MUL_Cfg0(P1MUL_CFG0_NCOMODE_COS_SIN, nco_fcw[antNow], calib_ap[antNow]); // nco ap
            BB_P1MUL_Cfg1(NUM_BYTE32, P1MUL_CACHE_ADDR + antNow * ADC_MAX_LEN * NUM_BYTE32); // dstIntraInc,dstBaseAddr
            BB_OPCLEAR(BB_CLEAR_P1MUL);
            BB_ISR_CLEAR(BB_ISR_P1MUL_END);
            BB_OPTRIG(BB_TRIG_DMA);
            BB_waitISR(BB_ISR_P1MUL_END);
        }
        USART_Transmit_Bytes(&USART0, (uint8_t *)P1MUL_CACHE_ADDR, ADC_MAX_LEN * NUM_ANT * NUM_BYTE32);
        BB_OPGATE_DIS(BB_GATE_DMA | BB_GATE_P1MUL);
#endif
        /********************************************P2 mean******************************************************/
#if 1
//                  memset((uint8_t *)FFT1D_MEAN_ADDR,0,10*1024);
        BB_OPCLEAR(BB_CLEAR_P2);
        BB_OPGATE_EN(BB_GATE_P2);
        BB_P2_CFG0(NUM_ANT - 1, USE_RANGE - 1, NUM_CHIRP - 1, P2_CFG0_DIV64, P2_CFG0_SUBMODE_CPX_SUM_MUL, P2_CFG0_MODE_ACC); //jumpCnt,interCnt,intraCnt,rsfBit,sub_mode,mode
        BB_P2_CFG1(USE_RANGE * NUM_BYTE32, NUM_BYTE32); //src0JumpInc,src0InterInc
        BB_P2_CFG2(NUM_BIG_CHIRP_BYTES, FFT1D_CACHE_ADDR); //src0IntraInc,src0BaseAddr
        BB_P2_CFG3(0, 0); //src1JumpInc,src1InterInc
        BB_P2_CFG4(0, 0); //src1IntraInc,src1BaseAddr
        BB_P2_CFG5(USE_RANGE * NUM_BYTE32, NUM_BYTE32); //dstJumpInc,dstInterInc
        BB_P2_CFG6(0, FFT1D_MEAN_ADDR); //dstIntraInc,dstBaseAddr
        BB_ISR_CLEAR(BB_ISR_P2_JUMP_END);
        BB_OPTRIG(BB_TRIG_P2);
        BB_waitISR(BB_ISR_P2_JUMP_END);
        BB_OPGATE_DIS(BB_GATE_P2);

        if (r2_str.meanEn == 2)
        {
            memcpy((uint8_t *)(SRAM2_BASE), (uint8_t *)(FFT1D_MEAN_ADDR), USE_RANGE * NUM_ANT * NUM_BYTE32); //save tmp
            memcpy((uint8_t *)(FFT1D_MEAN_ADDR), (uint8_t *)(SRAM1_BASE), USE_RANGE * NUM_ANT * NUM_BYTE32); //recover old fft1d mean
            memcpy((uint8_t *)(SRAM1_BASE), (uint8_t *)(SRAM2_BASE), USE_RANGE * NUM_ANT * NUM_BYTE32);   //backup fft1d mean
        }

#if 0
        USART_Transmit_Bytes(&USART0, (uint8_t *)FFT1D_MEAN_ADDR, USE_RANGE * NUM_ANT * NUM_BYTE32);
#endif
#endif

        if (r2_str.meanEn == 0) {
            Write_M32(FFT1D_MEAN_ADDR, 0);
        }
        /************s*********************************P2 minus+FFT2DANT ALL*********************************************/
#if 1
        BUF_CFG0(BUF_CFG0_WR_FLT2PF_LSF_EN_RIGHT, 0, BUF_CFG0_CONVERT_CPX_MODE_FIX, BUF_CFG0_RD_POWMODE_ABS);
        BB_P2_CFG0(0, 0, NUM_CHIRP - 1, P2_CFG0_DIV1, P2_CFG0_SUBMODE_CPX_MINUS, P2_CFG0_MODE_ADD); //jumpCnt,interCnt,intraCnt,rsfBit,sub_mode,mode
        BB_P2_CFG1(0, 0); //src0JumpInc,src0InterInc
        BB_P2_CFG3(0, 0); //src1JumpInc,src1InterInc
        BB_P2_CFG5(0, 0); //dstJumpInc,dstInterInc
        BB_P2_CFG6(0, BB_FFT_BASE); //dstIntraInc,dstBaseAddr
        BB_FFT_Cfg0(0, 0, FFT2D_USE_A, FFT2D_USE_B, NUM_CHIRP - 1, FFTPT_256, FFT_UNLOAD_EN, FFT_MODE_FORWARD); //rsfOutput,rsfInput,useA,useB,inPt,fftPt,unloadEn,mode   //useA>useB
        BB_FFT_ZP_Clear();
        BB_FFT_Cfg1(WIN_VEL1_ADDR, FFT_CFG1_OUTLSFEN_RIGHT, 0, FFT_CFG1_INLSFEN_RIGHT, 0, FFT_CFG1_WIN_SIZE_14BIT, FFT_CFG1_WIN_EN); //winBaseAddr,winSize,winEn
        for (uint8_t txNum = 0; txNum < NUM_TX; txNum++)
        {
            bb_regb_str->FFT_SRC_ZP00 = fft2d_zp.txNum_zp0[txNum];
            bb_regb_str->FFT_SRC_ZP01 = fft2d_zp.txNum_zp1[txNum];
            bb_regb_str->FFT_SRC_ZP02 = fft2d_zp.txNum_zp2[txNum];
            bb_regb_str->FFT_SRC_ZP03 = fft2d_zp.txNum_zp3[txNum];
            for (uint8_t rxNum = 0; rxNum < NUM_RX; rxNum++)
            {
                BB_FFT_CMD0(FFT_CMD0_CLEAR_IBUF);
                BB_OPCLEAR(BB_CLEAR_FFT | BB_CLEAR_P2);
                BB_OPGATE_EN(BB_GATE_P2 | BB_GATE_FFT);
                sP2_Cfg2 = (uint64_t)NUM_BIG_CHIRP_BYTES << BB_BIT32_POS | (FFT1D_CACHE_ADDR + USE_RANGE * (txNum * NUM_RX + rxNum) * NUM_BYTE32); //src0IntraInc,src0BaseAddr
                sFFT_Cfg2 = (uint64_t)NUM_BIG_CHIRP_BYTES << BB_BIT32_POS | (FFT2D_CACHE_ADDR + USE_RANGE * (txNum * NUM_RX + rxNum) * NUM_BYTE32);
                if (r2_str.meanEn == 0) {
                    Write_M32(FFT1D_MEAN_ADDR, 0);
                    sP2_Cfg4 = (uint64_t)(FFT1D_MEAN_ADDR); //src1IntraInc,src1BaseAddr
                    for (uint32_t rangeNow = 1; rangeNow <= USE_RANGE; rangeNow++) {
                        bb_regb_str->P2_CFG2 = sP2_Cfg2;
                        bb_regb_str->P2_CFG4 = sP2_Cfg4;
                        bb_regb_str->FFT_CFG2 = sFFT_Cfg2;

                        BB_FFT_CMD0(FFT_CMD0_CLEAR_IBUF);
                        BB_ISR_CLEAR(BB_ISR_FFT_UNLOAD_END);
                        BB_OPTRIG(BB_TRIG_P2);
                        sP2_Cfg2 = ((uint64_t)NUM_BIG_CHIRP_BYTES) << BB_BIT32_POS | (FFT1D_CACHE_ADDR + USE_RANGE * (txNum * NUM_RX + rxNum) * NUM_BYTE32 + rangeNow * NUM_BYTE32); //src0IntraInc,src0BaseAddr
//                      sP2_Cfg4 =(uint64_t)FFT1D_MEAN_ADDR+USE_RANGE*NUM_BYTE32*txNum*NUM_RX+rxNum+rangeNow*NUM_BYTE32;  //src1IntraInc,src1BaseAddr
                        sFFT_Cfg2 = ((uint64_t)NUM_BIG_CHIRP_BYTES) << BB_BIT32_POS | (FFT2D_CACHE_ADDR + USE_RANGE * (txNum * NUM_RX + rxNum) * NUM_BYTE32 + rangeNow * NUM_BYTE32); //dstIntraInc,dstBaseAddr
                        BB_waitISR(BB_ISR_FFT_UNLOAD_END);
                    }
                } else {
                    sP2_Cfg4 = (uint64_t)(FFT1D_MEAN_ADDR + USE_RANGE * NUM_BYTE32 * (txNum * NUM_RX + rxNum)); //src1IntraInc,src1BaseAddr
                    for (uint32_t rangeNow = 1; rangeNow <= USE_RANGE; rangeNow++) {
                        bb_regb_str->P2_CFG2 = sP2_Cfg2;
                        bb_regb_str->P2_CFG4 = sP2_Cfg4;
                        bb_regb_str->FFT_CFG2 = sFFT_Cfg2;

                        BB_FFT_CMD0(FFT_CMD0_CLEAR_IBUF);
                        BB_ISR_CLEAR(BB_ISR_FFT_UNLOAD_END);
                        BB_OPTRIG(BB_TRIG_P2);
                        sP2_Cfg2 = ((uint64_t)NUM_BIG_CHIRP_BYTES) << BB_BIT32_POS | (FFT1D_CACHE_ADDR + USE_RANGE * (txNum * NUM_RX + rxNum) * NUM_BYTE32 + rangeNow * NUM_BYTE32); //src0IntraInc,src0BaseAddr
                        sP2_Cfg4 = (uint64_t)FFT1D_MEAN_ADDR + USE_RANGE * NUM_BYTE32 * (txNum * NUM_RX + rxNum) + rangeNow * NUM_BYTE32; //src1IntraInc,src1BaseAddr
                        sFFT_Cfg2 = ((uint64_t)NUM_BIG_CHIRP_BYTES) << BB_BIT32_POS | (FFT2D_CACHE_ADDR + USE_RANGE * (txNum * NUM_RX + rxNum) * NUM_BYTE32 + rangeNow * NUM_BYTE32); //dstIntraInc,dstBaseAddr
                        BB_waitISR(BB_ISR_FFT_UNLOAD_END);
                    }
                }
            }
        }
        BB_OPGATE_DIS(BB_GATE_P2 | BB_GATE_FFT);

#if 0
        USART_Transmit_Bytes(&USART0, (uint8_t *)&head[0], 8);
//        for (uint32_t chirpNow = 0; chirpNow < NUM_CHIRP; chirpNow++) {
//            USART_Transmit_Bytes(&USART0, (uint8_t *)FFT2D_CACHE_ADDR + USE_RANGE * CFAR_ANT * NUM_BYTE32 + chirpNow * NUM_BIG_CHIRP_BYTES, USE_RANGE * NUM_BYTE32);
//        }
//
        for (uint32_t chirpNow = 0; chirpNow < NUM_CHIRP; chirpNow++) {
            USART_Transmit_Bytes(&USART0, (uint8_t *)FFT2D_CACHE_ADDR + chirpNow * NUM_BIG_CHIRP_BYTES, USE_RANGE * NUM_ANT * NUM_BYTE32);
        }
// ABS
        for (uint32_t chirpNow = 0; chirpNow < NUM_CHIRP; chirpNow++) {
            USART_Transmit_Bytes(&USART0, (uint8_t *)FFT2D_CACHE_ADDR + 0x400000 + chirpNow * NUM_BIG_CHIRP_BYTES, USE_RANGE * NUM_ANT * NUM_BYTE32);
        }
        while (1);
#endif
#if 0 //abs
        USART_Transmit_Bytes(&USART0, (uint8_t *)&head[0], 8);
        for (uint32_t chirpNow = 0; chirpNow < NUM_VEL; chirpNow++) {
            USART_Transmit_Bytes(&USART0, (uint8_t *)(FFT2D_CACHE_ADDR + BB_ABS_OFFSET + chirpNow * NUM_BIG_CHIRP_BYTES + 10 * NUM_BYTE32), 30 * NUM_BYTE32);
        }
#endif
#endif
        /********************************************sum fft2d******************************************************/
        memset((uint8_t *)FFT2D_ABS_SUM_ADDR, 0, USE_RANGE * NUM_BYTE32 * NUM_CHIRP);
#if 1
        Write_M32(ZEROS_ADDR, 0);
        //imag=0
        BB_DMA_Cfg0(DMA_CFG0_TWICEMODE_NORMAL, DMA_CFG0_MINMAX_MODE_ABS, DMA_CFG0_FLT2PF_LSF_EN_RIGHT, 0, 0, NUM_CHIRP - 1, USE_RANGE - 1, DMA_CFG0_WMODE_ORI, DMA_CFG0_DSIZE_32BIT); //jumpCnt,interCnt,intraCnt,fix2pf_sel,dsize
        BB_DMA_Cfg1(0, 0); //srcJumpInc,srcInterInc
        BB_DMA_Cfg2(0, ZEROS_ADDR); //srcIntraInc,srcBaseAddr
        BB_DMA_Cfg3(0, USE_RANGE * NUM_BYTE64); //dstJumpInc,dstInterInc
        BB_DMA_Cfg4(NUM_BYTE64, FFT2D_ABS_SUM_ADDR + NUM_BYTE32); //dstIntraInc,dstBaseAddr
        BB_OPCLEAR(BB_CLEAR_DMA);
        BB_ISR_CLEAR_ALL;
        BB_OPGATE_EN(BB_GATE_DMA);
        BB_OPTRIG(BB_TRIG_DMA);
        BB_waitISR(BB_ISR_DMA_JUMP_END);
        BB_OPGATE_DIS(BB_GATE_DMA);
#if 0
        USART_Transmit_Bytes(&USART0, (uint8_t *)FFT2D_ABS_SUM_ADDR, USE_RANGE * NUM_CHIRP * NUM_BYTE64);
        while (1);
#endif
        //sum
        BB_OPCLEAR(BB_CLEAR_P2);
        BB_OPGATE_EN(BB_GATE_P2);
        BB_P2_CFG0(NUM_CHIRP - 1, USE_RANGE - 1, NUM_ANT - 1, P2_CFG0_DIV128, P2_CFG0_SUBMODE_ABS_SUM, P2_CFG0_MODE_ACC); //jumpCnt,interCnt,intraCnt,rsfBit,sub_mode,mode
        BB_P2_CFG1(USE_RANGE * NUM_ANT * NUM_BYTE32, NUM_BYTE32); //src0JumpInc,src0InterInc
        BB_P2_CFG2(USE_RANGE * NUM_BYTE32, FFT2D_CACHE_ADDR); //src0IntraInc,src0BaseAddr
        BB_P2_CFG3(0, 0); //src1JumpInc,src1InterInc
        BB_P2_CFG4(0, 0); //src1IntraInc,src1BaseAddr
        BB_P2_CFG5(USE_RANGE * NUM_BYTE64, NUM_BYTE64); //dstJumpInc,dstInterInc
        BB_P2_CFG6(0, FFT2D_ABS_SUM_ADDR); //dstIntraInc,dstBaseAddr
        BB_ISR_CLEAR_ALL;
        BB_OPTRIG(BB_TRIG_P2);
        BB_waitISR(BB_ISR_P2_JUMP_END);
        BB_OPGATE_DIS(BB_GATE_P2);
#if 0
        USART_Transmit_Bytes(&USART0, (uint8_t *)&head[0], 8);
        USART_Transmit_Bytes(&USART0, (uint8_t *)FFT2D_ABS_SUM_ADDR, USE_RANGE * NUM_BYTE64 * NUM_CHIRP);
        while (1);
#endif
        //change pf
        DMA_Start_TransferM2M_SRAM2K(FFT2D_ABS_SUM_ADDR, FFT2D_PF_SUM_CPX_ADDR, USE_RANGE * NUM_BYTE64 * NUM_CHIRP, DMA_CHANNEL0);
#if 0
        USART_Transmit_Bytes(&USART0, (uint8_t *)&head[0], 8);
        USART_Transmit_Bytes(&USART0, (uint8_t *)FFT2D_PF_SUM_ADDR, USE_RANGE * NUM_BYTE32 * NUM_CHIRP);
        while (1);
#endif
#endif
        /********************************************CFAR******************************************************/

#if 1 //ABUF3->CFAR->ABUF4
        BUF_CFG0(BUF_CFG0_WR_FLT2PF_LSF_EN_RIGHT, 0, BUF_CFG0_CONVERT_CPX_MODE_FLOAT, BUF_CFG0_RD_POWMODE_ABS);
//              printf("cfarPre:");
//              for(uint32_t chirpNow=0;chirpNow<NUM_CHIRP;chirpNow++){
//                      for(uint32_t rangeNow=0;rangeNow<USE_RANGE;rangeNow++){
//                          printf("0x%08X,",FFT2D_Buf[rangeNow+USE_RANGE*CFAR_ANT+NUM_BIG_CHIRP_BYTES/NUM_BYTE32*chirpNow]);
//                      }
//              }
//              memset((uint8_t *)CFAR_CACHE_ADDR,0,128*1024);
        memcpy((uint8_t *)(BB_DBUF0_BASE), (uint8_t *)(FFT2D_PF_SUM_ADDR), USE_RANGE * NUM_BYTE32 * NUM_CHIRP);
        BB_CFAR_Vel_Init();
        BB_OPCLEAR(BB_CLEAR_CFAR);
        BB_OPGATE_EN(BB_GATE_CFAR); //val,sw
        BB_ISR_CLEAR(BB_ISR_CFAR_END);
        BB_OPTRIG(BB_TRIG_CFAR); //val
        BB_waitISR(BB_ISR_CFAR_END);
        cfarTargetNum = bb_regb_str->CFAR_STA0;
        BB_OPGATE_DIS(BB_GATE_CFAR); //val,sw
#if 0
        USART_Transmit_Bytes(&USART0, (uint8_t *)CFAR_CACHE_ADDR, cfarTargetNum * NUM_BYTE64);
#endif
#endif

        /********************************************other ANT FFT2D******************************************************/
#if 0  //ABUF2->FFT2D->ABUF3
//              memset((uint8_t *)FFT2D_CACHE_ADDR,0,USE_RANGE*NUM_CHIRP*NUM_BYTE32);
        BB_P2_CFG0(0, 0, NUM_CHIRP - 1, P2_CFG0_DIV1, P2_CFG0_SUBMODE_CPX_MINUS, P2_CFG0_MODE_ADD); //jumpCnt,interCnt,intraCnt,rsfBit,sub_mode,mode
        BB_P2_CFG1(0, 0); //src0JumpInc,src0InterInc
        BB_P2_CFG3(0, 0); //src1JumpInc,src1InterInc
        BB_P2_CFG5(0, 0); //dstJumpInc,dstInterInc
        BB_P2_CFG6(0, BB_FFT_BASE); //dstIntraInc,dstBaseAddr
        BB_FFT_Cfg0(0, 0, FFT2D_USE_A, FFT2D_USE_B, NUM_CHIRP - 1, FFTPT_256, FFT_UNLOAD_EN, FFT_MODE_FORWARD); //rsfOutput,rsfInput,useA,useB,inPt,fftPt,unloadEn,mode   //useA>useB
        BB_FFT_ZP_Clear();

        BB_OPCLEAR(BB_CLEAR_FFT | BB_CLEAR_P2);
        BB_OPGATE_EN(BB_GATE_P2 | BB_GATE_FFT);
        if (r2_str.meanEn == 0) {
            bb_regb_str->P2_CFG4 = FFT1D_MEAN_ADDR;
            for (uint32_t txNow = 0; txNow < NUM_TX; txNow++) {
                for (uint32_t rxNow = 0; rxNow < NUM_RX; rxNow++) {
                    uint32_t antNow = txNow * NUM_RX + rxNow;
                    uint16_t d1Cnt = 0;
                    if (antNow == CFAR_ANT)
                        continue;
                    BB_FFT_Cfg1(WIN_VEL1_ADDR + NUM_CHIRP * NUM_BYTE16 * txNow, FFT_CFG1_OUTLSFEN_RIGHT, 0, FFT_CFG1_INLSFEN_RIGHT, 0, FFT_CFG1_WIN_SIZE_14BIT, FFT_CFG1_WIN_EN); //winBaseAddr,winSize,winEn
                    bb_regb_str->FFT_SRC_ZP00 = fft2d_zp.txNum_zp0[txNow];
                    bb_regb_str->FFT_SRC_ZP01 = fft2d_zp.txNum_zp1[txNow];
                    bb_regb_str->FFT_SRC_ZP02 = fft2d_zp.txNum_zp2[txNow];
                    bb_regb_str->FFT_SRC_ZP03 = fft2d_zp.txNum_zp3[txNow];
                    for (uint32_t cfarNow = 0; cfarNow < cfarTargetNum; cfarNow++) {
                        if (d1Cnt <= bb_cfarRst_str[cfarNow].interIdx) {
                            bb_regb_str->P2_CFG2  = (uint64_t)NUM_BIG_CHIRP_BYTES << BB_BIT32_POS | (FFT1D_CACHE_ADDR + bb_cfarRst_str[cfarNow].interIdx * NUM_BYTE32 + USE_RANGE * NUM_BYTE32 * antNow);
//                                  bb_regb_str->P2_CFG4  =(uint64_t)FFT1D_MEAN_ADDR+bb_cfarRst_str[cfarNow].interIdx*NUM_BYTE32+USE_RANGE*NUM_BYTE32*antNow;
                            bb_regb_str->FFT_CFG2 = (uint64_t)NUM_BIG_CHIRP_BYTES << BB_BIT32_POS | (FFT2D_CACHE_ADDR + bb_cfarRst_str[cfarNow].interIdx * NUM_BYTE32 + USE_RANGE * NUM_BYTE32 * antNow);
                            BB_FFT_CMD0(FFT_CMD0_CLEAR_IBUF);
                            BB_ISR_CLEAR(BB_ISR_FFT_UNLOAD_END);
                            BB_OPTRIG(BB_TRIG_P2);
                            BB_waitISR(BB_ISR_FFT_UNLOAD_END);
                            d1Cnt = bb_cfarRst_str[cfarNow].interIdx + 1;                       //等fft做完，否则占用端口
                        }
                    }
                }
            }
        } else {
            for (uint32_t txNow = 0; txNow < NUM_TX; txNow++) {
                for (uint32_t rxNow = 0; rxNow < NUM_RX; rxNow++) {
                    uint32_t antNow = txNow * NUM_RX + rxNow;
                    uint16_t d1Cnt = 0;
                    if (antNow == CFAR_ANT)
                        continue;
                    BB_FFT_Cfg1(WIN_VEL1_ADDR + NUM_CHIRP * NUM_BYTE16 * txNow, FFT_CFG1_OUTLSFEN_RIGHT, 0, FFT_CFG1_INLSFEN_RIGHT, 0, FFT_CFG1_WIN_SIZE_14BIT, FFT_CFG1_WIN_EN); //winBaseAddr,winSize,winEn
                    bb_regb_str->FFT_SRC_ZP00 = fft2d_zp.txNum_zp0[txNow];
                    bb_regb_str->FFT_SRC_ZP01 = fft2d_zp.txNum_zp1[txNow];
                    bb_regb_str->FFT_SRC_ZP02 = fft2d_zp.txNum_zp2[txNow];
                    bb_regb_str->FFT_SRC_ZP03 = fft2d_zp.txNum_zp3[txNow];
                    for (uint32_t cfarNow = 0; cfarNow < cfarTargetNum; cfarNow++) {
                        if (d1Cnt <= bb_cfarRst_str[cfarNow].interIdx) {
                            bb_regb_str->P2_CFG2  = (uint64_t)NUM_BIG_CHIRP_BYTES << BB_BIT32_POS | (FFT1D_CACHE_ADDR + bb_cfarRst_str[cfarNow].interIdx * NUM_BYTE32 + USE_RANGE * NUM_BYTE32 * antNow);
                            bb_regb_str->P2_CFG4  = (uint64_t)FFT1D_MEAN_ADDR + bb_cfarRst_str[cfarNow].interIdx * NUM_BYTE32 + USE_RANGE * NUM_BYTE32 * antNow;
                            bb_regb_str->FFT_CFG2 = (uint64_t)NUM_BIG_CHIRP_BYTES << BB_BIT32_POS | (FFT2D_CACHE_ADDR + bb_cfarRst_str[cfarNow].interIdx * NUM_BYTE32 + USE_RANGE * NUM_BYTE32 * antNow);
                            BB_FFT_CMD0(FFT_CMD0_CLEAR_IBUF);
                            BB_ISR_CLEAR(BB_ISR_FFT_UNLOAD_END);
                            BB_OPTRIG(BB_TRIG_P2);
                            BB_waitISR(BB_ISR_FFT_UNLOAD_END);
                            d1Cnt = bb_cfarRst_str[cfarNow].interIdx + 1;                       //等fft做完，否则占用端口
                        }
                    }
                }
            }
        }
        BB_OPGATE_DIS(BB_GATE_P2 | BB_GATE_FFT);
#if 0
        for (uint32_t cfarNow = 0; cfarNow < cfarTargetNum; cfarNow++) {
            for (uint32_t antNow = 0; antNow < NUM_ANT; antNow++) {
                USART_Transmit_Bytes(&USART0, (uint8_t *)FFT2D_CACHE_ADDR + (bb_cfarRst_str[cfarNow].interIdx + antNow * USE_RANGE)*NUM_BYTE32 + bb_cfarRst_str[cfarNow].intraIdx * NUM_BIG_CHIRP_BYTES, NUM_BYTE32);
            }
        }
#endif
#endif
        /********************************************FFT3D,DPK******************************************************/

#if 1       /*FFT3D,DPK*/
//          memset((uint8_t *)FFT3D_CACHE_ADDR,0,NUM_ANGLE*NUM_BYTE32);
//          memset((uint8_t *)DPK_DST_FIRST_ADDR,0,24*4);
        BB_DMA_Cfg0(DMA_CFG0_TWICEMODE_NORMAL, DMA_CFG0_MINMAX_MODE_ABS, DMA_CFG0_FLT2PF_LSF_EN_RIGHT, 0, 0, 0, NUM_ANT_H - 1, DMA_CFG0_WMODE_ORI, DMA_CFG0_DSIZE_32BIT); //jumpCnt,interCnt,intraCnt,fix2pf_sel,dsize
        BB_DMA_Cfg1(0, 0); //srcJumpInc,srcInterInc
        BB_DMA_Cfg3(0, 0); //dstJumpInc,dstInterInc
        BB_DMA_Cfg4(0, BB_FFT_BASE); //dstIntraInc,dstBaseAddr
        BB_FFT_Cfg0(0, 0, FFT3D_USE_A, FFT3D_USE_B, NUM_ANT_H - 1, FFTPT_128, FFT_UNLOAD_EN, FFT_MODE_FORWARD); //rsfOutput,rsfInput,useA,useB,inPt,fftPt,unloadEn,mode   //useAuseB
        BB_FFT_ZP_Clear();
        bb_regb_str->FFT_SRC_ZP00 = (uint64_t)0xFF;
        bb_regb_str->FFT_SRC_ZP01 = (uint64_t)0;
//              bb_regb_str->FFT_SRC_ZP02 = (uint64_t)0;
//              bb_regb_str->FFT_SRC_ZP03 = (uint64_t)0;
        BB_FFT_Cfg1(WIN_RANGE_ADDR, FFT_CFG1_OUTLSFEN_RIGHT, 0, FFT_CFG1_INLSFEN_RIGHT, 0, FFT_CFG1_WIN_SIZE_14BIT, FFT_CFG1_WIN_DIS); //winBaseAddr,winSize,winEn
        BB_FFT_Cfg2(NUM_BYTE32, FFT3D_CACHE_ADDR); //dstIntraInc,dstBaseAddr
        BB_FFT_CMD0(FFT_CMD0_CLEAR_IBUF);

        BB_OPCLEAR(BB_GATE_DMA | BB_GATE_FFT | BB_GATE_DPK);
        BB_OPGATE_EN(BB_GATE_DMA | BB_GATE_FFT | BB_GATE_DPK);
        sDMA_Cfg2 = (uint64_t)(USE_RANGE * NUM_BYTE32) << BB_BIT32_POS | (FFT2D_CACHE_ADDR + bb_cfarRst_str[0].interIdx * NUM_BYTE32 + bb_cfarRst_str[0].intraIdx * NUM_BIG_CHIRP_BYTES + NUM_ANT_H * USE_RANGE * NUM_BYTE32);
        sDPK_Cfg0 = (uint64_t)(DPK_TIMES) << BB_BIT8_POS | (uint64_t)(DPK_LENLOG  ) << BB_BIT4_POS | (uint64_t)DPK_CFG0_MODE_MINUS;
#if 0  //RA FFT3D
        USART_Transmit_Bytes(&USART0, (uint8_t *)&head[0], 8);
        USART_Transmit_Bytes(&USART0, (uint8_t *)&cfarTargetNum, NUM_BYTE32);
#endif
        frameRst->targetNum = 0;
        for (uint32_t cfarNow = 0; cfarNow < cfarTargetNum; cfarNow++) {
            bb_regb_str->DMA_CFG2 = sDMA_Cfg2;
            BB_FFT_CMD0(FFT_CMD0_CLEAR_IBUF);
            BB_ISR_CLEAR(BB_ISR_FFT_UNLOAD_END);
            BB_OPTRIG(BB_TRIG_DMA);
            sDMA_Cfg2 = (uint64_t)(USE_RANGE * NUM_BYTE32) << BB_BIT32_POS | (FFT2D_CACHE_ADDR + bb_cfarRst_str[cfarNow + 1].interIdx * NUM_BYTE32 + bb_cfarRst_str[cfarNow + 1].intraIdx * NUM_BIG_CHIRP_BYTES + NUM_ANT_H * USE_RANGE * NUM_BYTE32);
            BB_waitISR(BB_ISR_FFT_UNLOAD_END);
#if 0  //cpx
            USART_Transmit_Bytes(&USART0, (uint8_t*)FFT3D_CACHE_ADDR, NUM_ANGLE * NUM_BYTE32);
#endif
#if 0  //abs FFT3D
            USART_Transmit_Bytes(&USART0, (uint8_t*)&bb_cfarRst_str[cfarNow].interIdx, NUM_BYTE16);
            USART_Transmit_Bytes(&USART0, (uint8_t*)&bb_cfarRst_str[cfarNow].intraIdx, NUM_BYTE16);
            USART_Transmit_Bytes(&USART0, (uint8_t*)FFT3D_CACHE_ADDR + BB_ABS_OFFSET, NUM_ANGLE * NUM_BYTE32);
#endif
            /*DPK*/
#if 1
            bb_dpkRst_str[0].cutPow = bb_regb_str->FFT_STA0 >> BB_BIT32_POS;
            bb_dpkRst_str[0].idx = bb_regb_str->FFT_STA0 & (uint64_t)0xFFF;
            bb_dpkRst_str[0].pFloat = FFT3D_Buf[BB_FFT_STA0_curMaxIdx];
            //                  BB_DPK_CFG0(bb_dpkRst_str[0].pFloat,bb_dpkRst_str[0].idx,DPK_TIMES,DPK_LENLOG,DPK_CFG0_MODE_MINUS);//preData,preIdx,dpkTime,lenBW,mode
            bb_regb_str->DPK_CFG0 = (uint64_t)(bb_dpkRst_str[0].pFloat) << BB_BIT32_POS | (uint64_t)(bb_dpkRst_str[0].idx ) << BB_BIT12_POS | sDPK_Cfg0;
            BB_OPCLEAR(BB_GATE_DPK);
            BB_ISR_CLEAR(BB_ISR_DPK_END);
            BB_OPTRIG(BB_TRIG_DPK);
            BB_waitISR(BB_ISR_DPK_END);
#ifdef PROTOCOL_DPK
            for (uint8_t dpkNow = 0; dpkNow < DPK_TIMES; dpkNow++) {
                if (bb_dpkRst_str[dpkNow].cutPow > bb_dpkRst_str[DPK_TIMES].res * r2_str.dpkThres) {

                    frameRst->target[frameRst->targetNum].d1Idx = bb_cfarRst_str[cfarNow].interIdx;
                    frameRst->target[frameRst->targetNum].d2Idx = bb_cfarRst_str[cfarNow].intraIdx;
                    frameRst->target[frameRst->targetNum].d3Idx = bb_dpkRst_str[dpkNow].idx;
                    frameRst->target[frameRst->targetNum].powABS = bb_dpkRst_str[dpkNow].cutPow;
                    frameRst->targetNum++;
//                              printf("%d\n",frameRst->targetNum);
                }
#if 0 //dpk output
                if (dpkNow == 0) {
                    bb_dpkRst_str[dpkNow].real = 0;
                    bb_dpkRst_str[dpkNow].imag = 0;
                }
                USART_Transmit_Bytes(&USART0, (uint8_t*)&bb_dpkRst_str[dpkNow].cutPow, 4);
                USART_Transmit_Bytes(&USART0, (uint8_t*)&bb_dpkRst_str[dpkNow].idx, 4);
                USART_Transmit_Bytes(&USART0, (uint8_t*)&bb_dpkRst_str[dpkNow].real, 4);
                USART_Transmit_Bytes(&USART0, (uint8_t*)&bb_dpkRst_str[dpkNow].imag, 4);
                USART_Transmit_Bytes(&USART0, (uint8_t*)&bb_dpkRst_str[dpkNow + 1].res, 4);
                USART_Transmit_Bytes(&USART0, (uint8_t*)&bb_dpkRst_str[dpkNow].pFloat, 4);

#endif
            }
#endif
#ifdef PROTOCOL_CFAR
            frameRst->target[frameRst->targetNum].d1Idx = bb_cfarRst_str[cfarNow].interIdx;
            frameRst->target[frameRst->targetNum].d2Idx = bb_cfarRst_str[cfarNow].intraIdx;
            frameRst->target[frameRst->targetNum].d3Idx = 0;
            frameRst->target[frameRst->targetNum].powABS = FFT2D_ABS_Buf[bb_cfarRst_str[cfarNow].interIdx + bb_cfarRst_str[cfarNow].intraIdx * NUM_BIG_CHIRP_BYTES / NUM_BYTE32];
            frameRst->targetNum++;
#endif

#endif
        }
        BB_OPGATE_DIS(BB_GATE_DMA | BB_GATE_FFT | BB_GATE_DPK);
#endif
        //ele[3]=FFT2D[d2Idx][16][d1Idx].*steeringVec[rx][d3Idx]
#if 1
#if 0 //printf

        printf("d1Idx=%d,d2Idx=%d,d3Idx=%d\n", frameRst->target[0].d1Idx, frameRst->target[0].d2Idx, frameRst->target[0].d3Idx);
        printf("FFT2D:\n");
        for (uint32_t antNow = 0; antNow < NUM_ANT_V * NUM_RX; antNow++) {
            printf("%08X ", FFT2D_Buf[frameRst->target[0].d1Idx + frameRst->target[0].d2Idx * USE_RANGE * NUM_ANT + antNow * USE_RANGE]);
        }
        printf("steeringVec:\n");
        for (uint32_t rxNow = 0; rxNow < NUM_RX; rxNow++) {
            printf("%08X ", steeringVec_Pf[frameRst->target[0].d3Idx + rxNow * NUM_ANGLE]);
        }
#endif
        BB_P2_CFG0(0, NUM_ANT_V - 1, NUM_RX - 1, P2_CFG0_DIV1, P2_CFG0_SUBMODE_CPX_SUM_MUL, P2_CFG0_MODE_MAC); //jumpCnt,interCnt,intraCnt,rsfBit,sub_mode,mode
        BB_P2_CFG1(0, NUM_RX * USE_RANGE * NUM_BYTE32); //src0JumpInc,src0InterInc
        BB_P2_CFG3(0, 0); //src1JumpInc,src1InterInc
        BB_P2_CFG5(0, 0); //dstJumpInc,dstInterInc
        BB_P2_CFG6(0, BB_FFT_BASE); //dstIntraInc,dstBaseAddr
//              BB_P2_CFG5(0,NUM_BYTE32); //dstJumpInc,dstInterInc
//              BB_P2_CFG6(0,FFT4D_PRE_ADDR); //dstIntraInc,dstBaseAddr
        BB_P2_CFG7(P2_CFG7_SRC0CONJ_DIS, P2_CFG7_SRC0LSF_RIGHT, 0,
                   P2_CFG7_SRC1CONJ_DIS, P2_CFG7_SRC1LSF_RIGHT, 0,
                   P2_CFG7_DSTCONJ_DIS, P2_CFG7_DSTLSF_RIGHT, 0);
        BB_FFT_Cfg0(0, 0, FFT4D_USE_A, FFT4D_USE_B, NUM_ANT_V - 1, FFTPT_128, FFT_UNLOAD_EN, FFT_MODE_FORWARD); //rsfOutput,rsfInput,useA,useB,inPt,fftPt,unloadEn,mode   //useA>useB
        BB_FFT_Cfg1(WIN_RANGE_ADDR, FFT_CFG1_OUTLSFEN_RIGHT, 0, FFT_CFG1_INLSFEN_RIGHT, 0, FFT_CFG1_WIN_SIZE_14BIT, FFT_CFG1_WIN_DIS); //winBaseAddr,winSize,winEn
        BB_FFT_Cfg2(NUM_BYTE32, FFT4D_CACHE_ADDR); //dstIntraInc,dstBaseAddr
        BB_FFT_ZP_Clear();
        bb_regb_str->FFT_SRC_ZP00 = (uint64_t)0x07;
        bb_regb_str->FFT_SRC_ZP01 = (uint64_t)0;
        BB_OPGATE_EN(BB_GATE_P2 | BB_GATE_FFT);
        for (uint32_t targetNow = 0; targetNow < frameRst->targetNum; targetNow++) {
            BB_P2_CFG2(USE_RANGE * NUM_BYTE32, FFT2D_CACHE_ADDR + frameRst->target[targetNow].d1Idx * NUM_BYTE32 + frameRst->target[targetNow].d2Idx * NUM_BIG_CHIRP_BYTES); //src0IntraInc,src0BaseAddr
            BB_P2_CFG4(NUM_ANGLE * NUM_BYTE32, STEERING_VEC_PF_ADDR + frameRst->target[targetNow].d3Idx * NUM_BYTE32); //src1IntraInc,src1BaseAddr
#if 0
            BB_OPCLEAR(BB_CLEAR_P2);
            BB_ISR_CLEAR(BB_ISR_P2_JUMP_END);
            BB_OPTRIG(BB_TRIG_P2);
            BB_waitISR(BB_ISR_P2_JUMP_END);
#if 0
            USART_Transmit_Bytes(&USART0, (uint8_t*)FFT4D_PRE_ADDR, NUM_ANT_V * NUM_BYTE32);
#endif
#if 0
            BB_DMA_Cfg0(DMA_CFG0_MINMAX_MODE_ABS, DMA_CFG0_FLT2PF_LSF_EN_RIGHT, 0, 0, 0, NUM_ANT_V - 1, DMA_CFG0_WMODE_ORI, DMA_CFG0_DSIZE_32BIT); //jumpCnt,interCnt,intraCnt,fix2pf_sel,dsize
            BB_DMA_Cfg1(0, 0); //srcJumpInc,srcInterInc
            BB_DMA_Cfg2(NUM_BYTE32, FFT4D_PRE_ADDR); //srcIntraInc,srcBaseAddr
            BB_DMA_Cfg3(0, 0); //dstJumpInc,dstInterInc
            BB_DMA_Cfg4(0, BB_FFT_BASE); //dstIntraInc,dstBaseAddr
            BB_FFT_Cfg0(0, 0, FFT4D_USE_A, FFT4D_USE_B, NUM_ANT_V - 1, FFTPT_128, FFT_UNLOAD_EN, FFT_MODE_FORWARD); //rsfOutput,rsfInput,useA,useB,inPt,fftPt,unloadEn,mode   //useAuseB
            BB_FFT_Cfg1(WIN_RANGE_ADDR, FFT_CFG1_OUTLSFEN_RIGHT, 0, FFT_CFG1_INLSFEN_RIGHT, 0, FFT_CFG1_WIN_SIZE_14BIT, FFT_CFG1_WIN_DIS); //winBaseAddr,winSize,winEn
            BB_FFT_Cfg2(NUM_BYTE32, FFT4D_CACHE_ADDR); //dstIntraInc,dstBaseAddr
            BB_FFT_ZP_Clear();
            bb_regb_str->FFT_SRC_ZP00 = (uint64_t)0x7;
            bb_regb_str->FFT_SRC_ZP01 = (uint64_t)0;
            //              bb_regb_str->FFT_SRC_ZP02 = (uint64_t)0;
            //              bb_regb_str->FFT_SRC_ZP03 = (uint64_t)0;
            BB_OPCLEAR(BB_CLEAR_FFT | BB_CLEAR_DMA);
            BB_OPGATE_EN(BB_GATE_DMA | BB_GATE_FFT);
            BB_FFT_CMD0(FFT_CMD0_CLEAR_IBUF);
            BB_ISR_CLEAR(BB_ISR_FFT_UNLOAD_END);
            BB_OPTRIG(BB_TRIG_DMA);
            BB_waitISR(BB_ISR_FFT_UNLOAD_END);
#endif
#endif

            //FFT4D
            BB_OPCLEAR(BB_CLEAR_FFT | BB_CLEAR_P2);
            BB_FFT_CMD0(FFT_CMD0_CLEAR_IBUF);
            BB_ISR_CLEAR(BB_ISR_FFT_UNLOAD_END);
            BB_OPTRIG(BB_TRIG_P2);
            BB_waitISR(BB_ISR_FFT_UNLOAD_END);

#if 0
            USART_Transmit_Bytes(&USART0, (uint8_t*)FFT4D_CACHE_ADDR, NUM_ANGLE * NUM_BYTE32);
#endif
            frameRst->target[targetNow].d4Idx = BB_FFT_STA0_curMaxIdx;
        }
        BB_OPGATE_DIS(BB_GATE_P2 | BB_GATE_FFT);
#endif
#if 0//fft 2d max
        {
            uint32_t cfarMax = 0;
            for (uint32_t cfarNow = 1; cfarNow < cfarTargetNum; cfarNow++) {
                if (FFT2D_ABS_Buf[bb_cfarRst_str[cfarMax].interIdx + bb_cfarRst_str[cfarMax].intraIdx * NUM_BIG_CHIRP_BYTES / NUM_BYTE32] < FFT2D_ABS_Buf[bb_cfarRst_str[cfarNow].interIdx + bb_cfarRst_str[cfarNow].intraIdx * NUM_BIG_CHIRP_BYTES / NUM_BYTE32]) {
                    cfarMax = cfarNow;
                }
            }
            uint32_t sTemp = bb_cfarRst_str[cfarMax].interIdx + bb_cfarRst_str[cfarMax].intraIdx * NUM_BIG_CHIRP_BYTES / NUM_BYTE32;
            USART_Transmit_Bytes(&USART0, (uint8_t *)&head[0], 8);
            for (uint32_t antNow = 0; antNow < NUM_ANT; antNow++) {
                USART_Transmit_Bytes(&USART0, (uint8_t*)&FFT2D_REAL_Buf[sTemp + antNow * USE_RANGE], NUM_BYTE32);
                USART_Transmit_Bytes(&USART0, (uint8_t*)&FFT2D_IMAG_Buf[sTemp + antNow * USE_RANGE], NUM_BYTE32);
            }
        }
#endif
#endif
//              TEST_WritePin=0;
        scmd = 1;
        break;
//          HW_Delay_ms(250);
    case 1:
#if 0
        scmd = 0;
#endif
#if 1
        rtc_current = ((uint64_t)RTC->CNT1 << 32) + RTC->CNT0;
        frameTime = (uint16_t)(rtc_current - rtc_last) * 1000 / lrc_clk;
        rtc_last = rtc_current;
        ProtocolSendLen = ProtocolSendPack(frameRst, ProtocolPackBuff, frameTime);

        Capture_SendResult(realSUM, imageSUM, RANGE_BIN, ProtocolPackBuff, ProtocolSendLen);
	
					
		
       // USART_Transmit_Bytes(&USART0, (uint8_t*)&ProtocolPackBuff[0], ProtocolSendLen);
        scmd = 0;
        DeepSleep_Config();
        RF_Clock40M_En();
        SYS_CLK_PLL_Init();
        ADC_Init();
#endif
        break;
    }
}
void Func03_Handle(void) {
    USART_Transmit_Bytes(&USART0, (uint8_t*)IMPORT_ADDR, IMPORT_MAX_LEN);
    r2_str.WorkMode = 0;
}
