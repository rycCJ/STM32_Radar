#include "bsp.h"
#include "controller_link.h"
/* Observer only: never changes bytes forwarded to the PC. */
static uint8_t packet[4096];
static uint32_t used;
static uint16_t points;
static uint32_t get32(const uint8_t *p) {
    return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
static uint32_t crc32(const uint8_t *p,uint32_t n) {
    uint32_t crc=0xFFFFFFFFU,i;
    while (n--) { crc^=*p++; for(i=0;i<8;i++) crc=(crc>>1)^((0U-(crc&1U))&0xEDB88320U); }
    return crc^0xFFFFFFFFU;
}
uint16_t ControllerPoints_Count(void) { return points; }
void ControllerPoints_Reset(void) { used=0; points=0; }
void ControllerPoints_Byte(uint8_t value) {
    uint32_t length,n;
    if (used==sizeof(packet)) used=0;
    packet[used++]=value;
    while (used>=4) {
        if (!memcmp(packet,"RDR1",4)) {
            if (used<48) return;
            length=get32(packet+8);
            if (packet[4]!=1 || packet[6]!=48 || packet[7]!=0 || length>sizeof(packet)-52) goto shift;
            n=length+52; if (used<n) return;
            if (packet[5]==3 && length>=12 && crc32(packet+4,length+44)==get32(packet+48+length)) {
                points=(uint16_t)(packet[58]|((uint16_t)packet[59]<<8));
            }
            used=0; return;
        }
        if (packet[0]==0x66 && packet[1]==0xBB) {
            if (used<1204) return;
            if (packet[1202]==0xBB && packet[1203]==0x66) { used=0; return; }
            goto shift;
        }
        if (packet[0]==0x55 && packet[1]==0xAA) {
            if (used<6) return;
            length=get32(packet+2);
            if (length<6 || length>sizeof(packet)-6) goto shift;
            n=length+6; if (used<n) return;
            if ((n-12)%9==0 && packet[8]==1 && packet[9]==1) points=(uint16_t)(packet[10]|((uint16_t)packet[11]<<8));
            used=0; return;
        }
shift:
        memmove(packet,packet+1,--used);
    }
}
