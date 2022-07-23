#pragma once

void platform_get_window_size(uint32_t* width, uint32_t* height);
char* platform_read_file(LPCWSTR path, uint32_t* length);