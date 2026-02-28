#include "wifi_state_machine.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "interfaces/i_timer_hal.hpp"

using namespace wifi_manager;
using State = wifi_manager::State;
using WFAction = WiFiStateMachine::Action;
using namespace testing;

class MockTimerHAL : public ITimerHAL
{
public:
    MOCK_METHOD(uint64_t, get_time_ms, (), (const, override));
};

// Parameters for the CommandValidationTest
struct CommandValidationParam
{
    State state;
    CommandId command;
    WFAction expected;
};

class CommandValidationTest : public ::testing::TestWithParam<CommandValidationParam>
{
protected:
    MockTimerHAL timer;
    WiFiStateMachine sm{timer};
};

TEST_P(CommandValidationTest, ValidateCommand)
{
    auto [state, command, expected] = GetParam();
    sm.transition_to(state);
    EXPECT_EQ(expected, sm.validate_command(command));
}

INSTANTIATE_TEST_SUITE_P(
    WiFiStateMachine,
    CommandValidationTest,
    ::testing::Values(

        // COUNT - Out of range Command
        CommandValidationParam{State::UNINITIALIZED, CommandId::COUNT, WFAction::EXECUTE},

        // UNINITIALIZED - all commands should be ERROR
        CommandValidationParam{State::UNINITIALIZED, CommandId::START, WFAction::ERROR},
        CommandValidationParam{State::UNINITIALIZED, CommandId::STOP, WFAction::ERROR},
        CommandValidationParam{State::UNINITIALIZED, CommandId::CONNECT, WFAction::ERROR},
        CommandValidationParam{State::UNINITIALIZED, CommandId::DISCONNECT, WFAction::ERROR},
        CommandValidationParam{State::UNINITIALIZED, CommandId::EXIT, WFAction::ERROR},

        // INITIALIZING - all commands should be ERROR
        CommandValidationParam{State::INITIALIZING, CommandId::START, WFAction::ERROR},
        CommandValidationParam{State::INITIALIZING, CommandId::STOP, WFAction::ERROR},
        CommandValidationParam{State::INITIALIZING, CommandId::CONNECT, WFAction::ERROR},
        CommandValidationParam{State::INITIALIZING, CommandId::DISCONNECT, WFAction::ERROR},
        CommandValidationParam{State::INITIALIZING, CommandId::EXIT, WFAction::ERROR},

        // INITIALIZED - only START is EXECUTE
        CommandValidationParam{State::INITIALIZED, CommandId::START, WFAction::EXECUTE},
        CommandValidationParam{State::INITIALIZED, CommandId::STOP, WFAction::SKIP},
        CommandValidationParam{State::INITIALIZED, CommandId::CONNECT, WFAction::ERROR},
        CommandValidationParam{State::INITIALIZED, CommandId::DISCONNECT, WFAction::ERROR},
        CommandValidationParam{State::INITIALIZED, CommandId::EXIT, WFAction::ERROR},

        // STARTING - only STOP is valid
        CommandValidationParam{State::STARTING, CommandId::START, WFAction::SKIP},
        CommandValidationParam{State::STARTING, CommandId::STOP, WFAction::EXECUTE},
        CommandValidationParam{State::STARTING, CommandId::CONNECT, WFAction::ERROR},
        CommandValidationParam{State::STARTING, CommandId::DISCONNECT, WFAction::ERROR},
        CommandValidationParam{State::STARTING, CommandId::EXIT, WFAction::ERROR},

        // STARTED - only STOP and CONNECT are valid
        CommandValidationParam{State::STARTED, CommandId::START, WFAction::SKIP},
        CommandValidationParam{State::STARTED, CommandId::STOP, WFAction::EXECUTE},
        CommandValidationParam{State::STARTED, CommandId::CONNECT, WFAction::EXECUTE},
        CommandValidationParam{State::STARTED, CommandId::DISCONNECT, WFAction::SKIP},
        CommandValidationParam{State::STARTED, CommandId::EXIT, WFAction::ERROR},

        // CONNECTING - only STOP and DISCONNECT are valid
        CommandValidationParam{State::CONNECTING, CommandId::START, WFAction::SKIP},
        CommandValidationParam{State::CONNECTING, CommandId::STOP, WFAction::EXECUTE},
        CommandValidationParam{State::CONNECTING, CommandId::CONNECT, WFAction::SKIP},
        CommandValidationParam{State::CONNECTING, CommandId::DISCONNECT, WFAction::EXECUTE},
        CommandValidationParam{State::CONNECTING, CommandId::EXIT, WFAction::ERROR},
        CommandValidationParam{State::CONNECTING, CommandId::COUNT, WFAction::EXECUTE},

        // CONNECTED_NO_IP - only STOP and DISCONNECT are valid
        CommandValidationParam{State::CONNECTED_NO_IP, CommandId::START, WFAction::SKIP},
        CommandValidationParam{State::CONNECTED_NO_IP, CommandId::STOP, WFAction::EXECUTE},
        CommandValidationParam{State::CONNECTED_NO_IP, CommandId::CONNECT, WFAction::SKIP},
        CommandValidationParam{State::CONNECTED_NO_IP, CommandId::DISCONNECT, WFAction::EXECUTE},
        CommandValidationParam{State::CONNECTED_NO_IP, CommandId::EXIT, WFAction::ERROR},

        // CONNECTED_GOT_IP - only STOP and DISCONNECT are valid
        CommandValidationParam{State::CONNECTED_GOT_IP, CommandId::START, WFAction::SKIP},
        CommandValidationParam{State::CONNECTED_GOT_IP, CommandId::STOP, WFAction::EXECUTE},
        CommandValidationParam{State::CONNECTED_GOT_IP, CommandId::CONNECT, WFAction::SKIP},
        CommandValidationParam{State::CONNECTED_GOT_IP, CommandId::DISCONNECT, WFAction::EXECUTE},
        CommandValidationParam{State::CONNECTED_GOT_IP, CommandId::EXIT, WFAction::ERROR},

        // DISCONNECTING - only STOP and DISCONNECT are valid
        CommandValidationParam{State::DISCONNECTING, CommandId::START, WFAction::SKIP},
        CommandValidationParam{State::DISCONNECTING, CommandId::STOP, WFAction::EXECUTE},
        CommandValidationParam{State::DISCONNECTING, CommandId::CONNECT, WFAction::ERROR},
        CommandValidationParam{State::DISCONNECTING, CommandId::DISCONNECT, WFAction::SKIP},
        CommandValidationParam{State::DISCONNECTING, CommandId::EXIT, WFAction::ERROR},

        // WAITING_RECONNECT
        CommandValidationParam{State::WAITING_RECONNECT, CommandId::START, WFAction::SKIP},
        CommandValidationParam{State::WAITING_RECONNECT, CommandId::STOP, WFAction::EXECUTE},
        CommandValidationParam{State::WAITING_RECONNECT, CommandId::CONNECT, WFAction::EXECUTE},
        CommandValidationParam{State::WAITING_RECONNECT, CommandId::DISCONNECT, WFAction::EXECUTE},
        CommandValidationParam{State::WAITING_RECONNECT, CommandId::EXIT, WFAction::ERROR},

        // ERROR_CREDENTIALS
        CommandValidationParam{State::ERROR_CREDENTIALS, CommandId::START, WFAction::SKIP},
        CommandValidationParam{State::ERROR_CREDENTIALS, CommandId::STOP, WFAction::EXECUTE},
        CommandValidationParam{State::ERROR_CREDENTIALS, CommandId::CONNECT, WFAction::EXECUTE},
        CommandValidationParam{State::ERROR_CREDENTIALS, CommandId::DISCONNECT, WFAction::EXECUTE},
        CommandValidationParam{State::ERROR_CREDENTIALS, CommandId::EXIT, WFAction::ERROR},

        // STOPPING
        CommandValidationParam{State::STOPPING, CommandId::START, WFAction::ERROR},
        CommandValidationParam{State::STOPPING, CommandId::STOP, WFAction::SKIP},
        CommandValidationParam{State::STOPPING, CommandId::CONNECT, WFAction::ERROR},
        CommandValidationParam{State::STOPPING, CommandId::DISCONNECT, WFAction::ERROR},
        CommandValidationParam{State::STOPPING, CommandId::EXIT, WFAction::ERROR}

        ));

