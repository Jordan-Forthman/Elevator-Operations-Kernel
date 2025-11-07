#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>		//for dynamic memory allocation
#include <linux/uaccess.h>	//safe copyies proc data to user space
#include <linux/wait.h>
#include <linux/errno.h>
#include <linux/types.h>

/*hooks system calls, module assigns its implementation*/
extern int (*STUB_start_elevator)(void);
extern int (*STUB_issue_request)(int, int, int);
extern int (*STUB_stop_elevator)(void);

// states
enum state_t { OFFLINE, IDLE, UP, DOWN, LOADING };
static const char *state_str[] = {"OFFLINE", "IDLE", "UP", "DOWN", "LOADING"};

struct pet {
    int type;
    int dest_floor;
    struct list_head list;	//for linking in queues
};

static const int weights[] = {3, 14, 10, 16};
static const char type_chars[] = {'C', 'P', 'H', 'D'};

/* ----- Shared Elevator State ----- */
static struct list_head floor_waiting[5];	//FIFO queue per floor
static int floor_num_waiting[5] = {0};		//waiting counts per floor

static struct list_head elevator_pets;	//onboard pet list
static int elevator_num_pets = 0;	//onboard count
static int elevator_weight = 0;		//current weight

static int total_serviced = 0;	//serviced count
static int total_waiting = 0;	//waiting count

static enum state_t elevator_state = OFFLINE;	//init to OFFLINE
static int current_floor = 1;		//init position @ floor 1
static int direction = 1;		//init direction (up=1, down=-1)
static bool is_deactivating = false;	//deactivation flag; prevents new loads while offloading

/* --- Synchronization Primitives --- */
static struct mutex elev_lock;
static wait_queue_head_t elev_wait;	//wait queue for thread
static struct task_struct *elev_thread; //thread pointer for managing kthread
static bool work_to_do = false;		//work flag; signals waiting pets to wake thread

static struct proc_dir_entry *proc_file;	//proc entry pointer

/* checks if floor needs attention. declared early to allow use before definition */
static bool need_service(int floor);


/* ---------- helpers ---------- */
/* checks if a floor needs service by iterating through all floors and calling need_service*/
static bool has_pending(void) {
    int i;
    for (i = 1; i <= 5; i++)
        if (need_service(i)) return true;
    return false;
}

/* determines if a floor requires elevator */
static bool need_service(int floor) {
    /* pets waiting on a floor should not be serviced if deactivating */
    bool has_wait = (!is_deactivating && floor_num_waiting[floor - 1] > 0);	//checks for waiting pets if no stop
    bool has_dest = false;	//init destination flag
    struct pet *p;	//ptr for iteration
    list_for_each_entry(p, &elevator_pets, list) {
        if (p->dest_floor == floor) { has_dest = true; break; }	//sets flag if its at destinatoin
    }
    return has_wait || has_dest;
}

/* checks for service needs ahead in the current direction */
static bool has_requests_in_dir(int dir) {
    int i;
    if (dir > 0) {	//if direction is UP
        for (i = current_floor + 1; i <= 5; i++)   //loop updward
            if (need_service(i)) return true;
    } else {		// if direction is DOWN
        for (i = current_floor - 1; i >= 1; i--)   //loop downward
            if (need_service(i)) return true;
    }
    return false;	//no requests, triggers direction change
}

/* unload pets at current floor */
static void do_unload(void) {
    struct pet *p, *tmp;
    // loop safely over onboard pets
    list_for_each_entry_safe(p, tmp, &elevator_pets, list) {
        if (p->dest_floor == current_floor) {
            elevator_weight -= weights[p->type];   //subtract weight
            elevator_num_pets--;		   //decrease # of pets onboard
            total_serviced++;			   //increment serviced num
            list_del(&p->list);		//remove from list
            kfree(p);			//free memory
        }
    }
}

/* checks if any onboard pet destinations match current floor */
static bool can_unload(void) {
    struct pet *p;	//iteratoin ptr
    /*loop over onboard pets. scan for matches */
    list_for_each_entry(p, &elevator_pets, list)
        if (p->dest_floor == current_floor) return true;
    return false;
}

