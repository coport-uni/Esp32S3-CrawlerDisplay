#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t audio_check_init(void);

void audio_check_beep(void);

#ifdef __cplusplus
}
#endif
