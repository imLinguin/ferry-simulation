#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include "../include/common/ipc.h"
#include "../include/common/messages.h"

// Simple test framework
static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT(condition, message) \
    do { \
        if (condition) { \
            tests_passed++; \
            printf("  ✓ %s\n", message); \
        } else { \
            tests_failed++; \
            printf("  ✗ FAILED: %s\n", message); \
        } \
    } while(0)

#define TEST_SUITE(name) \
    printf("\n=== %s ===\n", name)

// Test queue operations
void test_queue_operations(void) {
    TEST_SUITE("Queue Operations");
    
    // Test queue creation
    int queue_id = queue_create('T');
    ASSERT(queue_id != -1, "Create queue with key 'T'");
    
    // Test queue open
    int queue_id2 = queue_open('T');
    ASSERT(queue_id2 == queue_id, "Open existing queue returns same ID");
    
    // Test sending and receiving messages
    struct {
        long mtype;
        char data[64];
    } test_msg;
    
    test_msg.mtype = 1;
    strcpy(test_msg.data, "Test message");
    
    int send_result = msgsnd(queue_id, &test_msg, sizeof(test_msg.data), IPC_NOWAIT);
    ASSERT(send_result == 0, "Send message to queue");
    
    struct {
        long mtype;
        char data[64];
    } recv_msg;
    
    ssize_t recv_size = msgrcv(queue_id, &recv_msg, sizeof(recv_msg.data), 1, IPC_NOWAIT);
    ASSERT(recv_size > 0, "Receive message from queue");
    ASSERT(strcmp(recv_msg.data, "Test message") == 0, "Message content matches");
    
    // Test queue cleanup
    int close_result = queue_close(queue_id);
    ASSERT(close_result == 0, "Close and remove queue");
    
    // Verify queue is removed
    int reopen_result = msgget('T', 0666);
    ASSERT(reopen_result == -1 && errno == ENOENT, "Queue no longer exists after close");
}

// Test semaphore operations
void test_semaphore_operations(void) {
    TEST_SUITE("Semaphore Operations");
    
    // Test semaphore creation with initial values
    unsigned short initial_values[3] = {1, 0, 5};
    int sem_id = sem_create('S', 3, initial_values);
    ASSERT(sem_id != -1, "Create semaphore set with 3 semaphores");
    
    // Test reading initial values
    int val0 = sem_get_val(sem_id, 0);
    int val1 = sem_get_val(sem_id, 1);
    int val2 = sem_get_val(sem_id, 2);
    ASSERT(val0 == 1, "Semaphore 0 initial value is 1");
    ASSERT(val1 == 0, "Semaphore 1 initial value is 0");
    ASSERT(val2 == 5, "Semaphore 2 initial value is 5");
    
    // Test sem_wait (decrement)
    int wait_result = sem_wait_single(sem_id, 0);
    ASSERT(wait_result == 0, "Wait on semaphore 0 succeeds");
    ASSERT(sem_get_val(sem_id, 0) == 0, "Semaphore 0 value decremented to 0");
    
    // Test sem_signal (increment)
    int signal_result = sem_signal_single(sem_id, 0);
    ASSERT(signal_result == 0, "Signal semaphore 0 succeeds");
    ASSERT(sem_get_val(sem_id, 0) == 1, "Semaphore 0 value incremented back to 1");
    
    // Test set value
    int set_result = sem_set_noundo(sem_id, 1, 10);
    ASSERT(set_result == 0, "Set semaphore 1 value to 10");
    ASSERT(sem_get_val(sem_id, 1) == 10, "Semaphore 1 value is now 10");
    
    // Test cleanup
    int close_result = sem_close(sem_id);
    ASSERT(close_result == 0, "Close and remove semaphore set");
    
    // Verify semaphore is removed
    int reopen_result = semget('S', 3, 0666);
    ASSERT(reopen_result == -1 && errno == ENOENT, "Semaphore set no longer exists");
}

// Test shared memory operations
void test_shared_memory_operations(void) {
    TEST_SUITE("Shared Memory Operations");
    
    // Test shared memory creation
    size_t size = 4096;
    int shm_id = shm_create('M', size);
    ASSERT(shm_id != -1, "Create shared memory segment");
    
    // Test attaching to shared memory
    void *addr = shm_attach(shm_id);
    ASSERT(addr != (void*)-1, "Attach to shared memory");
    
    // Test writing and reading
    strcpy((char*)addr, "Shared data");
    ASSERT(strcmp((char*)addr, "Shared data") == 0, "Write and read shared memory");
    
    // Test detaching
    int detach_result = shm_detach(addr);
    ASSERT(detach_result == 0, "Detach from shared memory");
    
    // Test reattaching in another "process" (same process for test)
    void *addr2 = shm_attach(shm_id);
    ASSERT(addr2 != (void*)-1, "Reattach to shared memory");
    ASSERT(strcmp((char*)addr2, "Shared data") == 0, "Data persists across attach/detach");
    
    shm_detach(addr2);
    
    // Test cleanup
    int close_result = shm_close(shm_id);
    ASSERT(close_result == 0, "Close and remove shared memory");
    
    // Verify shared memory is removed
    int reopen_result = shmget('M', size, 0666);
    ASSERT(reopen_result == -1 && errno == ENOENT, "Shared memory no longer exists");
}

