#ifndef RADAR_INTERNAL_H
#define RADAR_INTERNAL_H
#include "App_Func.h"
#define RANGE_BIN 150
extern uint32_t head[2];
extern const uint32_t nco_fcw[NUM_ANT];
extern const uint32_t calib_ap[NUM_ANT];
extern uint8_t start_FFT[2];
extern uint8_t end_FFT[2];
extern uint64_t rtc_last;
extern uint32_t *calib_apBuf_Cpx;
extern uint32_t *calib_apBuf_Pf;
extern uint16_t *winRange;
extern uint16_t *winVel;
extern uint32_t *sincBuf_Cpx;
extern uint32_t *sincBuf_Pf;
extern uint32_t *steeringVec_Pf;
extern uint32_t *FFT1D_Buf;
extern uint32_t *FFT1D_ABS_Buf;
extern uint16_t *abuf16;
extern uint32_t *abuf32;
extern uint32_t *FFT2D_Buf;
extern uint32_t *FFT2D_ABS_Buf;
extern uint32_t *FFT3D_Buf;
extern uint32_t *FFT4D_Pre;
extern BB_DPKRST_T *bb_dpkRst_str;
extern BB_CFARRST_T *bb_cfarRst_str;
extern STRUCT_FRAMERST *frameRst;
extern uint8_t *ProtocolPackBuff;
extern uint32_t total;
extern uint32_t ProtocolSendLen;
extern uint32_t cfarTargetNum;
extern uint16_t frameTime;
#endif
