#include <stdio.h>
#include "controller_link.h"
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
int main(void) {
    int value;
#ifdef _WIN32
    _setmode(_fileno(stdin),_O_BINARY);
#endif
    ControllerPoints_Reset();
    while ((value=getchar())!=EOF) ControllerPoints_Byte((uint8_t)value);
    printf("%u\n",ControllerPoints_Count());
    return 0;
}
