# Ferry Simulation (19)

A multi-process ferry terminal simulation system implementing synchronization mechanisms using System V IPC (Inter-Process Communication) primitives. The simulation models passengers traveling through a terminal with security screening and boarding onto ferries.

## Project Overview

This project simulates a ferry terminal where:
- **Passengers** undergo baggage checks, security screening, and board ferries
- **Ferry Managers** control individual ferries, manage boarding, and handle departures
- **Port Manager** orchestrates the entire simulation, spawning processes and managing lifecycle
- **Security Manager** handles gender-segregated security stations with frustration mechanisms

The simulation demonstrates advanced IPC concepts including message queues, shared memory, and semaphores for process synchronization.

## Building the Project

This project uses the **Meson** build system.

### Prerequisites

- GCC or compatible C compiler
- Meson build system (>= 1.3.0)
- Ninja build tool
- Python 3 (for tests)

### Build Instructions

1. **Configure the build directory:**
   ```bash
   meson setup buildDir
   ```

2. **Compile the project:**
   ```bash
   meson compile -C buildDir
   ```

3. **Run tests (optional):**
   ```bash
   meson setup buildDir -Denable-tests=true
   meson test -C buildDir
   ```

### Build Outputs

The build process generates the following executables in `buildDir/`:
- `ferry-simulation` - Main entry point that initializes IPC and spawns processes
- `port-manager` - Manages ferry and passenger process lifecycle
- `ferry-manager` - Controls individual ferry operations
- `passenger` - Simulates individual passenger behavior

## Running the Simulation

```bash
./buildDir/ferry-simulation
```

The simulation will:
1. Initialize all IPC resources (message queues, shared memory, semaphores)
2. Spawn ferry manager and passenger processes
3. Run until all passengers have boarded or port closes
4. Clean up IPC resources

**Termination:** Press `Ctrl+C` to gracefully shut down all processes.

**Output:** All events are logged to `simulation.log` with timestamps.

## Architecture

### Process Structure

```
ferry-simulation (main)
├── port-manager
│   ├── security-manager (thread/fork)
│   ├── ferry-manager (ferry 0)
│   ├── ferry-manager (ferry 1)
│   ├── ...
│   ├── passenger (ID 0)
│   ├── passenger (ID 1)
│   └── ...
```

### Workflow

