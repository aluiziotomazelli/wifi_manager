
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "esp_netif_types.h"
#include "esp_wifi_types.h"

#include "wifi_event_handler.hpp"
#include "interfaces/i_wifi_sync_manager.hpp"

constexpr esp_event_base_t ANY_BASE = nullptr;

using namespace wifi_manager;
using namespace testing;

class MockWiFiSyncManager : public IWiFiSyncManager
{
public:
    MOCK_METHOD(esp_err_t, init, (), (override));
    MOCK_METHOD(void, deinit, (), (override));
    MOCK_METHOD(esp_err_t, post_message, (const wifi_manager::Message &msg), (override));
    MOCK_METHOD(esp_err_t, post_message_from_isr, (const wifi_manager::Message &msg), (override));
    MOCK_METHOD(void, clear_bits, (uint32_t), (override));
    MOCK_METHOD(void, set_bits, (uint32_t), (override));
    MOCK_METHOD(uint32_t, wait_for_bits, (uint32_t, uint32_t), (override));
    MOCK_METHOD(bool, is_initialized, (), (const, override));
    MOCK_METHOD(QueueHandle_t, get_queue, (), (const, override));
    MOCK_METHOD(EventGroupHandle_t, get_event_group, (), (const, override));
};

class WiFiEventHandlerTest : public ::testing::Test
{
protected:
    MockWiFiSyncManager sync;
    WiFiEventHandler handler{&sync};
};

// -- -handle_wifi_event-- -

TEST_F(WiFiEventHandlerTest, WifiEventStaStart)
{
    EXPECT_CALL(sync, post_message_from_isr(Field(&wifi_manager::Message::event, EventId::STA_START)));
    handler.handle_wifi_event(ANY_BASE, WIFI_EVENT_STA_START, nullptr);
}

TEST_F(WiFiEventHandlerTest, WifiEventStaStop)
{
    EXPECT_CALL(sync, post_message_from_isr(Field(&wifi_manager::Message::event, EventId::STA_STOP)));
    handler.handle_wifi_event(ANY_BASE, WIFI_EVENT_STA_STOP, nullptr);
}

TEST_F(WiFiEventHandlerTest, WifiEventStaConnected)
{
    EXPECT_CALL(sync, post_message_from_isr(Field(&wifi_manager::Message::event, EventId::STA_CONNECTED)));
    handler.handle_wifi_event(ANY_BASE, WIFI_EVENT_STA_CONNECTED, nullptr);
}

TEST_F(WiFiEventHandlerTest, WifiEventStaDisconnectedWithData)
{
    wifi_event_sta_disconnected_t disc = {};
    disc.reason = WIFI_REASON_AUTH_EXPIRE;
    disc.rssi = -70;

    EXPECT_CALL(
        sync,
        post_message_from_isr(AllOf(
            Field(&wifi_manager::Message::event, EventId::STA_DISCONNECTED),
            Field(&wifi_manager::Message::reason, WIFI_REASON_AUTH_EXPIRE),
            Field(&wifi_manager::Message::rssi, -70))));
    handler.handle_wifi_event(ANY_BASE, WIFI_EVENT_STA_DISCONNECTED, &disc);
}

TEST_F(WiFiEventHandlerTest, WifiEventStaDisconnectedNullData)
{
    // reason e rssi ficam zerados quando data == nullptr
    EXPECT_CALL(
        sync,
        post_message_from_isr(AllOf(
            Field(&wifi_manager::Message::event, EventId::STA_DISCONNECTED),
            Field(&wifi_manager::Message::reason, 0),
            Field(&wifi_manager::Message::rssi, 0))));
    handler.handle_wifi_event(ANY_BASE, WIFI_EVENT_STA_DISCONNECTED, nullptr);
}

TEST_F(WiFiEventHandlerTest, WifiEventUnhandledIsIgnored)
{
    EXPECT_CALL(sync, post_message_from_isr(_)).Times(0);
    handler.handle_wifi_event(ANY_BASE, WIFI_EVENT_STA_BEACON_TIMEOUT, nullptr);
}

// --- handle_ip_event ---

TEST_F(WiFiEventHandlerTest, IpEventGotIp)
{
    EXPECT_CALL(sync, post_message_from_isr(Field(&wifi_manager::Message::event, EventId::GOT_IP)));
    handler.handle_ip_event(ANY_BASE, IP_EVENT_STA_GOT_IP, nullptr);
}

TEST_F(WiFiEventHandlerTest, IpEventUnhandledIsIgnored)
{
    EXPECT_CALL(sync, post_message_from_isr(_)).Times(0);
    handler.handle_ip_event(ANY_BASE, IP_EVENT_AP_STAIPASSIGNED, nullptr);
}

// --- null sync_manager ---

TEST_F(WiFiEventHandlerTest, NullSyncManagerDoesNotCrash)
{
    WiFiEventHandler null_handler{nullptr};
    EXPECT_NO_FATAL_FAILURE(null_handler.handle_wifi_event(ANY_BASE, WIFI_EVENT_STA_START, nullptr));
    EXPECT_NO_FATAL_FAILURE(null_handler.handle_ip_event(ANY_BASE, IP_EVENT_STA_GOT_IP, nullptr));
}

// --- static callbacks ---

TEST_F(WiFiEventHandlerTest, StaticWifiCallbackDispatchesToHandler)
{
    EXPECT_CALL(sync, post_message_from_isr(Field(&wifi_manager::Message::event, EventId::STA_START)));
    WiFiEventHandler::wifi_event_callback(&handler, ANY_BASE, WIFI_EVENT_STA_START, nullptr);
}

TEST_F(WiFiEventHandlerTest, StaticIpCallbackDispatchesToHandler)
{
    EXPECT_CALL(sync, post_message_from_isr(Field(&wifi_manager::Message::event, EventId::GOT_IP)));
    WiFiEventHandler::ip_event_callback(&handler, ANY_BASE, IP_EVENT_STA_GOT_IP, nullptr);
}

TEST_F(WiFiEventHandlerTest, StaticCallbackNullArgDoesNotCrash)
{
    EXPECT_NO_FATAL_FAILURE(WiFiEventHandler::wifi_event_callback(nullptr, ANY_BASE, WIFI_EVENT_STA_START, nullptr));
}

TEST_F(WiFiEventHandlerTest, StaticIpCallbackNullArgDoesNotCrash)
{
    EXPECT_NO_FATAL_FAILURE(WiFiEventHandler::ip_event_callback(nullptr, ANY_BASE, IP_EVENT_STA_GOT_IP, nullptr));
}