TEST(WiFiStateMachineTest, InitialState)
{
    MockTimerHAL timer;
    WiFiStateMachine sm(timer);
    EXPECT_EQ(State::UNINITIALIZED, sm.get_current_state());
    EXPECT_EQ(0, sm.get_retry_count());
    EXPECT_EQ(0, sm.get_next_reconnect_ms());
}

TEST(WiFiStateMachineTest, TransitionToInitialised)
{
    MockTimerHAL timer;
    WiFiStateMachine sm(timer);
    sm.transition_to(State::INITIALIZED);
    EXPECT_EQ(State::INITIALIZED, sm.get_current_state());
}

TEST(WiFiStateMachineTest, EventResolution)
{
    MockTimerHAL timer;
    WiFiStateMachine sm(timer);
    sm.transition_to(State::STARTING);
    auto outcome = sm.resolve_event(EventId::STA_START);
    EXPECT_EQ(State::STARTED, outcome.next_state);
}

TEST(WiFiStateMachineTest, HandleOutOfRangeEvent)
{
    MockTimerHAL timer;
    WiFiStateMachine sm(timer);
    sm.transition_to(State::STARTED);
    auto outcome = sm.resolve_event(EventId::COUNT); // out of range event
    EXPECT_EQ(State::STARTED, outcome.next_state);   // should not change state
}

