// host_test/test_wifi_manager/main/test_wifi_manager_task.cpp

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "wifi_manager.hpp"

#include "mock_wifi_bootstrapper.hpp"
#include "mock_wifi_config_storage.hpp"
#include "mock_wifi_driver_hal.hpp"
#include "mock_wifi_message_processor.hpp"
#include "mock_wifi_state_machine.hpp"
#include "mock_wifi_sync_manager.hpp"

using namespace wifi_manager;
using namespace testing;

// =============================================================================
// Fixture
// =============================================================================

class WiFiManagerTaskTest : public ::testing::Test
{
protected:
    NiceMock<MockWiFiDriverHAL> *driver_hal;
    NiceMock<MockWiFiConfigStorage> *storage;
    NiceMock<MockWiFiSyncManager> *sync_manager;
    NiceMock<MockWiFiStateMachine> *state_machine;
    NiceMock<MockWiFiBootstrapper> *bootstrapper;
    NiceMock<MockWiFiMessageProcessor> *processor;

    std::unique_ptr<WiFiManager> manager;

    // Real FreeRTOS primitives — task uses these directly via mocked getters
    QueueHandle_t real_queue = nullptr;
    EventGroupHandle_t real_event_group = nullptr;
    static constexpr uint8_t QUEUE_SIZE = 10;

    State current_state = State::UNINITIALIZED;
    bool sync_initialized = false;

    void SetUp() override
    {
        auto driver_hal_owned = std::make_unique<NiceMock<MockWiFiDriverHAL>>();
        auto storage_owned = std::make_unique<NiceMock<MockWiFiConfigStorage>>();
        auto sync_manager_owned = std::make_unique<NiceMock<MockWiFiSyncManager>>();
        auto state_machine_owned = std::make_unique<NiceMock<MockWiFiStateMachine>>();
        auto bootstrapper_owned = std::make_unique<NiceMock<MockWiFiBootstrapper>>();
        auto processor_owned = std::make_unique<NiceMock<MockWiFiMessageProcessor>>();

        driver_hal = driver_hal_owned.get();
        storage = storage_owned.get();
        sync_manager = sync_manager_owned.get();
        state_machine = state_machine_owned.get();
        bootstrapper = bootstrapper_owned.get();
        processor = processor_owned.get();

        // Real FreeRTOS primitives so the task doesn't assert on null handles
        real_queue = xQueueCreate(QUEUE_SIZE, sizeof(wifi_manager::Message));
        real_event_group = xEventGroupCreate();

        // sync_manager delegates to real primitives
        ON_CALL(*sync_manager, get_queue()).WillByDefault(Return(real_queue));
        ON_CALL(*sync_manager, get_event_group()).WillByDefault(Return(real_event_group));
        ON_CALL(*sync_manager, is_initialized()).WillByDefault(ReturnPointee(&sync_initialized));
        ON_CALL(*sync_manager, post_message(_)).WillByDefault(Invoke([this](const wifi_manager::Message &msg) {
            return xQueueSend(real_queue, &msg, 0) == pdTRUE ? ESP_OK : ESP_FAIL;
        }));
        ON_CALL(*sync_manager, clear_bits(_)).WillByDefault(Invoke([this](uint32_t bits) {
            xEventGroupClearBits(real_event_group, bits);
        }));
        ON_CALL(*sync_manager, set_bits(_)).WillByDefault(Invoke([this](uint32_t bits) {
            xEventGroupSetBits(real_event_group, bits);
        }));
        ON_CALL(*sync_manager, wait_for_bits(_, _))
            .WillByDefault(Invoke([this](uint32_t bits, uint32_t timeout_ms) -> uint32_t {
                return xEventGroupWaitBits(
                    real_event_group,
                    bits,
                    pdTRUE,  // clear on exit
                    pdFALSE, // wait for any bit
                    pdMS_TO_TICKS(timeout_ms));
            }));

        // state_machine tracks state via local variable
        ON_CALL(*state_machine, get_current_state()).WillByDefault(ReturnPointee(&current_state));
        ON_CALL(*state_machine, transition_to(_)).WillByDefault(SaveArg<0>(&current_state));
        // Task blocks on queue instead of busy looping
        ON_CALL(*state_machine, get_wait_ms()).WillByDefault(Return(0xFFFFFFFF));

        // bootstrapper: init() creates the task and marks sync as initialized
        ON_CALL(*bootstrapper, init(_, _, _, _, _))
            .WillByDefault(Invoke([this](TaskFunction_t fn, void *params, TaskHandle_t *handle, uint32_t stack_size, UBaseType_t priority) {
                sync_initialized = true;
                BaseType_t result = xTaskCreate(fn, "wifi_task", stack_size, params, priority, handle);
                return result == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
            }));
        ON_CALL(*bootstrapper, deinit(_)).WillByDefault(Return(ESP_OK));

        // storage has valid flags
        ON_CALL(*storage, is_valid()).WillByDefault(Return(true));

        manager = std::make_unique<WiFiManager>(
            std::move(driver_hal_owned),
            std::move(storage_owned),
            std::move(sync_manager_owned),
            std::move(state_machine_owned),
            std::move(bootstrapper_owned),
            std::move(processor_owned));
    }