/* load waiting pets FIFO if space/weight allows */
static void do_load(void) {
    struct pet *p, *tmp;	//iteration ptrs for deletion during loop
    /* pets board in FIFO order */ 
    list_for_each_entry_safe(p, tmp, &floor_waiting[current_floor - 1], list) {
        int w = weights[p->type];	//get weight
        /* respect limits: max 5 pets and max 50 lbs */
        if (elevator_num_pets < 5 && elevator_weight + w <= 50) {
            list_del(&p->list);				//remove from waiting
            list_add_tail(&p->list, &elevator_pets);	//add to onboard FIFO
            elevator_weight += w;
            elevator_num_pets++;
            floor_num_waiting[current_floor - 1]--;	//decrement waiting # on floor
            total_waiting--;				//decrement total waiting
        } else {
            /* don't accept new pets if they can't fit */ 
            break;
        }
    }
}

/* checks if first waiting pet can fit */
static bool can_load(void) {
    //return if there are no waiting pets
    if (list_empty(&floor_waiting[current_floor - 1])) return false;

    //get first pet
    struct pet *p = list_first_entry(&floor_waiting[current_floor - 1],
                                     struct pet, list);
    // check weight
    int w = weights[p->type];
    // respect limits: max 5 pets and max 50 lbs 
    return elevator_num_pets < 5 && elevator_weight + w <= 50;
}

/* ---------- elevator thread ---------- */
/* main elevator control loop usin LOOK*/
static int elevator_thread_fn(void *data) {
    unsigned int sleep_ms = 0;	//delay variable

    /* main loop */
    while (!kthread_should_stop()) {
	//if delay is set, sleep and reset for next cycle
        if (sleep_ms > 0) {
            msleep(sleep_ms);
            sleep_ms = 0;
        }

        mutex_lock(&elev_lock);		//LOCK

        /* handle stop condition: must finish delivering pets */
        if (is_deactivating && list_empty(&elevator_pets)) {
            elevator_state = OFFLINE; 	//signal completion
            work_to_do = false;		//clear flag
            mutex_unlock(&elev_lock);	//UNLOCK
            break; /* exit thread loop */
        }

        bool loading = can_unload();	//check if can unload pets at the current floor

        /* stop signal means we don't load new pets */
        bool boarding = !is_deactivating && can_load();

        /* STATE: LOADING (Load/Unload) */
        /* can unload and load on the same tick */
        if (loading || boarding) {
            elevator_state = LOADING;
            if (loading)
                do_unload();	//unload if yes, DROP OFFS FIRST
            if (boarding)
                do_load();	//load if yes, PICK UPS SECONDS

            sleep_ms = 1000; 		// wait 1.0 second when loading/unloading
            mutex_unlock(&elev_lock);	// UNLOCK
            continue;	//next cycle
        }

        /* STATE: UP/DOWN (Moving) */
        if (has_pending()) {
            // implement LOOK scheduling algorithm
            if (!has_requests_in_dir(direction))
                direction = -direction; 	// change direction if no requests ahead

            elevator_state = (direction > 0) ? UP : DOWN;
            current_floor += direction;		//move floors
            sleep_ms = 2000; 			// wait 2.0 seconds when moving
            mutex_unlock(&elev_lock);		// UNLOCK
            continue;	//next cycle
        }

        /* STATE: IDLE (No work) */
        elevator_state = IDLE;
        work_to_do = false;	//clear flag to prepare for wait

        mutex_unlock(&elev_lock);	// UNLOCK

        /* wait in IDLE until signalled */
        wait_event_interruptible(elev_wait, work_to_do || is_deactivating);
    }
    return 0;
}

/* ---------- syscall stubs ---------- */
/* activate elevator if OFFLINE */
static int my_start_elevator(void) {
    mutex_lock(&elev_lock);	//LOCKS

    /* UNLOCK and return if already active */
    if (elevator_state != OFFLINE) {
        mutex_unlock(&elev_lock);
        return 1;
    }

    /* reset all variables to default */
    current_floor = 1;
    elevator_weight = 0;
    elevator_num_pets = 0;
    is_deactivating = false;
    direction = 1;
    elevator_state = IDLE;

    /* check if work was added *before* elevator started */
    work_to_do = has_pending();

    mutex_unlock(&elev_lock);	// UNLOCK

    /* start thread; launch simulation */
    elev_thread = kthread_run(elevator_thread_fn, NULL, "pet_elevator");

    /* if elevator fails to launch, reset state and return w/ error */
    if (IS_ERR(elev_thread)) {
        mutex_lock(&elev_lock);		// LOCK
        elevator_state = OFFLINE;	// revert to offline
        mutex_unlock(&elev_lock);	// UNLOCK
        return -ENOMEM;
    }
    return 0;	// successful activation
}

