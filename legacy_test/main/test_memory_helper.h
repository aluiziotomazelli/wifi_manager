#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void set_memory_leak_threshold(int threshold);
void reset_memory_leak_threshold(void);

#ifdef __cplusplus
}
#endif