#ifndef CAPTURE_OUTPUT_H
#define CAPTURE_OUTPUT_H
#include <stdint.h>
#define CAPTURE_LEGACY 0
#define CAPTURE_ADC 1
#define CAPTURE_IQ 2
#define CAPTURE_POINT 3
int Capture_Command(const uint8_t *command, uint32_t length);
void Capture_Started(void);
void Capture_Begin(void);
void Capture_SendADC(const void *data, uint32_t length);
void Capture_SendResult(const float *real, const float *imag, uint32_t count,
                        const uint8_t *point, uint32_t length);
#endif