/*  queue a pet request if valid */
static int my_issue_request(int start, int dest, int type) {
    /* check if invalid args */
    if (start < 1 || start > 5 || dest < 1 || dest > 5 ||
        start == dest || type < 0 || type > 3)
        return 1;

    struct pet *p = kmalloc(sizeof(*p), GFP_KERNEL); //dynamic allocation
    if (!p) return 1; 		// check if memory error
    p->type = type;		// set type
    p->dest_floor = dest; 	// set destination
    INIT_LIST_HEAD(&p->list);	// init list to prepare for linking

    mutex_lock(&elev_lock);	// LOCK

    list_add_tail(&p->list, &floor_waiting[start - 1]);	// add FIFO
    floor_num_waiting[start - 1]++;			// increment floor count
    total_waiting++;					// increment total for proc

    /* wake thread only if elevator is active. if offline, my_start_elevator handles it */
    if (elevator_state != OFFLINE) {
        work_to_do = true;
        wake_up_interruptible(&elev_wait);
    }

    mutex_unlock(&elev_lock);	// UNLOCK
    return 0;	// successfully queued
}

/* initiate stop, if not already */
static int my_stop_elevator(void) {
    mutex_lock(&elev_lock);	// LOCK

    /* return 1 if elevator is already stopping */
    if (is_deactivating) {
        mutex_unlock(&elev_lock);   // UNLOCK
        return 1;
    }

    is_deactivating = true;

    /* wake up thread if it's IDLE so it can process the stop request */
    wake_up_interruptible(&elev_wait);

    mutex_unlock(&elev_lock);	 // UNLOCK
    return 0;
}

/* ---------- /proc/elevator ---------- */
#define PROC_BUF_SIZE 2048
/* reads and formats elevator status for /proc */
static ssize_t proc_read(struct file *fp, char __user *ubuf,
                         size_t size, loff_t *offs)
{
    char *kbuf;		// kernel buffer
    int len = 0;
    struct pet *p;	// iteration ptr for lists
    int f;		// floor loop variable

    /* single read only */
    if (*offs > 0) return 0;

    kbuf = kmalloc(PROC_BUF_SIZE, GFP_KERNEL);	// allocate buffer
    if (!kbuf) return -ENOMEM;			// fail if no memory

    mutex_lock(&elev_lock);	// LOCK

    len += scnprintf(kbuf + len, PROC_BUF_SIZE - len,
                     "Elevator state: %s\n", state_str[elevator_state]);   // add state
    len += scnprintf(kbuf + len, PROC_BUF_SIZE - len,
                     "Current floor: %d\n", current_floor);		   // add floor
    len += scnprintf(kbuf + len, PROC_BUF_SIZE - len,
                     "Current load: %d lbs\n", elevator_weight);	   // weight display
    len += scnprintf(kbuf + len, PROC_BUF_SIZE - len,
                     "Elevator status:");				   // start status

    /* print formatted list or empty space for empty list*/
    if (list_empty(&elevator_pets))
        len += scnprintf(kbuf + len, PROC_BUF_SIZE - len, " (empty)");
    else
        list_for_each_entry(p, &elevator_pets, list)
            len += scnprintf(kbuf + len, PROC_BUF_SIZE - len,
                             " %c%d", type_chars[p->type], p->dest_floor);

    len += scnprintf(kbuf + len, PROC_BUF_SIZE - len, "\n");	// newline

    /* reverse floor loop for top to bottom display */
    for (f = 5; f >= 1; f--) {
        len += scnprintf(kbuf + len, PROC_BUF_SIZE - len,
                         "[%c] Floor %d: %d",
                         (f == current_floor) ? '*' : ' ',
                         f, floor_num_waiting[f - 1]);
        if (floor_num_waiting[f - 1] > 0)
            /* Pets line up in FIFO on individual floors */
            list_for_each_entry(p, &floor_waiting[f - 1], list)
                len += scnprintf(kbuf + len, PROC_BUF_SIZE - len,
                                 " %c%d", type_chars[p->type], p->dest_floor);

        len += scnprintf(kbuf + len, PROC_BUF_SIZE - len, "\n");	// newline
    }

    len += scnprintf(kbuf + len, PROC_BUF_SIZE - len,
                     "Number of pets: %d\n", elevator_num_pets);	// onboard count
    len += scnprintf(kbuf + len, PROC_BUF_SIZE - len,
                     "Number of pets waiting: %d\n", total_waiting);	// waiting pets
    len += scnprintf(kbuf + len, PROC_BUF_SIZE - len,
                     "Number of pets serviced: %d\n", total_serviced);	// serviced pets

    mutex_unlock(&elev_lock);	// UNLOCK

    /* check size and copy to user */
    if (len > size) { kfree(kbuf); return -ENOSPC; }
    if (copy_to_user(ubuf, kbuf, len)) { kfree(kbuf); return -EFAULT; }

    *offs += len;	// updates offset for read completion
    kfree(kbuf);	// frees buffer
    return len;		// return bytes read
}

