#include "App/SdImageViewer/sd_image_viewer.h"

#include "fatfs.h"
#include "MMInterface/LCD/tft_lcd.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define SD_IMAGE_MAX_FILES 50U
#define SD_IMAGE_NAME_LEN 16U
#define BMP_HEADER_MIN_SIZE 54U
#define BMP_MAX_WIDTH TFT_LCD_WIDTH
#define BMP_MAX_HEIGHT TFT_LCD_HEIGHT
#define VIEWER_COLOR_SD_START 0x001FU
#define VIEWER_COLOR_LINK_ERROR 0xF800U
#define VIEWER_COLOR_MOUNT_ERROR 0xFFE0U
#define VIEWER_COLOR_SCAN_ERROR 0x07FFU
#define VIEWER_COLOR_NO_IMAGE 0x07E0U
#define VIEWER_COLOR_IMAGE_ERROR 0xF81FU
#define BMP_READ_RETRY_COUNT 3U

static char image_files[SD_IMAGE_MAX_FILES][SD_IMAGE_NAME_LEN];
static uint8_t image_count = 0U;
static uint8_t current_image = 0U;
static bool initialized = false;
static volatile int8_t pending_delta = 0;

__ALIGN_BEGIN static uint8_t bmp_row_buffer[BMP_MAX_WIDTH * 3U] __ALIGN_END;
__ALIGN_BEGIN static uint16_t lcd_line_buffer[BMP_MAX_WIDTH] __ALIGN_END;

static FRESULT scan_fixed_image_names(void);
static FRESULT show_current_image(void);
static FRESULT draw_bmp_file(const char* path);
static FRESULT read_exact_at(FIL* file, uint32_t offset, uint8_t* buffer, UINT size);
static void build_image_path(char* dst, size_t dst_size, const char* name);
static bool is_fixed_bmp_name(const char* name, uint8_t* index);
static bool is_bmp_name(const char* name);
static bool image_path_exists(const char* path);
static char ascii_upper(char c);
static uint16_t read_le16(const uint8_t* data);
static uint32_t read_le32(const uint8_t* data);
static int32_t read_le32s(const uint8_t* data);
static uint16_t bgr888_to_rgb565(uint8_t b, uint8_t g, uint8_t r);

bool SdImageViewer_Init(void)
{
    if (initialized)
    {
        return (image_count > 0U);
    }

    initialized = true;
    TftLcd_Clear(VIEWER_COLOR_SD_START);

    if (retSD != 0U)
    {
        // printf("FatFS link failed: %u\r\n", retSD);
        TftLcd_Clear(VIEWER_COLOR_LINK_ERROR);
        return false;
    }

    FRESULT res = f_mount(&SDFatFS, SDPath, 1);
    if (res != FR_OK)
    {
        // printf("SD mount failed: %d\r\n", res);
        TftLcd_Clear(VIEWER_COLOR_MOUNT_ERROR);
        return false;
    }

    res = scan_fixed_image_names();
    if (res != FR_OK)
    {
        // printf("SD image scan failed: %d\r\n", res);
        TftLcd_Clear(VIEWER_COLOR_SCAN_ERROR);
        return false;
    }

    // printf("SD images found: %u\r\n", image_count);

    if (image_count == 0U)
    {
        TftLcd_Clear(VIEWER_COLOR_NO_IMAGE);
        return false;
    }

    current_image = 0U;
    return (show_current_image() == FR_OK);
}

void SdImageViewer_RequestNext(void)
{
    pending_delta = 1;
}

void SdImageViewer_RequestPrevious(void)
{
    pending_delta = -1;
}

void SdImageViewer_TaskTick(void)
{
    if (!initialized || image_count == 0U || pending_delta == 0)
    {
        return;
    }

    int8_t delta = pending_delta;
    pending_delta = 0;

    while (delta > 0)
    {
        current_image = (uint8_t)((current_image + 1U) % image_count);
        delta--;
    }

    while (delta < 0)
    {
        current_image = (current_image == 0U) ? (uint8_t)(image_count - 1U) : (uint8_t)(current_image - 1U);
        delta++;
    }

    (void)show_current_image();
}

bool SdImageViewer_IsActive(void)
{
    return (initialized && image_count > 0U);
}

uint8_t SdImageViewer_GetImageCount(void)
{
    return image_count;
}