// Test multi-process semaphore coordination
void test_semaphore_multiprocess(void) {
    TEST_SUITE("Multi-Process Semaphore Coordination");
    
    unsigned short initial_val = 0;
    int sem_id = sem_create('C', 1, &initial_val);
    ASSERT(sem_id != -1, "Create coordination semaphore");
    
    pid_t pid = fork();
    
    if (pid == 0) {
        // Child process: wait for signal, then signal back
        usleep(100000);  // 100ms delay
        sem_signal_single(sem_id, 0);  // Signal parent
        exit(0);
    } else {
        // Parent process: wait for child signal
        ASSERT(pid > 0, "Fork child process");
        
        int wait_result = sem_wait_single(sem_id, 0);
        ASSERT(wait_result == 0, "Parent received signal from child");
        
        int status;
        waitpid(pid, &status, 0);
        ASSERT(WIFEXITED(status) && WEXITSTATUS(status) == 0, "Child exited successfully");
        
        sem_close(sem_id);
    }
}

// Test multi-process shared memory
void test_shared_memory_multiprocess(void) {
    TEST_SUITE("Multi-Process Shared Memory");
    
    int shm_id = shm_create('D', sizeof(int));
    ASSERT(shm_id != -1, "Create shared memory for inter-process test");
    
    int *shared_counter = (int*)shm_attach(shm_id);
    ASSERT(shared_counter != (int*)-1, "Attach to shared memory");
    *shared_counter = 0;
    
    pid_t pid = fork();
    
    if (pid == 0) {
        // Child process: increment counter
        int *child_addr = (int*)shm_attach(shm_id);
        (*child_addr)++;
        shm_detach(child_addr);
        exit(0);
    } else {
        ASSERT(pid > 0, "Fork child process for shared memory test");
        
        int status;
        waitpid(pid, &status, 0);
        
        ASSERT(*shared_counter == 1, "Child incremented shared counter");
        ASSERT(WIFEXITED(status) && WEXITSTATUS(status) == 0, "Child exited successfully");
        
        shm_detach(shared_counter);
        shm_close(shm_id);
    }
}

// Test message priority (for ramp queue)
void test_message_priority(void) {
    TEST_SUITE("Message Priority Handling");
    
    int queue_id = queue_create('P');
    ASSERT(queue_id != -1, "Create priority test queue");
    
    // Send messages with different priorities
    struct {
        long mtype;
        int data;
    } msg;
    
    // Send low priority (3), then high priority (1), then medium (2)
    msg.mtype = 3;
    msg.data = 300;
    msgsnd(queue_id, &msg, sizeof(msg.data), 0);
    
    msg.mtype = 1;
    msg.data = 100;
    msgsnd(queue_id, &msg, sizeof(msg.data), 0);
    
    msg.mtype = 2;
    msg.data = 200;
    msgsnd(queue_id, &msg, sizeof(msg.data), 0);
    
    // Receive in priority order using negative mtype
    struct {
        long mtype;
        int data;
    } recv;
    
    msgrcv(queue_id, &recv, sizeof(recv.data), -3, 0);
    ASSERT(recv.mtype == 1 && recv.data == 100, "Highest priority (1) received first");
    
    msgrcv(queue_id, &recv, sizeof(recv.data), -3, 0);
    ASSERT(recv.mtype == 2 && recv.data == 200, "Medium priority (2) received second");
    
    msgrcv(queue_id, &recv, sizeof(recv.data), -3, 0);
    ASSERT(recv.mtype == 3 && recv.data == 300, "Lowest priority (3) received last");
    
    queue_close(queue_id);
}

// Test error conditions
void test_error_conditions(void) {
    TEST_SUITE("Error Conditions and Edge Cases");
    
    // Test opening non-existent queue
    int bad_queue = queue_open('Z');
    ASSERT(bad_queue == -1, "Opening non-existent queue returns -1");
    
    // Test double close protection
    int queue_id = queue_create('E');
    queue_close(queue_id);
    ASSERT(queue_close_if_exists('E') == -1, "Close non-existent queue returns error");
    
    // Test invalid semaphore operations
    int bad_sem = semget('X', 1, 0666);
    ASSERT(bad_sem == -1, "Opening non-existent semaphore returns -1");
    
    // Test attaching to non-existent shared memory
    int bad_shm = shmget('Y', 1024, 0666);
    ASSERT(bad_shm == -1, "Opening non-existent shared memory returns -1");
}

// Main test runner
int main(void) {
    printf("╔════════════════════════════════════════╗\n");
    printf("║     IPC Functions Unit Test Suite     ║\n");
    printf("╚════════════════════════════════════════╝\n");
    
    test_queue_operations();
    test_semaphore_operations();
    test_shared_memory_operations();
    test_semaphore_multiprocess();
    test_shared_memory_multiprocess();
    test_message_priority();
    test_error_conditions();
    
    printf("\n╔════════════════════════════════════════╗\n");
    printf("║            Test Results                ║\n");
    printf("╠════════════════════════════════════════╣\n");
    printf("║ Passed: %-3d                           ║\n", tests_passed);
    printf("║ Failed: %-3d                           ║\n", tests_failed);
    printf("╚════════════════════════════════════════╝\n");
    
    return tests_failed > 0 ? 1 : 0;
}