/* define file operations for /proc/elevator */
static const struct proc_ops elevator_pops = {
    .proc_read = proc_read,	// assigns read func; handles cat /proc/elevator
};


/* ---------- module init/exit ---------- */
/* module entry point */
static int __init elevator_init(void)
{
    /* init mutex, queue, list */
    int i;
    mutex_init(&elev_lock);
    init_waitqueue_head(&elev_wait);
    /*  floor loop for pre-floor setup */
    for (i = 0; i < 5; i++) {
        INIT_LIST_HEAD(&floor_waiting[i]);
        floor_num_waiting[i] = 0;
    }

    /* init empty elevator, state, start floor, direction, work flag */
    INIT_LIST_HEAD(&elevator_pets);
    elevator_state = OFFLINE;
    current_floor = 1;
    direction = 1;
    work_to_do = false;

    /* creates proc. error handling if creation fails */
    proc_file = proc_create("elevator", 0444, NULL, &elevator_pops);
    if (!proc_file) {
        pr_err("elevator: failed to create /proc/elevator\n");
        return -ENOMEM;
    }

    /* assign hooks */
    STUB_start_elevator = my_start_elevator;
    STUB_issue_request   = my_issue_request;
    STUB_stop_elevator   = my_stop_elevator;

    return 0;	// module loaded successfully
}

/* module cleanup */
static void __exit elevator_exit(void)
{
    int i;
    struct pet *p, *tmp;

    /* reset function pointers when exiting */
    STUB_start_elevator = NULL;
    STUB_issue_request   = NULL;
    STUB_stop_elevator   = NULL;

    proc_remove(proc_file);		// remove proc

    if (elevator_state != OFFLINE)	// stops if running
        my_stop_elevator();

    /* wait for the thread to stop itself. ensures OFFLINE before stop */
    while (1) {
        mutex_lock(&elev_lock);		// LOCK

	/* if ready, exit wait and UNLOCK */
        if (elevator_state == OFFLINE) {
            mutex_unlock(&elev_lock);
            break;
        }

        mutex_unlock(&elev_lock);	// UNLOCK
        msleep(100);			// short delay
    }

    /* stops thread to terminate solution */
    if (elev_thread) {
        kthread_stop(elev_thread);
    }

    /* clean up allocated memory aka free waiting pets */
    for (i = 0; i < 5; i++) {
        list_for_each_entry_safe(p, tmp, &floor_waiting[i], list) {
            list_del(&p->list);	// clean list
            kfree(p); 		// clean memory
        }
    }

    /* free onboard pets and clean memory */
    list_for_each_entry_safe(p, tmp, &elevator_pets, list) {
        list_del(&p->list);
        kfree(p);
    }
}

MODULE_LICENSE("GPL");
module_init(elevator_init);	// regsiter init, called on load
module_exit(elevator_exit);	// register exit, called on unload
