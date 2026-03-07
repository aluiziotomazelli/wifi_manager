#include "wifi_manager.hpp"

extern "C" void app_main(void)
{
    using namespace wifi_manager;
    auto &wm = WiFiManager::get_instance();
    wm.init();
}