static FRESULT scan_fixed_image_names(void)
{
    DIR dir;
    FILINFO info;
    char extra_files[SD_IMAGE_MAX_FILES][SD_IMAGE_NAME_LEN];
    uint8_t extra_count = 0U;

    image_count = 0U;

    for (uint8_t i = 0U; i < SD_IMAGE_MAX_FILES; i++)
    {
        image_files[i][0] = '\0';
        extra_files[i][0] = '\0';
    }

    FRESULT res = f_opendir(&dir, SDPath);
    if (res != FR_OK)
    {
        return res;
    }

    for (;;)
    {
        res = f_readdir(&dir, &info);
        if (res != FR_OK || info.fname[0] == '\0')
        {
            break;
        }

        if ((info.fattrib & AM_DIR) != 0U)
        {
            continue;
        }

        uint8_t index = 0U;
        if (is_fixed_bmp_name(info.fname, &index))
        {
            build_image_path(image_files[index - 1U], SD_IMAGE_NAME_LEN, info.fname);
        }
        else if (is_bmp_name(info.fname) && extra_count < SD_IMAGE_MAX_FILES)
        {
            build_image_path(extra_files[extra_count], SD_IMAGE_NAME_LEN, info.fname);
            extra_count++;
        }
    }

    (void)f_closedir(&dir);

    if (res != FR_OK)
    {
        return res;
    }

    uint8_t write_index = 0U;
    for (uint8_t i = 0U; i < SD_IMAGE_MAX_FILES; i++)
    {
        if (image_files[i][0] != '\0')
        {
            if (write_index != i)
            {
                (void)snprintf(image_files[write_index], SD_IMAGE_NAME_LEN, "%s", image_files[i]);
            }
            write_index++;
        }
    }

    for (uint8_t i = 0U; i < extra_count && write_index < SD_IMAGE_MAX_FILES; i++)
    {
        if (!image_path_exists(extra_files[i]))
        {
            (void)snprintf(image_files[write_index], SD_IMAGE_NAME_LEN, "%s", extra_files[i]);
            write_index++;
        }
    }

    image_count = write_index;
    return FR_OK;
}

static FRESULT show_current_image(void)
{
    if (current_image >= image_count)
    {
        return FR_INVALID_OBJECT;
    }

    // printf("Show image %u/%u: %s\r\n",
    //        (uint32_t)current_image + 1U,
    //        (uint32_t)image_count,
    //        image_files[current_image]);

    FRESULT res = draw_bmp_file(image_files[current_image]);
    if (res != FR_OK)
    {
        // printf("Draw image failed: %d\r\n", res);
    }

    return res;
}

static FRESULT draw_bmp_file(const char* path)
{
    FIL file;
    uint8_t header[BMP_HEADER_MIN_SIZE];
    UINT bytes_read = 0U;

    FRESULT res = f_open(&file, path, FA_READ);
    if (res != FR_OK)
    {
        return res;
    }

    res = f_read(&file, header, sizeof(header), &bytes_read);
    if (res != FR_OK || bytes_read != sizeof(header))
    {
        (void)f_close(&file);
        return (res == FR_OK) ? FR_INVALID_OBJECT : res;
    }

    if (header[0] != 'B' || header[1] != 'M')
    {
        (void)f_close(&file);
        return FR_INVALID_OBJECT;
    }

    uint32_t pixel_offset = read_le32(&header[10]);
    uint32_t dib_size = read_le32(&header[14]);
    int32_t width = read_le32s(&header[18]);
    int32_t height_raw = read_le32s(&header[22]);
    uint16_t planes = read_le16(&header[26]);
    uint16_t bits_per_pixel = read_le16(&header[28]);
    uint32_t compression = read_le32(&header[30]);

    if (dib_size < 40U || width <= 0 || height_raw == 0 || planes != 1U ||
        bits_per_pixel != 24U || compression != 0U)
    {
        (void)f_close(&file);
        return FR_INVALID_OBJECT;
    }

    bool top_down = (height_raw < 0);
    uint32_t image_width = (uint32_t)width;
    uint32_t image_height = top_down ? (uint32_t)(-height_raw) : (uint32_t)height_raw;

    if (image_width > BMP_MAX_WIDTH || image_height > BMP_MAX_HEIGHT)
    {
        (void)f_close(&file);
        return FR_INVALID_OBJECT;
    }

    uint32_t row_stride = ((image_width * 3U) + 3U) & ~3UL;
    if (row_stride > sizeof(bmp_row_buffer))
    {
        (void)f_close(&file);
        return FR_INVALID_OBJECT;
    }

    uint16_t x0 = (uint16_t)((TftLcd_GetWidth() - image_width) / 2U);
    uint16_t y0 = (uint16_t)((TftLcd_GetHeight() - image_height) / 2U);

    TftLcd_Clear(TFT_LCD_COLOR_BLACK);

    for (uint32_t row = 0U; row < image_height; row++)
    {
        uint32_t row_offset = pixel_offset + (row * row_stride);
        res = read_exact_at(&file, row_offset, bmp_row_buffer, (UINT)row_stride);
        if (res != FR_OK)
        {
            (void)f_close(&file);
            return res;
        }

        for (uint32_t x = 0U; x < image_width; x++)
        {
            uint32_t src = x * 3U;
            lcd_line_buffer[x] = bgr888_to_rgb565(bmp_row_buffer[src],
                                                  bmp_row_buffer[src + 1U],
                                                  bmp_row_buffer[src + 2U]);
        }

        uint16_t y = top_down ? (uint16_t)(y0 + row)
                              : (uint16_t)(y0 + image_height - 1U - row);
        TftLcd_DrawRgb565Line(x0, y, lcd_line_buffer, (uint16_t)image_width);
    }

    return f_close(&file);
}