1. **Initialization** ([main.c](src/processes/main.c#L1-L200)):
   - Creates IPC resources (queues, shared memory, semaphores)
   - Initializes ferry states with different baggage limits
   - Spawns port manager process

2. **Passenger Journey** ([passenger.c](src/processes/passenger.c#L40-L250)):
   - **Check-in**: Generate random attributes (gender, VIP status, bag weight)
   - **Baggage Check**: Wait for ferry with acceptable baggage limit
   - **Security Screening**: Gender-segregated stations with frustration mechanism
   - **Ramp Queue**: VIP priority boarding
   - **Boarding**: Walk onto ferry, signal completion

3. **Ferry Operations** ([ferry_manager.c](src/processes/ferry_manager.c#L30-L249)):
   - Wait for turn to dock
   - Open boarding gate (random delay)
   - Process ramp queue (VIP priority)
   - Depart when full or timer expires
   - Travel, return, repeat

4. **Security Management** ([port_manager.c](src/processes/port_manager.c#L267-L380)):
   - 3 stations, 2 capacity each, gender-segregated
   - Frustration counter: if overtaken 3 times, block until slot available
   - Random screening time (2-5 seconds)

## IPC Structures

### 1. Message Queues

The system uses three message queues for asynchronous communication between processes.

#### Security Queue ([ipc.h](include/common/ipc.h#L14))

**Purpose:** Communication between passengers and security manager.

**Key:** Generated from `IPC_KEY_QUEUE_SECURITY_ID` ('s')

**Message Structure:** ([messages.h](include/common/messages.h#L8-L14))
```c
typedef struct SecurityMessage {
    long mtype;         // 1 for manager, <pid> for passenger response
    Gender gender;      // GENDER_MAN or GENDER_WOMAN
    long pid;           // Passenger process ID
    int passenger_id;   // Passenger identifier
    int frustration;    // Overtaken count (0-3)
} SecurityMessage;
```

**Usage:**
- Passenger sends request with `mtype=1` ([passenger.c](src/processes/passenger.c#L167-L174))
- Security manager receives and processes ([port_manager.c](src/processes/port_manager.c#L311-L318))
- Manager responds with `mtype=<passenger_pid>` ([port_manager.c](src/processes/port_manager.c#L367-L371))

**Operations:**
```c
// Passenger requesting security screening
security_message.mtype = SECURITY_MESSAGE_MANAGER_ID;  // 1
security_message.gender = ticket.gender;
security_message.pid = getpid();
msgsnd(queue_security, &security_message, MSG_SIZE(security_message), 0);

// Passenger waiting for completion
msgrcv(queue_security, &security_message, MSG_SIZE(security_message), getpid(), 0);
```

#### Ramp Queue ([ipc.h](include/common/ipc.h#L15))

**Purpose:** Boarding coordination between passengers and ferry managers.

**Key:** Generated from `IPC_KEY_QUEUE_RAMP_ID` ('r')

**Message Structure:** ([messages.h](include/common/messages.h#L21-L27))
```c
typedef struct RampMessage {
    long mtype;         // 1=exit, 2=VIP, 3=Regular, or PID for response
    long pid;           // Passenger PID
    int passenger_id;   // Passenger identifier
    int weight;         // Baggage weight
    int is_vip;         // VIP status flag
} RampMessage;
```

**Message Types:**
- `RAMP_MESSAGE_EXIT (1)`: Passenger leaving ramp ([passenger.c](src/processes/passenger.c#L238-L245))
- `RAMP_PRIORITY_VIP (2)`: VIP boarding request (higher priority)
- `RAMP_PRIORITY_REGULAR (3)`: Regular boarding request

**Usage:**
- Passenger sends boarding request ([passenger.c](src/processes/passenger.c#L210-L218))
- Ferry manager processes with priority (VIP first) ([ferry_manager.c](src/processes/ferry_manager.c#L170-L194))
- Ferry responds with `mtype=<passenger_pid>`
- Passenger exits ramp after boarding ([passenger.c](src/processes/passenger.c#L238-L245))

**Priority Reception:**
```c
// Ferry receives messages in priority order: exit(1), VIP(2), regular(3)
msgrcv(queue_ramp, &ramp_msg, MSG_SIZE(ramp_msg), -RAMP_PRIORITY_REGULAR, IPC_NOWAIT);
```

#### Log Queue ([ipc.h](include/common/ipc.h#L13))

**Purpose:** Centralized logging from all processes.

**Key:** Generated from `IPC_KEY_LOG_ID` ('l')

**Message Structure:** ([messages.h](include/common/messages.h#L29-L35))
```c
typedef struct LogMessage {
    long mtype;
    int identifier;     // Process/ferry ID
    time_t timestamp;
    char message[1024];
} LogMessage;
```

**Operations** ([logging.c](src/common/logging.c)):
- All processes send log messages to queue
- Main process reads and writes to `simulation.log`

### 2. Shared Memory

#### Shared State ([state.h](include/common/state.h#L29-L34))

**Purpose:** Global simulation state accessible by all processes.

**Key:** Generated from `IPC_KEY_SHM_ID` ('S')

**Structure:**
```c
typedef struct SharedState {
    int port_open;              // Port open/closed flag
    int current_ferry_id;       // Currently docked ferry (-1 if none)
    FerryState ferries[FERRY_COUNT];  // All ferry states
    SimulationStats stats;      // Simulation statistics
} SharedState;
```

**Ferry State:** ([state.h](include/common/state.h#L13-L19))
```c
typedef struct FerryState {
    int ferry_id;
    int baggage_limit;          // Max baggage weight per passenger
    int passenger_count;        // Current passenger count
    int baggage_weight_total;   // Total baggage weight
    FerryStatus status;         // WAITING/BOARDING/DEPARTED/TRAVELING
} FerryState;
```

**Usage:**
- Initialized in [main.c](src/processes/main.c#L141-L159)
- Passengers check baggage limits ([passenger.c](src/processes/passenger.c#L138-L149))
- Ferries update passenger counts ([ferry_manager.c](src/processes/ferry_manager.c#L179-L184))
- Port manager monitors state ([port_manager.c](src/processes/port_manager.c#L27-L32))

**Access Pattern:**
```c
// Attach shared memory
shared_state = (SharedState*)shm_attach(shm_id);

// Read current ferry baggage limit (protected by semaphore)
sem_wait_single(sem_state_mutex, SEM_STATE_MUTEX_VARIANT_CURRENT_FERRY);
int limit = shared_state->ferries[shared_state->current_ferry_id].baggage_limit;
sem_signal_single(sem_state_mutex, SEM_STATE_MUTEX_VARIANT_CURRENT_FERRY);
```

### 3. Semaphores

#### State Mutex Semaphore Set ([ipc.h](include/common/ipc.h#L18))

**Purpose:** Mutual exclusion for shared memory regions.

**Key:** Generated from `IPC_KEY_SEM_STATE_ID` ('M')

**Semaphore Count:** 3

**Variants** ([ipc.h](include/common/ipc.h#L32-L38)):
```c
typedef enum SemStateMutexVariant {
    SEM_STATE_MUTEX_VARIANT_PORT,           // [0] Port state
    SEM_STATE_MUTEX_VARIANT_CURRENT_FERRY,  // [1] Current ferry ID
    SEM_STATE_MUTEX_VARIANT_FERRIES_STATE,  // [2] Ferry states array
    SEM_STATE_MUTEX_VARIANT_STATS,          // [3] Statistics
    SEM_STATE_MUTEX_VARIANT_COUNT
} SemStateMutexVariant;
```

**Operations** ([ipc.c](src/common/ipc.c#L156-L167)):
```c
// Lock critical section
sem_wait_single(sem_id, SEM_STATE_MUTEX_VARIANT_FERRIES_STATE);
// ... access shared_state->ferries[] ...
// Unlock
sem_signal_single(sem_id, SEM_STATE_MUTEX_VARIANT_FERRIES_STATE);
```

**Usage Examples:**
- Protecting current ferry access ([passenger.c](src/processes/passenger.c#L136-L151))
- Updating ferry passenger count ([ferry_manager.c](src/processes/ferry_manager.c#L179-L184))
- Port closure coordination ([port_manager.c](src/processes/port_manager.c#L27-L32))

#### Security Semaphore ([ipc.h](include/common/ipc.h#L19))

**Purpose:** Limit concurrent security screening capacity.

**Key:** Generated from `IPC_KEY_SEM_SECURITY_ID` ('E')

**Initial Value:** `SECURITY_STATIONS * SECURITY_STATION_CAPACITY = 3 * 2 = 6` ([main.c](src/processes/main.c#L172-L177))

**Operations:**
- Passenger decrements before requesting screening ([passenger.c](src/processes/passenger.c#L160))
- Passenger increments after completion ([passenger.c](src/processes/passenger.c#L183))

```c
// Wait for security capacity
sem_wait_single(sem_security, 0);
// Request security station...
// ... screening happens ...
// Release capacity
sem_signal_single(sem_security, 0);
```

#### Ramp Slots Semaphore Set ([ipc.h](include/common/ipc.h#L21))

**Purpose:** Control concurrent access to ferry ramp (separate VIP/regular slots).

**Key:** Generated from `IPC_KEY_SEM_RAMP_SLOTS_ID` ('T')

**Semaphore Count:** 2
- **[0]:** Regular passenger slots
- **[1]:** VIP passenger slots

**Initial Values:** Both start at 0, ferry sets them when ready ([ferry_manager.c](src/processes/ferry_manager.c#L153-L154))

**Capacity:**
- Regular: `RAMP_CAPACITY_REG = 3` ([config.h](include/common/config.h#L13))
- VIP: `RAMP_CAPACITY_VIP = 2` ([config.h](include/common/config.h#L14))

**Operations:**
```c
// Ferry opens ramp (sets capacity)
sem_set_noundo(sem_ramp_slots, 0, RAMP_CAPACITY_REG);  // Regular slots
sem_set_noundo(sem_ramp_slots, 1, RAMP_CAPACITY_VIP);  // VIP slots

// Passenger waits for slot
sem_wait_single_nointr_noundo(sem_ramp_slots, ticket.vip);  // 0 or 1

// Ferry releases slot after passenger boards
sem_signal_single_noundo(sem_ramp_slots, ramp_msg.is_vip);
```

**Why NOUNDO?** Prevents automatic undo on process exit - slots should only be released explicitly.

#### Current Ferry Semaphore ([ipc.h](include/common/ipc.h#L22))

**Purpose:** Ferry turn coordination (only one ferry docks at a time).

**Key:** Generated from `IPC_KEY_SEM_CURRENT_FERRY` ('F')

**Initial Value:** 1 (one ferry can start immediately) ([main.c](src/processes/main.c#L192-L199))

**Usage:**
- Ferry waits to become active ([ferry_manager.c](src/processes/ferry_manager.c#L115))
- Ferry signals next ferry after departure ([ferry_manager.c](src/processes/ferry_manager.c#L219))

```c
// Wait for turn to dock
sem_wait_single(sem_current_ferry, 0);
// ... manage boarding ...
// Depart and signal next ferry
sem_signal_single(sem_current_ferry, 0);
```

### IPC Function Wrappers

All IPC operations use wrapper functions from [ipc.c](src/common/ipc.c) with automatic EINTR handling:

**Semaphore Operations:**
- `sem_wait_single()` - Decrement with SEM_UNDO, retry on EINTR ([ipc.c](src/common/ipc.c#L156-L167))
- `sem_signal_single()` - Increment with SEM_UNDO, retry on EINTR ([ipc.c](src/common/ipc.c#L212-L223))
- `sem_wait_single_noundo()` - Decrement without SEM_UNDO ([ipc.c](src/common/ipc.c#L137-L147))
- `sem_signal_single_noundo()` - Increment without SEM_UNDO ([ipc.c](src/common/ipc.c#L195-L205))

**Queue Operations:**
- `queue_create()` - Create new queue ([ipc.c](src/common/ipc.c#L16-L18))
- `queue_open()` - Open existing queue ([ipc.c](src/common/ipc.c#L25-L27))
- `queue_close()` - Remove queue ([ipc.c](src/common/ipc.c#L34-L36))

**Shared Memory Operations:**
- `shm_create()` - Create shared memory ([ipc.c](src/common/ipc.c#L239-L241))
- `shm_attach()` - Attach to process space ([ipc.c](src/common/ipc.c#L266-L268))
- `shm_detach()` - Detach from process space ([ipc.c](src/common/ipc.c#L275-L277))

## Configuration

All simulation parameters are defined in [config.h](include/common/config.h):

| Parameter | Value | Description |
|-----------|-------|-------------|
| `PASSENGER_COUNT` | 50 | Total passengers to spawn |
| `FERRY_COUNT` | 5 | Number of ferries in rotation |
| `FERRY_CAPACITY` | 50 | Max passengers per ferry |
| `SECURITY_STATIONS` | 3 | Number of security checkpoints |
| `SECURITY_STATION_CAPACITY` | 2 | Max passengers per station |
| `SECURITY_MAX_FRUSTRATION` | 3 | Max overtakes before priority |
| `RAMP_CAPACITY_REG` | 3 | Regular passenger ramp slots |
| `RAMP_CAPACITY_VIP` | 2 | VIP passenger ramp slots |
| `FERRY_DEPARTURE_INTERVAL` | 15 | Seconds before auto-depart |
| `FERRY_TRAVEL_TIME` | 30 | One-way travel time (seconds) |
| `PASSENGER_SECURITY_TIME_MIN` | 2 | Min security screening time |
| `PASSENGER_SECURITY_TIME_MAX` | 5 | Max security screening time |
| `FERRY_BAGGAGE_LIMIT_MIN` | 20 | Minimum baggage limit |
| `FERRY_BAGGAGE_LIMIT_MAX` | 60 | Maximum baggage limit |

## Synchronization Patterns

### 1. Producer-Consumer (Security Queue)

**Producers:** Passengers requesting screening  
**Consumer:** Security manager  
**Synchronization:** Semaphore counting + message queue

```c
// Producer (Passenger)
sem_wait_single(sem_security, 0);  // Acquire capacity
msgsnd(queue_security, &msg, ...); // Send request
msgrcv(queue_security, &msg, ..., getpid(), 0); // Wait for response
sem_signal_single(sem_security, 0); // Release capacity

// Consumer (Security Manager)
msgrcv(queue_security, &msg, ..., 1, 0); // Receive request
// ... process screening ...
msgsnd(queue_security, &msg, ..., msg.pid, 0); // Send completion
```

### 2. Mutual Exclusion (Shared Memory)

**Resource:** Ferry state in shared memory  
**Protection:** Binary semaphore (mutex)

```c
sem_wait_single(sem_state_mutex, SEM_STATE_MUTEX_VARIANT_FERRIES_STATE);
shared_state->ferries[ferry_id].passenger_count++;
sem_signal_single(sem_state_mutex, SEM_STATE_MUTEX_VARIANT_FERRIES_STATE);
```

### 3. Resource Limiting (Ramp Slots)

**Resource:** Limited ramp capacity  
**Mechanism:** Counting semaphore with separate VIP/regular pools

```c
// Ferry opens gate
sem_set_noundo(sem_ramp_slots, 0, RAMP_CAPACITY_REG);

// Passenger acquires slot
sem_wait_single_nointr_noundo(sem_ramp_slots, is_vip);

// Ferry releases slot after boarding
sem_signal_single_noundo(sem_ramp_slots, is_vip);
```

### 4. Turn-Based Access (Ferry Dock)

**Resource:** Single ferry dock position  
**Mechanism:** Binary semaphore with handoff pattern

```c
// Ferry N waits for turn
sem_wait_single(sem_current_ferry, 0);
// ... operate ferry ...
// Ferry N signals next ferry
sem_signal_single(sem_current_ferry, 0);
```

## Signal Handling

### SIGINT (Ctrl+C)
- **Port Manager:** Broadcasts SIGUSR2 to all processes, closes port ([port_manager.c](src/processes/port_manager.c#L26-L33))
- **Other Processes:** Ignored to allow graceful cleanup

### SIGUSR1 (Early Departure)
- **Ferry Manager:** Sets flag to trigger immediate departure ([ferry_manager.c](src/processes/ferry_manager.c#L22-L24))

### SIGUSR2 (Port Closing)
- **Passenger:** Sets flag to exit gracefully ([passenger.c](src/processes/passenger.c#L24-L26))
- **Ferry Manager:** Exits boarding loop

## Error Handling

- **EINTR Retry:** All blocking IPC calls automatically retry on signal interruption
- **Resource Cleanup:** IPC resources cleaned up on both normal and error exits
- **Deadlock Prevention:** Consistent lock ordering, timeouts on blocking operations
- **Process Orphans:** Port manager waits for all children before exiting

## Requirements Implementation

This implementation satisfies all requirements from [REQUIREMENTS.md](REQUIREMENTS.md):

✅ Baggage weight check with ferry-specific limits  
✅ 3 security stations with 2-person capacity each  
✅ Gender-segregated security screening  
✅ Frustration mechanism (max 3 overtakes)  
✅ VIP priority boarding  
✅ Ramp capacity management (K < N)  
✅ Automatic ferry departure after time interval T1  
✅ Early departure on signal (SIGUSR1)  
✅ Multiple ferries with different baggage limits  
✅ Ferry travel and return cycle  
✅ Port closure coordination (SIGUSR2)  
✅ Comprehensive event logging  

## License

Educational project for Operating Systems course.

## Author

Paweł Lidwin (155285)