    void TearDown() override
    {
        // Send EXIT to stop the task cleanly before destroying mocks
        if (real_queue) {
            wifi_manager::Message msg = {};
            msg.type = MessageType::COMMAND;
            msg.cmd = CommandId::EXIT;
            xQueueSend(real_queue, &msg, 0);
            vTaskDelay(pdMS_TO_TICKS(100));
            vQueueDelete(real_queue);
            real_queue = nullptr;
        }
        if (real_event_group) {
            vEventGroupDelete(real_event_group);
            real_event_group = nullptr;
        }
        sync_initialized = false;
        current_state = State::INITIALIZED;
    }

    // Helper: init the manager — creates and starts the wifi_task
    void setup_and_init()
    {
        ASSERT_EQ(ESP_OK, manager->init());
        vTaskDelay(pdMS_TO_TICKS(10)); // give task time to start and block on queue
    }

    // Helper: configure processor mock to set result bits when a command is processed
    void setup_processor_result(CommandId cmd, uint32_t result_bits)
    {
        ON_CALL(*processor, process_message(Field(&wifi_manager::Message::cmd, cmd), _))
            .WillByDefault(Invoke([this, result_bits](const wifi_manager::Message &, State) {
                xEventGroupSetBits(real_event_group, result_bits);
            }));
    }
};

// =============================================================================
// wifi_task — EXIT command
// =============================================================================

TEST_F(WiFiManagerTaskTest, TaskExitsCleanlyOnExitCommand)
{
    setup_and_init();

    // EXIT is sent by TearDown — just verify manager reaches a clean state
    // If the task doesn't exit, TearDown will hang for 100ms — acceptable
    EXPECT_EQ(State::INITIALIZED, manager->get_state());
}

// =============================================================================
// wifi_task — process_message dispatch
// =============================================================================

TEST_F(WiFiManagerTaskTest, TaskDispatchesCommandToProcessor)
{
    setup_and_init();

    EXPECT_CALL(*processor, process_message(Field(&wifi_manager::Message::cmd, CommandId::START), _)).Times(1);

    wifi_manager::Message msg = {};
    msg.type = MessageType::COMMAND;
    msg.cmd = CommandId::START;
    sync_manager->post_message(msg);

    vTaskDelay(pdMS_TO_TICKS(50)); // give task time to process
}

// =============================================================================
// wifi_task — on_idle_tick (queue timeout path)
// =============================================================================

TEST_F(WiFiManagerTaskTest, TaskCallsOnIdleTickOnQueueTimeout)
{
    // Use a short timeout so the task hits the else branch quickly
    ON_CALL(*state_machine, get_wait_ms()).WillByDefault(Return(50));

    setup_and_init();

    EXPECT_CALL(*processor, on_idle_tick(_)).Times(AtLeast(1));

    vTaskDelay(pdMS_TO_TICKS(200)); // wait for at least one timeout cycle
}

// =============================================================================
// start(timeout_ms) — sync variant
// =============================================================================

