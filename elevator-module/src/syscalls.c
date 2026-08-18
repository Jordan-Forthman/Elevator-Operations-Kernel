#include <linux/linkage.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/syscalls.h>
#include <linux/errno.h>

// STUB pointers for testing, init to NULL
int (*STUB_start_elevator)(void) = NULL;
int (*STUB_issue_request)(int, int, int) = NULL;
int (*STUB_stop_elevator)(void) = NULL;

// export symbols so STUB pointers are available to other modules
EXPORT_SYMBOL(STUB_start_elevator);
EXPORT_SYMBOL(STUB_issue_request);
EXPORT_SYMBOL(STUB_stop_elevator);

// activates the elevator if not already active
SYSCALL_DEFINE0(start_elevator) {
    // check for STUB hook
    if (STUB_start_elevator != NULL) {
        return STUB_start_elevator();
    }
    return -ENOSYS;
}

// creates a pet request, validates, adds to floor list
SYSCALL_DEFINE3(issue_request, int, start_floor, int, destination_floor, int, type) {
    // check for STUB hook
    if (STUB_issue_request != NULL) {
        return STUB_issue_request(start_floor, destination_floor, type);
    }
    return -ENOSYS;
}

// initiates deactivation (thread handles offloading)
SYSCALL_DEFINE0(stop_elevator) {
    // check for STUB hook
    if (STUB_stop_elevator != NULL) {
        return STUB_stop_elevator();
    }
    return -ENOSYS;
}