TEST(WiFiStateMachineTest, SuspectFailureHandlingHighRssi)
{
    MockTimerHAL timer;
    WiFiStateMachine sm(timer);

    sm.reset_retries();
    sm.transition_to(State::CONNECTING);
    EXPECT_TRUE(sm.handle_suspect_failure(WiFiStateMachine::RSSI_THRESHOLD_GOOD + 5));
    EXPECT_EQ(State::ERROR_CREDENTIALS, sm.get_current_state());
}

TEST(WiFiStateMachineTest, SuspectFailureHandlingMediumRssi)
{
    MockTimerHAL timer;
    WiFiStateMachine sm(timer);

    sm.reset_retries();
    sm.transition_to(State::CONNECTING);
    for (int i = 0; i < WiFiStateMachine::RETRY_LIMIT_MEDIUM - 1; i++) {
        EXPECT_FALSE(sm.handle_suspect_failure(WiFiStateMachine::RSSI_THRESHOLD_MEDIUM + 5));
    }
    EXPECT_TRUE(sm.handle_suspect_failure(WiFiStateMachine::RSSI_THRESHOLD_MEDIUM + 5));
    EXPECT_EQ(State::ERROR_CREDENTIALS, sm.get_current_state());
}

TEST(WiFiStateMachineTest, SuspectFailureHandlingLowRssi)
{
    MockTimerHAL timer;
    WiFiStateMachine sm(timer);

    sm.reset_retries();
    sm.transition_to(State::CONNECTING);
    for (int i = 0; i < WiFiStateMachine::RETRY_LIMIT_WEAK - 1; i++) {
        EXPECT_FALSE(sm.handle_suspect_failure(WiFiStateMachine::RSSI_THRESHOLD_WEAK + 5));
    }
    EXPECT_TRUE(sm.handle_suspect_failure(WiFiStateMachine::RSSI_THRESHOLD_WEAK + 5));
    EXPECT_EQ(State::ERROR_CREDENTIALS, sm.get_current_state());
}

TEST(WiFiStateMachineTest, SuspectFailureHandlingVeryCriticalRssi)
{
    MockTimerHAL timer;
    WiFiStateMachine sm(timer);

    sm.reset_retries();
    sm.transition_to(State::CONNECTING);
    for (int i = 0; i < 50; i++) {
        EXPECT_FALSE(sm.handle_suspect_failure(WiFiStateMachine::RSSI_THRESHOLD_WEAK - 5));
    }
    EXPECT_EQ(State::CONNECTING, sm.get_current_state());
}

TEST(WiFiStateMachineTest, BackoffCalculation)
{
    MockTimerHAL timer;
    WiFiStateMachine sm(timer);
    uint32_t delay;

    EXPECT_CALL(timer, get_time_ms()).WillRepeatedly(Return(0));

    sm.calculate_next_backoff(delay);
    EXPECT_EQ(1000, delay); // 2^0 * 1000
    EXPECT_EQ(State::WAITING_RECONNECT, sm.get_current_state());

    sm.calculate_next_backoff(delay);
    EXPECT_EQ(2000, delay); // 2^1 * 1000
    EXPECT_EQ(State::WAITING_RECONNECT, sm.get_current_state());

    sm.calculate_next_backoff(delay);
    EXPECT_EQ(4000, delay); // 2^2 * 1000
    EXPECT_EQ(State::WAITING_RECONNECT, sm.get_current_state());

    sm.reset_retries();
    sm.calculate_next_backoff(delay);
    EXPECT_EQ(1000, delay);
}

