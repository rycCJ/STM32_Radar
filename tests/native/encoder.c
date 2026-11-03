#include "main.h"
#include "Capture_Output.h"
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
TestRTC test_rtc={3200,0};
uint32_t lrc_clk=32000;
int USART0;
TestConfig r2_str={0,2};
void USART_Transmit_Bytes(int *port,uint8_t *data,int32_t size) {
    (void)port; fwrite(data,1,(size_t)size,stdout);
}
int capture_printf(const char *format,...) { (void)format; return 0; }
int main(int argc,char **argv) {
    float real[150],imag[150]; int16_t adc[512]; unsigned int i;
    uint8_t point[21]={0x55,0xAA,15,0,0,0,100,0,1,1,1,0};
    char command[40];
#ifdef _WIN32
    _setmode(_fileno(stdout),_O_BINARY);
#endif
    if (argc!=2) return 2;
    if (strcmp(argv[1],"ADCOLD")) {
        sprintf(command,"AT+MODE=%s",argv[1]);
        if (!Capture_Command((const uint8_t *)command,(uint32_t)strlen(command))) return 3;
    }
    for(i=0;i<150;i++) { real[i]=(float)i; imag[i]=(float)(1000+i); }
    for(i=0;i<512;i++) adc[i]=(int16_t)i;
    Capture_Begin();
    if (!strcmp(argv[1],"ADC") || !strcmp(argv[1],"ADCOLD")) Capture_SendADC(adc,sizeof(adc));
    else Capture_SendResult(real,imag,150,point,sizeof(point));
    return 0;
}
