#include "board.h"
#include "main.h"

extern RTC_HandleTypeDef hrtc;
char* board_get_timestamp(char* buffer, size_t buffer_size)
{
    /* Needs "YYYY-MM-DD HH:MM:SS" => 19 chars + null */
    if (buffer_size < 20) {
        if (buffer_size > 0) buffer[0] = '\0';
        return buffer;
    }

    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};

    if (HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN) != HAL_OK ||
        HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN) != HAL_OK) {
        buffer[0] = '\0';
        return buffer;
    }

    uint32_t year = 2000U + (uint32_t)sDate.Year;
    snprintf(buffer, buffer_size, "%04lu-%02lu-%02lu %02lu:%02lu:%02lu",
             (unsigned long)year,
             (unsigned long)sDate.Month,
             (unsigned long)sDate.Date,
             (unsigned long)sTime.Hours,
             (unsigned long)sTime.Minutes,
             (unsigned long)sTime.Seconds);
    return buffer;
}