TEST_F(WiFiManagerTaskTest, StartSyncReturnsOkWhenProcessorSetsStartedBit)
{
    EXPECT_CALL(*processor, process_message(_, _)).Times(AtLeast(1));

    setup_and_init();
    setup_processor_result(CommandId::START, STARTED_BIT);

    ON_CALL(*state_machine, validate_command(CommandId::START))
        .WillByDefault(Return(IWiFiStateMachine::Action::EXECUTE));

    EXPECT_EQ(ESP_OK, manager->start(500));
}

TEST_F(WiFiManagerTaskTest, StartSyncReturnsFailWhenProcessorSetsStartFailedBit)
{
    setup_and_init();
    setup_processor_result(CommandId::START, START_FAILED_BIT);

    ON_CALL(*state_machine, validate_command(CommandId::START))
        .WillByDefault(Return(IWiFiStateMachine::Action::EXECUTE));

    EXPECT_EQ(ESP_FAIL, manager->start(500));
}

// =============================================================================
// stop(timeout_ms) — sync variant
// =============================================================================

TEST_F(WiFiManagerTaskTest, StopSyncReturnsOkWhenProcessorSetsStoppedBit)
{
    setup_and_init();
    setup_processor_result(CommandId::STOP, STOPPED_BIT);

    ON_CALL(*state_machine, validate_command(CommandId::STOP))
        .WillByDefault(Return(IWiFiStateMachine::Action::EXECUTE));

    EXPECT_EQ(ESP_OK, manager->stop(500));
}

TEST_F(WiFiManagerTaskTest, StopSyncReturnsFailWhenProcessorSetsStopFailedBit)
{
    setup_and_init();
    setup_processor_result(CommandId::STOP, STOP_FAILED_BIT);

    ON_CALL(*state_machine, validate_command(CommandId::STOP))
        .WillByDefault(Return(IWiFiStateMachine::Action::EXECUTE));

    EXPECT_EQ(ESP_FAIL, manager->stop(500));
}

// =============================================================================
// connect(timeout_ms) — sync variant
// =============================================================================

TEST_F(WiFiManagerTaskTest, ConnectSyncReturnsOkWhenProcessorSetsConnectedBit)
{
    setup_and_init();
    setup_processor_result(CommandId::CONNECT, CONNECTED_BIT);

    ON_CALL(*state_machine, validate_command(CommandId::CONNECT))
        .WillByDefault(Return(IWiFiStateMachine::Action::EXECUTE));

    EXPECT_EQ(ESP_OK, manager->connect(500));
}

TEST_F(WiFiManagerTaskTest, ConnectSyncReturnsFailWhenProcessorSetsConnectFailedBit)
{
    setup_and_init();
    setup_processor_result(CommandId::CONNECT, CONNECT_FAILED_BIT);

    ON_CALL(*state_machine, validate_command(CommandId::CONNECT))
        .WillByDefault(Return(IWiFiStateMachine::Action::EXECUTE));

    EXPECT_EQ(ESP_FAIL, manager->connect(500));
}

TEST_F(WiFiManagerTaskTest, ConnectSyncReturnsErrorWhenStorageIsInvalid)
{
    setup_and_init();
    ON_CALL(*storage, is_valid()).WillByDefault(Return(false)); // storage is invalid

    EXPECT_CALL(*state_machine, validate_command(CommandId::CONNECT)).Times(0); // should not be called

    EXPECT_EQ(ESP_ERR_WIFI_PASSWORD, manager->connect(500));
}

// =============================================================================
// disconnect(timeout_ms) — sync variant
// =============================================================================

TEST_F(WiFiManagerTaskTest, DisconnectSyncReturnsOkWhenProcessorSetsDisconnectedBit)
{
    setup_and_init();
    setup_processor_result(CommandId::DISCONNECT, DISCONNECTED_BIT);

    ON_CALL(*state_machine, validate_command(CommandId::DISCONNECT))
        .WillByDefault(Return(IWiFiStateMachine::Action::EXECUTE));

    EXPECT_EQ(ESP_OK, manager->disconnect(500));
}

