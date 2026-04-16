#pragma once
#include <stdbool.h>

bool ota_mgr_handle_cmd_json(const char *json, int len);
void ota_mgr_mark_app_valid(void);