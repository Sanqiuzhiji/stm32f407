#include "justfloat_protocol.h"

#include <string.h>

uint16_t JustFloat_BuildFrame(justfloat_frame_t* frame,
                              const float* data,
                              uint8_t channel_count)
{
    if (frame == NULL || data == NULL || channel_count == 0U)
    {
        return 0U;
    }

    if (channel_count > JUSTFLOAT_MAX_CHANNELS)
    {
        channel_count = JUSTFLOAT_MAX_CHANNELS;
    }

    memcpy(frame->data, data, (uint32_t)channel_count * sizeof(float));

    const uint32_t pend = JUSTFLOAT_FRAME_END_WORD;
    // 从 frame 的起始地址开始，跳过 channel_count 个 float 的长度，这个位置就是帧尾应该写入的位置。
    uint8_t* end = (uint8_t*)frame + ((uint32_t)channel_count * sizeof(float));
    // 这里 end 是在 frame 的内部。
    memcpy(end, &pend, sizeof(pend));

    return (uint16_t)((channel_count + 1U) * sizeof(float));
}
