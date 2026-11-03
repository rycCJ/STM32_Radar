#include "main.h"
#include "Capture_Output.h"

static uint8_t mode = CAPTURE_LEGACY;
static uint8_t managed = 0;
static uint32_t sequence = 0;
static uint64_t acquisition_ticks = 0;
static uint32_t frame_crc;

static void put16(uint8_t *p, uint16_t v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); }
static void put32(uint8_t *p, uint32_t v) {
    p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); p[2]=(uint8_t)(v>>16); p[3]=(uint8_t)(v>>24);
}
static uint32_t crc_update(uint32_t crc, const uint8_t *p, uint32_t n) {
    uint32_t i;
    while (n--) {
        crc ^= *p++;
        for (i=0;i<8;i++) crc=(crc>>1)^((0U-(crc&1U))&0xEDB88320U);
    }
    return crc;
}
static void send_part(const void *data, uint32_t n) {
    frame_crc=crc_update(frame_crc,(const uint8_t *)data,n);
    USART_Transmit_Bytes(&USART0,(uint8_t *)data,(int32_t)n);
}
static void begin_packet(uint8_t type,uint32_t bytes,uint16_t count,uint16_t chirp,
                         uint16_t slot,uint8_t sample_type) {
    uint8_t h[48];
    uint32_t step;
    float bin_m=0.0f;
    /* Recompute metadata only: old RangeRes can precede Default_Param_Config. */
    if (type==CAPTURE_IQ) {
        float bandwidth=r2_str.bandWidth==1?6500000000.0f:3220000000.0f;
        bin_m=3.0e8f/(2.0f*(1024.0f/20.0f)*(bandwidth/55.0f));
    }
    memcpy(&step,&bin_m,4);
    memset(h,0,sizeof(h));
    memcpy(h,"RDR1",4); h[4]=1; h[5]=type; put16(h+6,48);
    put32(h+8,bytes); put32(h+12,sequence);
    put32(h+16,(uint32_t)acquisition_ticks); put32(h+20,(uint32_t)(acquisition_ticks>>32));
    put32(h+24,lrc_clk); put16(h+28,count); put16(h+30,chirp); put16(h+32,slot);
    h[34]=sample_type; h[35]=(sequence==0)?1:0;
    put32(h+36,10000000U); put32(h+40,step);
    frame_crc=0xFFFFFFFFU;
    USART_Transmit_Bytes(&USART0,h,4);
    send_part(h+4,44);
}
static void end_packet(void) {
    uint8_t crc[4]; put32(crc,frame_crc^0xFFFFFFFFU);
    USART_Transmit_Bytes(&USART0,crc,4); sequence++;
}
void Capture_Begin(void) {
    uint32_t high,low;
    if (mode==CAPTURE_LEGACY) return;
    do { high=RTC->CNT1; low=RTC->CNT0; } while (high!=RTC->CNT1);
    acquisition_ticks=((uint64_t)high<<32)|low;
}
int Capture_Command(const uint8_t *cmd,uint32_t length) {
    static const char *names[]={"LEGACY","ADC","IQ","POINT"};
    uint32_t i;
    if (length<8 || memcmp(cmd,"AT+MODE=",8)!=0) return 0;
    for (i=0;i<4;i++) {
        if (length==8+strlen(names[i]) && memcmp(cmd+8,names[i],length-8)==0) {
            mode=(uint8_t)i; managed=1; sequence=0;
            r2_str.Proj_Func=(mode==CAPTURE_ADC)?1:2;
            printf("AT+MODE=%s\n",names[i]); return 1;
        }
    }
    printf("AT+ERROR=MODE\n"); return 1;
}
void Capture_Started(void) { if (managed) printf("AT+STARTED\n"); }
void Capture_SendADC(const void *data,uint32_t length) {
    static const uint8_t legacy_head[]={1,2,3,4,5,6,7,8};
    if (mode==CAPTURE_ADC) {
        begin_packet(CAPTURE_ADC,length,(uint16_t)(length/2),0,0,1);
        send_part(data,length); end_packet();
    } else {
        USART_Transmit_Bytes(&USART0,(uint8_t *)legacy_head,8);
        USART_Transmit_Bytes(&USART0,(uint8_t *)data,(int32_t)length);
    }
}
void Capture_SendResult(const float *real,const float *imag,uint32_t count,
                         const uint8_t *point,uint32_t length) {
    static const uint8_t head[]={0x66,0xBB},tail[]={0xBB,0x66};
    if (mode==CAPTURE_IQ) {
        begin_packet(CAPTURE_IQ,count*8,(uint16_t)count,2,6,2);
        send_part(real,count*4); send_part(imag,count*4); end_packet();
    } else if (mode==CAPTURE_POINT) {
        begin_packet(CAPTURE_POINT,length,(uint16_t)((length-12)/9),0xFFFF,0xFFFF,3);
        send_part(point,length); end_packet();
    } else {
        USART_Transmit_Bytes(&USART0,(uint8_t *)head,2);
        USART_Transmit_Bytes(&USART0,(uint8_t *)real,(int32_t)(count*4));
        USART_Transmit_Bytes(&USART0,(uint8_t *)imag,(int32_t)(count*4));
        USART_Transmit_Bytes(&USART0,(uint8_t *)tail,2);
        USART_Transmit_Bytes(&USART0,(uint8_t *)point,(int32_t)length);
    }
}