static FRESULT read_exact_at(FIL* file, uint32_t offset, uint8_t* buffer, UINT size)
{
    for (uint8_t attempt = 0U; attempt < BMP_READ_RETRY_COUNT; attempt++)
    {
        UINT bytes_read = 0U;
        FRESULT res = f_lseek(file, offset);
        if (res != FR_OK)
        {
            continue;
        }

        res = f_read(file, buffer, size, &bytes_read);
        if (res == FR_OK && bytes_read == size)
        {
            return FR_OK;
        }
    }

    return FR_DISK_ERR;
}

static void build_image_path(char* dst, size_t dst_size, const char* name)
{
    size_t len = strlen(SDPath);

    if (len > 0U && SDPath[len - 1U] == '/')
    {
        (void)snprintf(dst, dst_size, "%s%s", SDPath, name);
    }
    else
    {
        (void)snprintf(dst, dst_size, "%s/%s", SDPath, name);
    }
}

static bool is_fixed_bmp_name(const char* name, uint8_t* index)
{
    if (name == NULL || index == NULL)
    {
        return false;
    }

    if (ascii_upper(name[0]) != 'P' || ascii_upper(name[1]) != 'I' || ascii_upper(name[2]) != 'C')
    {
        return false;
    }

    if (name[3] < '0' || name[3] > '9' ||
        name[4] < '0' || name[4] > '9' ||
        name[5] < '0' || name[5] > '9' ||
        name[6] != '.' ||
        ascii_upper(name[7]) != 'B' ||
        ascii_upper(name[8]) != 'M' ||
        ascii_upper(name[9]) != 'P' ||
        name[10] != '\0')
    {
        return false;
    }

    uint16_t value = (uint16_t)((name[3] - '0') * 100 +
                                (name[4] - '0') * 10 +
                                (name[5] - '0'));
    if (value == 0U || value > SD_IMAGE_MAX_FILES)
    {
        return false;
    }

    *index = (uint8_t)value;
    return true;
}

static bool is_bmp_name(const char* name)
{
    if (name == NULL)
    {
        return false;
    }

    size_t len = strlen(name);
    if (len < 5U)
    {
        return false;
    }

    return (name[len - 4U] == '.' &&
            ascii_upper(name[len - 3U]) == 'B' &&
            ascii_upper(name[len - 2U]) == 'M' &&
            ascii_upper(name[len - 1U]) == 'P');
}

static bool image_path_exists(const char* path)
{
    for (uint8_t i = 0U; i < SD_IMAGE_MAX_FILES; i++)
    {
        if (image_files[i][0] != '\0' && strcmp(image_files[i], path) == 0)
        {
            return true;
        }
    }

    return false;
}

static char ascii_upper(char c)
{
    if (c >= 'a' && c <= 'z')
    {
        return (char)(c - ('a' - 'A'));
    }

    return c;
}

static uint16_t read_le16(const uint8_t* data)
{
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8U);
}

static uint32_t read_le32(const uint8_t* data)
{
    return (uint32_t)data[0] |
           ((uint32_t)data[1] << 8U) |
           ((uint32_t)data[2] << 16U) |
           ((uint32_t)data[3] << 24U);
}

static int32_t read_le32s(const uint8_t* data)
{
    return (int32_t)read_le32(data);
}

static uint16_t bgr888_to_rgb565(uint8_t b, uint8_t g, uint8_t r)
{
    return (uint16_t)(((uint16_t)(r & 0xF8U) << 8U) |
                      ((uint16_t)(g & 0xFCU) << 3U) |
                      ((uint16_t)b >> 3U));
}
