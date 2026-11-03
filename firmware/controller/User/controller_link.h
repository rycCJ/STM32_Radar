#ifndef CONTROLLER_LINK_H
#define CONTROLLER_LINK_H
#include <stdint.h>
void ControllerLink_Init(void);
void ControllerLink_Poll(void);
int ControllerLink_Quiet(void);
uint16_t ControllerPoints_Count(void);
void ControllerPoints_Reset(void);
void ControllerPoints_Byte(uint8_t value);
#endif
