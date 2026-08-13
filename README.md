## Elevator Operations Kernel

Developed a kernel module that implements a pet elevator with scheduling capabilities,
supporting operations such as starting, stopping, and processing passenger requests. It includes a
/proc/elevator entry to display essential elevator status and information. The module ensures
efficient management of concurrency and synchronization within the kernel environment.
 

## File Listing
```
~/part1:
    empty empty.c empty.trace part1 part1.c part1.trace
~/part2: 
   Makefile  ~/part2/src:
        my_timer.c
~/part3:
    Makefile  ~/part3/src:
        elevator.c syscalls.c
```

**How to Compile & Execute**
### Requirements
- **Kernel Development Environment**: A Linux VM with kernel headers matching the running kernel
- **Compiler & Tools**: `gcc`, `make`, and kernel build tools
- **Root Access**: Required for `insmod`, `rmmod`, and accessing `/proc/elevator`.
---


### Compilation
Navigate to ~/part2

```
make
```
This will compile `my_timer.c` into `my_timer.ko`
 

Navigate to ~/part3
```
make
```
This will compile `elevator.c` into `elevator.ko`
 
---
### Execution
1. Load the elevator module:
   ```
   sudo insmod elevator.ko
   ```
2. Monitor elevator state (in one terminal):
   ```
   watch -n1 cat /proc/elevator
   ```
3. Generate pet requests (in another terminal):
   ```
   ./producer [number_of_pets]
   ```
4. Control the elevator:
   ```
   ./consumer --start   # Start elevator (begins servicing)
   ./consumer --stop    # Stop after unloading all onboard pets
   ```
5. Unload the module when done:
   ```
   sudo rmmod elevator
   ```

