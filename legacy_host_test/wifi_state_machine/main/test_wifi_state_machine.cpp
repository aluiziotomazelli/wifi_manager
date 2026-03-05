#include "unity.h"
#include "wifi_state_machine.hpp"
#include "freertos/FreeRTOS.h"
#include "host_test_common.hpp"

void setUp(void)
{
    host_test_setup_common_mocks();
}

void tearDown(void)
{
}

TEST_CASE("WiFiStateMachine: Initial State", "[wifi_fsm]")
{
    WiFiStateMachine fsm;
    TEST_ASSERT_EQUAL(WiFiStateMachine::State::UNINITIALIZED, fsm.get_current_state());
}

TEST_CASE("WiFiStateMachine: Transition to INITIALIZED", "[wifi_fsm]")
{
    WiFiStateMachine fsm;
    fsm.transition_to(WiFiStateMachine::State::INITIALIZED);
    TEST_ASSERT_EQUAL(WiFiStateMachine::State::INITIALIZED, fsm.get_current_state());
}

TEST_CASE("WiFiStateMachine: Command Validation", "[wifi_fsm]")
{
    WiFiStateMachine fsm;

    // In UNINITIALIZED, START should be ERROR
    TEST_ASSERT_EQUAL(WiFiStateMachine::Action::ERROR, fsm.validate_command(WiFiStateMachine::CommandId::START));

    fsm.transition_to(WiFiStateMachine::State::INITIALIZED);
    // In INITIALIZED, START should be EXECUTE
    TEST_ASSERT_EQUAL(WiFiStateMachine::Action::EXECUTE, fsm.validate_command(WiFiStateMachine::CommandId::START));
    // In INITIALIZED, STOP should be SKIP
    TEST_ASSERT_EQUAL(WiFiStateMachine::Action::SKIP, fsm.validate_command(WiFiStateMachine::CommandId::STOP));
}

TEST_CASE("WiFiStateMachine: Event Resolution", "[wifi_fsm]")
{
    WiFiStateMachine fsm;
    fsm.transition_to(WiFiStateMachine::State::STARTING);

    auto outcome = fsm.resolve_event(WiFiStateMachine::EventId::STA_START);
    TEST_ASSERT_EQUAL(WiFiStateMachine::State::STARTED, outcome.next_state);
}

TEST_CASE("WiFiStateMachine: Suspect Failure Handling (Dynamic RSSI)", "[wifi_fsm]")
{
    WiFiStateMachine fsm;

    printf("Testing Good Signal (-50 dBm) -> limit 1\n");
    fsm.reset_retries();
    fsm.transition_to(WiFiStateMachine::State::CONNECTING);
    TEST_ASSERT_TRUE(fsm.handle_suspect_failure(-50));
    TEST_ASSERT_EQUAL(WiFiStateMachine::State::ERROR_CREDENTIALS, fsm.get_current_state());

    printf("Testing Medium Signal (-60 dBm) -> limit 2\n");
    fsm.reset_retries();
    fsm.transition_to(WiFiStateMachine::State::CONNECTING);
    TEST_ASSERT_FALSE(fsm.handle_suspect_failure(-60));
    TEST_ASSERT_TRUE(fsm.handle_suspect_failure(-60));
    TEST_ASSERT_EQUAL(WiFiStateMachine::State::ERROR_CREDENTIALS, fsm.get_current_state());

    printf("Testing Weak Signal (-75 dBm) -> limit 5\n");
    fsm.reset_retries();
    fsm.transition_to(WiFiStateMachine::State::CONNECTING);
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_FALSE(fsm.handle_suspect_failure(-75));
    }
    TEST_ASSERT_TRUE(fsm.handle_suspect_failure(-75));
    TEST_ASSERT_EQUAL(WiFiStateMachine::State::ERROR_CREDENTIALS, fsm.get_current_state());

    printf("Testing Critical Signal (-85 dBm) -> infinite\n");
    fsm.reset_retries();
    fsm.transition_to(WiFiStateMachine::State::CONNECTING);
    for (int i = 0; i < 50; i++) {
        TEST_ASSERT_FALSE(fsm.handle_suspect_failure(-85));
    }
    TEST_ASSERT_EQUAL(WiFiStateMachine::State::CONNECTING, fsm.get_current_state());
}

TEST_CASE("WiFiStateMachine: Backoff Calculation", "[wifi_fsm]")
{
    WiFiStateMachine fsm;
    uint32_t delay;

    fsm.calculate_next_backoff(delay);
    TEST_ASSERT_EQUAL(1000, delay); // 2^0 * 1000
    TEST_ASSERT_EQUAL(WiFiStateMachine::State::WAITING_RECONNECT, fsm.get_current_state());

    fsm.calculate_next_backoff(delay);
    TEST_ASSERT_EQUAL(2000, delay); // 2^1 * 1000

    fsm.reset_retries();
    fsm.calculate_next_backoff(delay);
    TEST_ASSERT_EQUAL(1000, delay);
}

TEST_CASE("WiFiStateMachine: Get Wait Ticks", "[wifi_fsm]")
{
    WiFiStateMachine fsm;

    // Default state: not waiting
    TEST_ASSERT_EQUAL(portMAX_DELAY, fsm.get_wait_ticks());

    // Transition to WAITING_RECONNECT
    fsm.transition_to(WiFiStateMachine::State::WAITING_RECONNECT);

    // Initial calculation (should be valid)
    uint32_t delay;
    fsm.calculate_next_backoff(delay);

    TickType_t ticks = fsm.get_wait_ticks();
    TEST_ASSERT_TRUE(ticks > 0);
    TEST_ASSERT_TRUE(ticks <= pdMS_TO_TICKS(1000));
}