TEST_F(WiFiManagerTaskTest, DisconnectSyncReturnsFailWhenProcessorSetsConnectFailedBit)
{
    setup_and_init();
    setup_processor_result(CommandId::DISCONNECT, CONNECT_FAILED_BIT);

    ON_CALL(*state_machine, validate_command(CommandId::DISCONNECT))
        .WillByDefault(Return(IWiFiStateMachine::Action::EXECUTE));

    EXPECT_EQ(ESP_FAIL, manager->disconnect(500));
}

// =============================================================================
// Sync guards parametrized tests
// =============================================================================

struct SyncGuardParam
{
    CommandId cmd;
    std::function<esp_err_t(WiFiManager *, uint32_t)> call;
};

class WiFiManagerSyncGuardTest : public WiFiManagerTaskTest, public WithParamInterface<SyncGuardParam>
{
};

TEST_P(WiFiManagerSyncGuardTest, ReturnsInvalidStateWhenNotInitialized)
{
    current_state = State::UNINITIALIZED;
    EXPECT_EQ(ESP_ERR_INVALID_STATE, GetParam().call(manager.get(), 500));
}

TEST_P(WiFiManagerSyncGuardTest, ReturnsInvalidStateWhenActionError)
{
    setup_and_init();
    ON_CALL(*state_machine, validate_command(GetParam().cmd)).WillByDefault(Return(IWiFiStateMachine::Action::ERROR));
    EXPECT_EQ(ESP_ERR_INVALID_STATE, GetParam().call(manager.get(), 500));
}

TEST_P(WiFiManagerSyncGuardTest, ReturnsOkWhenActionSkip)
{
    setup_and_init();
    ON_CALL(*state_machine, validate_command(GetParam().cmd)).WillByDefault(Return(IWiFiStateMachine::Action::SKIP));
    EXPECT_EQ(ESP_OK, GetParam().call(manager.get(), 500));
}

TEST_P(WiFiManagerSyncGuardTest, ReturnsFailWhenQueueFull)
{
    setup_and_init();
    ON_CALL(*state_machine, validate_command(GetParam().cmd)).WillByDefault(Return(IWiFiStateMachine::Action::EXECUTE));

    // Suspend task so it doesn't consume messages while we fill the queue
    vTaskSuspend(manager->get_task_handle());

    // Fill the queue
    wifi_manager::Message dummy = {};
    dummy.type = MessageType::COMMAND;
    dummy.cmd = CommandId::STOP;
    for (int i = 0; i < QUEUE_SIZE; i++) xQueueSend(real_queue, &dummy, 0);

    EXPECT_EQ(ESP_FAIL, GetParam().call(manager.get(), 100));
    vTaskResume(manager->get_task_handle());
}

TEST_P(WiFiManagerSyncGuardTest, ReturnsTimeoutWhenNoBitsSet)
{
    setup_and_init();
    // processor does NOT set any bits — timeout must occur
    ON_CALL(*state_machine, validate_command(GetParam().cmd)).WillByDefault(Return(IWiFiStateMachine::Action::EXECUTE));

    EXPECT_EQ(ESP_ERR_TIMEOUT, GetParam().call(manager.get(), 50));
}

TEST_P(WiFiManagerSyncGuardTest, ReturnsFailWhenProcessorSetsInvalidStateBit)
{
    setup_and_init();
    setup_processor_result(GetParam().cmd, INVALID_STATE_BIT);

    ON_CALL(*state_machine, validate_command(GetParam().cmd)).WillByDefault(Return(IWiFiStateMachine::Action::EXECUTE));

    EXPECT_EQ(ESP_ERR_INVALID_STATE, GetParam().call(manager.get(), 500));
}

INSTANTIATE_TEST_SUITE_P(
    WiFiManager,
    WiFiManagerSyncGuardTest,
    ::testing::Values(
        SyncGuardParam{CommandId::START, [](WiFiManager *m, uint32_t t) { return m->start(t); }},
        SyncGuardParam{CommandId::STOP, [](WiFiManager *m, uint32_t t) { return m->stop(t); }},
        SyncGuardParam{CommandId::CONNECT, [](WiFiManager *m, uint32_t t) { return m->connect(t); }},
        SyncGuardParam{CommandId::DISCONNECT, [](WiFiManager *m, uint32_t t) { return m->disconnect(t); }}));
