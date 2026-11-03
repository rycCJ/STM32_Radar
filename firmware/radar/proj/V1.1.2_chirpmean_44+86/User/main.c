#include "main.h"
void Sys_Init(void);
void App_Init(void);
void App_Loop(void);
int main(void)
{
 Set_CoreLDO_Voltage(2);
 Sys_Init();
 MPU_LocalConfig();
 App_Init();
 while (1) { App_Loop(); }
}
