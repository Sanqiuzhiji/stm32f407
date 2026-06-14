#ifndef JUSTFLOAT_PROTOCOL_H
#define JUSTFLOAT_PROTOCOL_H

#include <stdint.h>

#define JUSTFLOAT_MAX_CHANNELS 16U
#define JUSTFLOAT_FRAME_END_WORD 0x7F800000UL

typedef struct
{
    float data[JUSTFLOAT_MAX_CHANNELS];
    uint32_t pend;
} justfloat_frame_t;

uint16_t JustFloat_BuildFrame(justfloat_frame_t* frame,
                              const float* data,
                              uint8_t channel_count);

#endif
