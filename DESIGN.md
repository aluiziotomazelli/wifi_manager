# WiFiManager Internal Design

This document explains the internal architecture, message flow, and state machine logic of the `WiFiManager` component.

## Architecture Overview

The `WiFiManager` follows a **Message-Driven State Machine** pattern. It uses a single FreeRTOS task (`wifi_task`) to serialize all operations, ensuring thread safety without heavy locking.

### Key Components

1.  **Unified Message Queue**: A FreeRTOS queue (`command_queue`) that receives both `MessageType::COMMAND` (from API) and `MessageType::EVENT` (from system callbacks).
2.  **Lookup Tables (LUTs)**:
    *   `command_matrix`: Validates if a command (e.g., `CONNECT`) is allowed in the current state.
    *   `transition_matrix`: Defines the "GOTO" logic for system events (e.g., if `STA_CONNECTED` arrives while `CONNECTING`, go to `CONNECTED_NO_IP`).
3.  **Synchronization**: An Event Group handles the handshake between the asynchronous task and the synchronous API callers.

---

## Message Flows

### 1. Synchronous Command Flow (e.g., `connect(timeout)`)

When a user calls a synchronous API method:

1.  **Validation**: The API call checks `command_matrix[current_state][COMMAND]`. If invalid, it returns immediately.
2.  **Preparation**: It clears relevant bits in the `wifi_event_group` (e.g., `CONNECTED_BIT`).
3.  **Posting**: It calls `post_message(is_async=false)`. This sends the command to the queue.
4.  **Blocking**: The API call then calls `xEventGroupWaitBits`, blocking the caller's task until the internal `wifi_task` signals completion.
5.  **Task Processing**: 
    *   `wifi_task` wakes up, receives the `COMMAND` message.
    *   It dispatches to the handler (e.g., `handle_connect`).
    *   The handler calls the actual ESP-IDF driver (e.g., `esp_wifi_connect`).
6.  **Feedback**: 
    *   If the driver call fails, the handler immediately sets the `CONNECT_FAILED_BIT`.
    *   If successful, the task waits for system events.
7.  **Resolution**: When the bits are set, the API caller task unblocks and returns the result to the user.

### 2. Asynchronous Command Flow (e.g., `connect()`)

Similar to the synchronous flow, but skips the blocking step:

1.  **Validation & Posting**: Same as above, but `post_message(is_async=true)` is used.
2.  **Immediate Return**: The API returns `ESP_OK` immediately after the message is successfully queued.
3.  **Background Processing**: The `wifi_task` processes the command in the background. The user is expected to check state or use a separate synchronization mechanism if they need to know when it finishes.

### 3. System Event Flow (e.g., `STA_CONNECTED`)

When a driver event occurs (e.g., link established):

1.  **Bridge Handler**: The `wifi_event_handler` (static callback) is triggered by the ESP-IDF event loop.
2.  **ISR-Safe Posting**: It packs the event data into a `Message` (`type=EVENT`) and calls `xQueueSendFromISR`. This is extremely fast and non-blocking.
3.  **Dispatcher**: The `wifi_task` wakes up and calls `process_message` -> `handle_event`.
4.  **Matrix Resolution**: 
    *   `handle_event` calls `resolve_event(event, current_state)`.
    *   It looks up the result in the `transition_matrix`.
    *   It updates correctly to the **Next State**.
5.  **Auto-Signaling**: If the matrix defines `bits_to_set` (e.g., `CONNECTED_BIT`), they are set automatically.
6.  **Side Effects**: The code then enters a `switch` statement to handle logic that the matrix can't capture (like starting the backoff timer on a disconnect or logging RSSI quality).

---

## State Machine Diagram (Simplified)

```mermaid
state_diagram
    [*] --> INITIALIZED
    INITIALIZED --> STARTING : START command
    STARTING --> STARTED : STA_START event
    STARTED --> CONNECTING : CONNECT command
    CONNECTING --> CONNECTED_NO_IP : STA_CONNECTED event
    CONNECTED_NO_IP --> CONNECTED_GOT_IP : GOT_IP event
    CONNECTED_GOT_IP --> WAITING_RECONNECT : STA_DISCONNECTED (unintended)
    WAITING_RECONNECT --> CONNECTING : Timer expired
    CONNECTED_GOT_IP --> DISCONNECTING : DISCONNECT command
    DISCONNECTING --> STARTED : STA_DISCONNECTED event
    STARTED --> STOPPING : STOP command
    STOPPING --> INITIALIZED : STA_STOP event
```
