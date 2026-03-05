#include "esp_task_wdt.h"
#include "unity.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void app_main(void)
{
    // Disable Task Watchdog to avoid triggers in Unity menu loop
    esp_task_wdt_deinit();

    // Give some time for QEMU UART to stabilize
    vTaskDelay(pdMS_TO_TICKS(100));

    unity_run_menu();

    // UNITY_BEGIN();
    // unity_run_all_tests();
    // UNITY_END();
}
