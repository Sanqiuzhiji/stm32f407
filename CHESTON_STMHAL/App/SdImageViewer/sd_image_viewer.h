#ifndef SD_IMAGE_VIEWER_H
#define SD_IMAGE_VIEWER_H

#include <stdbool.h>
#include <stdint.h>

bool SdImageViewer_Init(void);
void SdImageViewer_RequestNext(void);
void SdImageViewer_RequestPrevious(void);
void SdImageViewer_TaskTick(void);
bool SdImageViewer_IsActive(void);
uint8_t SdImageViewer_GetImageCount(void);

#endif
