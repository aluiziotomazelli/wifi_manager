# WiFi Manager Case Study: Refactoring for Scale and Legibility

## 1. Context & Problem Analysis
The current `WiFiManager` implementation uses a monolith `wifi_task` with nested `switch` statements to handle states and commands. While functional, it presents several challenges:
- **Cyclomatic Complexity**: Adding a new state or event requires modifying deep nests of `if/switch`.
- **Maintainability**: The task logic spans hundreds of lines, making it hard to reason about side effects.
- **Race Conditions**: Although protected by a mutex, the logic for state transitions is mixed with driver calls, increasing the risk of subtle bugs.

---

## 2. Option 1: Bitwise State Representation

### Concept
Representing states as powers of two allows combining them using bitwise operators.

```cpp
enum class State : uint32_t {
    UNINITIALIZED    = 1 << 0,
    INITIALIZING      = 1 << 1,
    INITIALIZED       = 1 << 2,
    STARTING          = 1 << 3,
    STARTED           = 1 << 4,
    // ...
};

// State Groups (Masks)
static constexpr uint32_t IS_OPERATIONAL = (uint32_t)State::STARTED; // Simplified example
static constexpr uint32_t CAN_CONNECT    = (uint32_t)State::STARTED;
```

### Atomicity & Mutex Analysis
On ESP32 (Xtensa and RISC-V), aligned 32-bit reads and writes are atomic. However:
1. **Read-Modify-Write (RMW)** operations like `state |= State::CONNECTED` are **NOT** atomic. They translate to: load -> bitwise OR -> store.
2. In a dual-core environment (ESP32 standard), if Core 0 is updating the state while Core 1 is checking it, Core 1 might see an intermediate or stale value.
3. C++ `std::atomic<State>` would solve this without a mutex for the variable itself, as it uses the `S32C1I` instruction (Xtensa) or AMO (RISC-V).

> [!IMPORTANT]
> A mutex is still recommended because state transitions often involve multiple steps (e.g., updating the state **and** triggering an event bit). `std::atomic` only protects the single variable.

### Pros/Cons
- ✅ **Pros**: Fast validation (single bitwise mask), flexible state grouping.
- ❌ **Cons**: Loss of strict type safety if casts to `uint32_t` are frequent; doesn't solve the "massive switch" problem by itself.

---

## 3. Option 2: State Transition Table (STT)

### Concept
A table-driven approach separates the **policy** (transition rules) from the **mechanism** (executing code).

```cpp
struct Transition {
    State currentState;
    CommandId command;
    State nextState;
    void (WiFiManager::*handler)(const Command&);
};

static constexpr Transition transition_table[] = {
    {State::INITIALIZED, CommandId::START, State::STARTING, &WiFiManager::handle_start},
    {State::STARTED,     CommandId::CONNECT, State::CONNECTING, &WiFiManager::handle_connect},
};
```

### Constexpr Validation
We can use `constexpr` to find the transition at compile-time (if possible) or at least ensure the table is stored in Flash (RODATA).

```cpp
// Example: Compile-time check for state existence
template<State... States>
struct StateChecker {
    static constexpr bool contains(State s) {
        return ((s == States) || ...);
    }
};

using OperationalStates = StateChecker<State::STARTED, State::CONNECTED_GOT_IP>;

static_assert(OperationalStates::contains(State::STARTED), "Faulty state check logic!");
```

### Pros/Cons
- ✅ **Pros**: Declarative logic (easy to see all rules in one place), drastic reduction in `switch` complexity.
- ❌ **Cons**: Requires mapping functions to handlers; member function pointers can be syntax-heavy.

---

## 4. Option 3: Command Dispatcher Pattern

### Concept
Instead of a giant `switch`, the `wifi_task` simply dispatches the command to a handler based on a map or array.

```cpp
using HandlerFunc = void (WiFiManager::*)(const Command&);
static const HandlerFunc handlers[] = {
    [CommandId::START]      = &WiFiManager::handle_start,
    [CommandId::CONNECT]    = &WiFiManager::handle_connect,
};
```
Inside the handler, we check the current state using a bitmask (Option 1).

---

## 5. Comparison Matrix

| Feature | Current | Option 1 (Bits) | Option 2 (STT) | Option 3 (Dispatcher) |
| :--- | :--- | :--- | :--- | :--- |
| **Legibility** | Low | Medium | High | High |
| **Ease of Extension** | Hard | Medium | Easy | Easy |
| **Memory usage** | Low | Very Low | Low (Flash) | Low (Flash) |
| **Performance** | High | Very High | High | High |
| **Type Safety** | High | Low | High | Medium |

---

## 6. Recommendation

The most robust approach for `WiFiManager` is a **Hybrid Approach**:

1. **Bitwise State** for internal grouping (validation).
2. **Command Dispatcher** to remove the nested switches.
3. **State Guard Method** to validate transitions before execution.

This keeps the code clean, avoids dynamic allocation, and leverages the ESP32's strengths while keeping thread safety simple with the existing recursive mutex.
