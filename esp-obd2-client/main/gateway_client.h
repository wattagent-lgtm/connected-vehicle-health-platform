#pragma once

#include "esp_err.h"

esp_err_t gateway_client_send(const char *json);
void gateway_client_close(void);
