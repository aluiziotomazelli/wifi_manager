// host_test/common/mock_wifi_bootstrapper.hpp
#pragma once

#include "gmock/gmock.h"
#include "interfaces/i_wifi_bootstrapper.hpp"

namespace wifi_manager {

class MockWiFiBootstrapper : public IWiFiBootstrapper
{
public:
    MOCK_METHOD(esp_err_t, init, (void *pvParameters, TaskHandle_t *pxTaskHandle), (override));
    MOCK_METHOD(esp_err_t, deinit, (TaskHandle_t *pxTaskHandle), (override));
};

} // namespace wifi_manager
