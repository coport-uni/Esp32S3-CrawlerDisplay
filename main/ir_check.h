#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t ir_check_init(void);

void ir_check_send_test(void);

#ifdef __cplusplus
}
#endif
