/* Host peripheral boundary for testing the actual firmware packet encoder. */
#ifndef TEST_MAIN_H
#define TEST_MAIN_H
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef struct { uint32_t CNT0,CNT1; } TestRTC;
extern TestRTC test_rtc;
#define RTC (&test_rtc)
extern uint32_t lrc_clk;
extern int USART0;
typedef struct { uint8_t bandWidth,Proj_Func; } TestConfig;
extern TestConfig r2_str;
void USART_Transmit_Bytes(int *port,uint8_t *data,int32_t size);
int capture_printf(const char *format,...);
#define printf capture_printf
#endif
