# Adding the elevator system calls to the kernel

The elevator exposes three custom system calls. This document covers the build
that uses them — `make SYSCALLS=1`, which requires a kernel patched and
rebuilt as described below.

If you only want to see the elevator run, you do not need any of this: the
default build is self-contained and is driven through `/proc/elevator_ctl`.
See the README's quickstart.

| Number | Prototype | Returns |
| --- | --- | --- |
| 548 | `int start_elevator(void)` | `1` if already active, `0` on success, `-ENOMEM` on failure |
| 549 | `int issue_request(int start_floor, int destination_floor, int type)` | `1` if the request is invalid, `0` otherwise |
| 550 | `int stop_elevator(void)` | `1` if already deactivating, `0` otherwise |

`type` is `0` Chihuahua, `1` Pug, `2` Pughuahua, `3` Dachshund.

## How the module reaches the syscalls

The syscalls themselves live in the kernel image, but the elevator logic lives
in a loadable module. They are joined by three exported function pointers:

```
user space          kernel image                      elevator.ko
-----------         ---------------------------       -----------------------
./consumer  ──►  sys_start_elevator (548)  ──►  STUB_start_elevator ──► my_start_elevator()
./producer  ──►  sys_issue_request  (549)  ──►  STUB_issue_request  ──► my_issue_request()
./consumer  ──►  sys_stop_elevator  (550)  ──►  STUB_stop_elevator  ──► my_stop_elevator()
```

`syscalls.c` (in `part3/src/`) defines the pointers, exports them with
`EXPORT_SYMBOL`, and returns `-ENOSYS` while they are `NULL`. `elevator_init()`
assigns them on load; `elevator_exit()` clears them on unload. That indirection
is what lets the elevator be rebuilt and reloaded without rebuilding the kernel
each time.

Because the pointers are defined in the kernel image, a `SYSCALLS=1` build
against an unpatched kernel fails at the modpost stage with:

```
ERROR: modpost: "STUB_start_elevator" [src/elevator.ko] undefined!
```

That error means the kernel below has not been patched, not that the module is
broken.

## Patching the kernel

Tested against the 6.10–6.16 series. Adjust paths if your tree differs.

**1. Get the source and work from a symlink**

```bash
cd /usr/src
sudo tar xf linux-<version>.tar.xz
sudo ln -s /usr/src/linux-<version> ~/linux-<version>
cd ~/linux-<version>
```

**2. Add the syscall implementations**

Copy `part3/src/syscalls.c` into a new `syscalls/` directory in the kernel
tree, and add a Makefile beside it:

```bash
mkdir -p syscalls
cp /path/to/repo/part3/src/syscalls.c syscalls/
printf 'obj-y := syscalls.o\n' > syscalls/Makefile
```

**3. Build the new directory** — append `syscalls/` to `core-y` in the
top-level kernel `Makefile`:

```make
core-y += kernel/ mm/ fs/ ipc/ security/ crypto/ syscalls/
```

**4. Register the numbers** in `arch/x86/entry/syscalls/syscall_64.tbl`
(older trees: `arch/x86/syscalls/syscall_64.tbl`):

```
548	common	start_elevator		sys_start_elevator
549	common	issue_request		sys_issue_request
550	common	stop_elevator		sys_stop_elevator
```

**5. Declare the prototypes** at the end of `include/linux/syscalls.h`, before
the closing `#endif`:

```c
asmlinkage long sys_start_elevator(void);
asmlinkage long sys_issue_request(int start_floor, int destination_floor, int type);
asmlinkage long sys_stop_elevator(void);
```

**6. Configure and build.** Module signing must be disabled, or the build will
stop looking for a signing certificate:

```bash
cp /boot/config-$(uname -r) .config
scripts/config --disable SYSTEM_TRUSTED_KEYS
scripts/config --disable SYSTEM_REVOCATION_KEYS
scripts/config --set-str CONFIG_SYSTEM_TRUSTED_KEYS ""
make olddefconfig
make -j$(nproc)
sudo make modules_install
sudo make install
sudo reboot
```

This takes a while — budget an hour or more on a laptop.

**7. Confirm the running kernel and the syscalls**

```bash
uname -r          # should report the version you just built
```

The syscalls return `-ENOSYS` until `elevator.ko` is loaded, which is the
correct behaviour — the numbers exist, but nothing is hooked to them yet:

```bash
cd part3
make SYSCALLS=1
make tools
sudo insmod src/elevator.ko
./tools/consumer --start
```

If `consumer` reports `syscall 548 unavailable`, the kernel running is not the
patched one — check `uname -r` against the version you installed, and confirm
GRUB booted the right entry.