TEST(WiFiStateMachineTest, BackoffMaximumReached)
{
    MockTimerHAL timer;
    WiFiStateMachine sm(timer);
    uint32_t delay;

    EXPECT_CALL(timer, get_time_ms()).WillRepeatedly(Return(0));

    for (int i = 0; i < WiFiStateMachine::MAX_BACKOFF_EXPONENT + 2; i++) {
        sm.calculate_next_backoff(delay);
        EXPECT_EQ(State::WAITING_RECONNECT, sm.get_current_state());
        if (i < WiFiStateMachine::MAX_BACKOFF_EXPONENT) {
            EXPECT_EQ(1000 * (1 << i), delay);
        }
        else {
            EXPECT_EQ(WiFiStateMachine::MAX_BACKOFF_MS, delay);
        }
    }
}

TEST(WiFiStateMachineTest, GetWaitMs)
{
    MockTimerHAL timer;
    WiFiStateMachine sm(timer);

    // Default state: not waiting
    EXPECT_EQ(0xFFFFFFFF, sm.get_wait_ms());

    // Transition to WAITING_RECONNECT
    sm.transition_to(State::WAITING_RECONNECT);

    // Initial calculation (should be valid)
    uint32_t delay;
    EXPECT_CALL(timer, get_time_ms()).WillOnce(Return(1000));
    sm.calculate_next_backoff(delay); // next_reconnect = 1000 + 1000 = 2000

    EXPECT_CALL(timer, get_time_ms()).WillOnce(Return(1500));
    uint32_t wait = sm.get_wait_ms();
    EXPECT_EQ(500, wait); // 2000 - 1500
}

TEST(WiFiStateMachineTest, GetWaitMsExpired)
{
    MockTimerHAL timer;
    WiFiStateMachine sm(timer);

    sm.transition_to(State::WAITING_RECONNECT);
    uint32_t delay;
    EXPECT_CALL(timer, get_time_ms()).WillOnce(Return(1000));
    sm.calculate_next_backoff(delay); // next_reconnect = 2000

    // now > deadline: tempo já expirou
    EXPECT_CALL(timer, get_time_ms()).WillOnce(Return(3000));
    EXPECT_EQ(0, sm.get_wait_ms()); // branch L219 = false → return 0
}

TEST(WiFiStateMachineTest, GetWaitMsOverflow)
{
    MockTimerHAL timer;
    WiFiStateMachine sm(timer);

    sm.transition_to(State::WAITING_RECONNECT);
    uint32_t delay;
    // next_reconnect = 0 + (1<<MAX_BACKOFF_EXPONENT)*1000 may be insufficient
    // Force next_reconnect_ms_ very high via backoff with now=0x100000000
    EXPECT_CALL(timer, get_time_ms()).WillOnce(Return(0x100000000ULL));
    sm.calculate_next_backoff(delay); // next_reconnect = 0x100000000 + MAX_BACKOFF_MS

    EXPECT_CALL(timer, get_time_ms()).WillOnce(Return(0));
    EXPECT_EQ(0xFFFFFFFF, sm.get_wait_ms()); // wait_ms > 32 bits → clamp
}

TEST(WiFiStateMachineTest, IsStaReady)
{
    MockTimerHAL timer;
    WiFiStateMachine sm(timer);
    sm.transition_to(State::STARTED);
    EXPECT_TRUE(sm.is_sta_ready());
}

TEST(WiFiStateMachineTest, IsStaReadyNotReady)
{
    MockTimerHAL timer;
    WiFiStateMachine sm(timer);
    sm.transition_to(State::STARTING);
    EXPECT_FALSE(sm.is_sta_ready());
}

TEST(WiFiStateMachineTest, IsActive)
{
    MockTimerHAL timer;
    WiFiStateMachine sm(timer);
    sm.transition_to(State::STARTED);
    EXPECT_TRUE(sm.is_active());
}

TEST(WiFiStateMachineTest, IsActiveNotActive)
{
    MockTimerHAL timer;
    WiFiStateMachine sm(timer);
    sm.transition_to(State::INITIALIZED);
    EXPECT_FALSE(sm.is_active());
}