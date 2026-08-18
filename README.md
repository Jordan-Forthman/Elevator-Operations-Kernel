# Elevator Operations Kernel

A Linux kernel module implementing a scheduled elevator, written in C against
the kernel API: a kthread for the control loop, mutexes and wait queues for
synchronization, `linux/list.h` for the queues, and `kmalloc` for per-passenger
allocation. State is published to user space through `/proc`.

The elevator carries pets between the five floors of a building, boarding them
FIFO within a 5-passenger and 50 lb capacity, and dispatching using the **LOOK**
scheduling algorithm.

## Quickstart

Needs a Linux machine with headers for the running kernel, plus `gcc` and
`make`:

```bash
sudo apt install build-essential linux-headers-$(uname -r)
```

Then:

```bash
git clone https://github.com/Jordan-Forthman/Elevator-Operations-Kernel.git
cd Elevator-Operations-Kernel
make

cd elevator-module
sudo insmod src/elevator.ko          # load the module
watch -n1 cat /proc/elevator         # terminal 1: live status

./tools/consumer --start             # terminal 2: start it
./tools/producer 10                  # send ten pets
./tools/consumer --stop              # finish deliveries, then go OFFLINE
sudo rmmod elevator                  # unload
```

`watch` shows the elevator working the building:

```
Elevator state: LOADING
Current floor: 4
Current load: 30 lbs
Elevator status: C5 D2 P4 H1
[ ] Floor 5: 1 H3
[*] Floor 4: 0
[ ] Floor 3: 2 P5 C1
[ ] Floor 2: 1 D4
[ ] Floor 1: 0
Number of pets: 4
Number of pets waiting: 4
Number of pets serviced: 27
```

Each pet shows as two characters, type and destination floor.
`C`hihuahua 3 lb, `P`ug 14 lb, Pug`h`uahua 10 lb, `D`achshund 16 lb.

## Two build modes

The elevator is driven by three operations: start, stop, and issue a request.
How user space reaches them depends on how you build.

**Default, stock kernel.** The module is self-contained and exposes a
write-only `/proc/elevator_ctl`. Nothing beyond kernel headers is required,
which is what the quickstart uses. The control file takes the three operations
directly:

```bash
echo "start"         | sudo tee /proc/elevator_ctl
echo "request 1 4 2" | sudo tee /proc/elevator_ctl   # floor 1 to 4, Pughuahua
echo "stop"          | sudo tee /proc/elevator_ctl
```

**`SYSCALLS=1`, patched kernel.** The design the project targets: three real
system calls compiled into the kernel image, which the module hooks on load
through exported function pointers.

| Number | Prototype |
| --- | --- |
| 548 | `int start_elevator(void)` |
| 549 | `int issue_request(int start_floor, int destination_floor, int type)` |
| 550 | `int stop_elevator(void)` |

This requires patching and rebuilding the kernel, covered in
[`docs/kernel-syscalls.md`](docs/kernel-syscalls.md). `producer` and `consumer`
call the syscalls when they exist and fall back to `/proc/elevator_ctl` when
they do not, so the same binaries drive either build.

The elevator logic is identical in both modes. Only the entry point differs.

## How it works

**Scheduling with LOOK.** The control thread keeps travelling in one direction
while any floor ahead still needs service, then reverses. Unlike SCAN it never
runs to the end of the building for no reason, and unlike FIFO it does not
re-cross floors it just passed. A floor "needs service" if a pet is waiting
there *or* an onboard pet is bound for it, and that second half is what keeps
deliveries from being skipped mid-sweep.

**Concurrency.** All shared state, meaning the five per-floor queues, the
onboard list, position, and counters, sits behind one mutex. The control loop
runs in a kthread, `issue_request` can arrive on any CPU from any process, and
`/proc` reads happen concurrently with both. The thread never sleeps holding the
lock: it releases before `msleep` and before waiting on the queue.

**Idling.** With no work the thread blocks on a wait queue rather than polling,
and a new request wakes it. An idle elevator costs no CPU.

**Timing.** 2.0 s to move a floor, 1.0 s to load or unload, the delays that make
the simulation observable under `watch`.

**Loading rules.** Pets board FIFO, and boarding stops at the first one that
does not fit rather than skipping ahead to a smaller one. That is head-of-line
blocking, which is the honest reading of a queue. Unloading happens before
loading at the same stop.

**Shutdown.** `stop` refuses new boardings but finishes delivering everyone
already aboard before the state reaches OFFLINE. Module unload waits for that,
stops the thread, and frees every outstanding allocation.

## Layout

```
syscall-tracing/     five syscalls verified against an empty baseline
timer-module/src/    my_timer.c   /proc/timer, current and elapsed time
elevator-module/src/ elevator.c   the elevator module
                     syscalls.c   syscall definitions for the patched build
elevator-module/tools/
                     producer.c, consumer.c   user-space drivers
docs/                kernel patching instructions
```

### syscall-tracing

`part1.c` makes exactly five syscalls more than an empty program, verified by
diffing their traces:

```bash
cd syscall-tracing
make verify
```

```
empty.trace: 31 syscalls
part1.trace: 36 syscalls
added:       5
the added calls are:
  clock_nanosleep
  getpid
  getppid
  read
  write
```

Traces are generated rather than committed. `make trace` rebuilds them.

### timer-module

```bash
cd timer-module && make
sudo insmod src/my_timer.ko
cat /proc/timer      # current time
sleep 1
cat /proc/timer      # current time plus elapsed since the last read
sudo rmmod my_timer
```

## Notes

Built for COP4610 (Operating Systems). The elevator itself is
`elevator-module/src/elevator.c`.

Since the coursework version this repo has been updated so it builds and runs
on an unmodified kernel, the user-space drivers have been written against the
specified syscall numbers, the `/proc` buffer has been sized to the
10,000-character bound the spec allows, and the control thread's lifetime has
been fixed so unloading the module cannot race a thread that already exited.
