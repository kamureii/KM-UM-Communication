# KM-UM Communication

A minimal, security-conscious example of **kernel-mode ↔ user-mode communication on Windows**:
a WDM kernel driver (`MyFirstDriver.sys`) exposes a device object, and a C++ console client
(`UM_APP.exe`) talks to it through `DeviceIoControl` IOCTLs.

The client sends a text message. The driver echoes it back once, then keeps answering periodic
requests and adds a kernel-side local timestamp to each reply.

```
 User mode                                   Kernel mode
┌──────────────────────┐                    ┌──────────────────────────────────┐
│ UM_APP.exe           │  CreateFileW       │ MyFirstDriver.sys                │
│                      │  \\.\MyDriver ───► │ \??\MyDriver -> \Device\MyDriver │
│ MESSAGE_PAYLOAD ─────┼─ DeviceIoControl ─►│ IRP_MJ_DEVICE_CONTROL            │
│   char Message[128]  │  (METHOD_BUFFERED) │  ├─ IOCTL_ECHO_ONCE              │
│                ◄─────┼────────────────────┤  └─ IOCTL_ECHO_REPEAT (+time)    │
└──────────────────────┘                    └──────────────────────────────────┘
              ▲                                          ▲
              └──────────── Common/Shared.h ─────────────┘
               (IOCTL codes, payload layout, device names)
```

## Features

| IOCTL | Code | Behaviour |
|---|---|---|
| `IOCTL_ECHO_ONCE` | `0x801` | Returns the message unchanged and logs it to the kernel debugger. |
| `IOCTL_ECHO_REPEAT` | `0x802` | Returns `"<message> \| Timestamp: HH:MM:SS"` using the kernel's local time. The client calls it once per second until you press **ESC**. |

- **One shared contract.** `Common/Shared.h` is compiled into both binaries, so the IOCTL codes and the payload layout cannot get out of sync. The payload has no pointers, so a 32-bit client works against a 64-bit driver without any thunking.
- **Buffered I/O.** Both IOCTLs use `METHOD_BUFFERED`, so the driver only touches the I/O manager's system buffer and never reads user memory directly.
- **Primitive driver package.** `MyFirstDriver.inf` installs the driver as a demand-start kernel service that runs from the driver store (DIRID 13). The build produces a test-signed catalog.

## Security design

The device is a deliberately small attack surface:

- **Locked-down device object.** It is created with `IoCreateDeviceSecure` and the SDDL `SDDL_DEVOBJ_SYS_ALL_ADM_ALL`, so only SYSTEM and Administrators can open it. `FILE_DEVICE_SECURE_OPEN` applies the same ACL to namespace opens such as `\\.\MyDriver\foo`.
- **IOCTL access checks.** The codes require `FILE_READ_DATA | FILE_WRITE_DATA`. A handle opened without those rights is rejected by the I/O manager before the driver runs.
- **Strict input validation.** Both input and output lengths must equal `sizeof(MESSAGE_PAYLOAD)`. The driver NUL-terminates the user-supplied string before using it, and builds the reply only with the bounded `RtlStringCb*` functions, which truncate safely and never overflow.
- **No kernel memory disclosure.** Input and output are the same size, so the bytes copied back to user mode are exactly the caller's own buffer plus the driver's reply.
- **Paging discipline.** `DriverEntry` lives in `INIT`, and the dispatch and unload routines live in `PAGE` with `PAGED_CODE()` assertions. SAL role types (`DRIVER_DISPATCH`, `DRIVER_UNLOAD`, …) are declared for Code Analysis. The driver builds with `/W4 /WX`, and Code Analysis reports no defects.

## Repository layout

```
Common/Shared.h                 Shared IOCTL / payload / device-name contract
MyFirstDriver/                  WDM kernel driver (WDK, x64 + ARM64 configurations)
  main.c                        DriverEntry, dispatch routines, unload
  MyFirstDriver.inf             Primitive driver package (service install)
  MyFirstDriver.slnx            Driver solution
UM_APP/                         User-mode console client (x86 + x64)
  UM_APP/UM_APP.cpp
  UM_APP.sln                    Client solution
```

## Building

Requirements: **Visual Studio 2022** with the *Desktop development with C++* workload, the
**Windows SDK and WDK 10.0.26100**, and (for ARM64) the *MSVC ARM64 build tools* component.

From a Developer Command Prompt:

```bat
msbuild MyFirstDriver\MyFirstDriver.vcxproj -p:Configuration=Release -p:Platform=x64
msbuild UM_APP\UM_APP.sln                    -p:Configuration=Release -p:Platform=x64
```

Or open `MyFirstDriver/MyFirstDriver.slnx` and `UM_APP/UM_APP.sln` in Visual Studio.

The driver package ends up in `MyFirstDriver\x64\<Config>\MyFirstDriver\` (`.sys`, `.inf`, `.cat`),
and the client ends up in `UM_APP\x64\<Config>\UM_APP.exe`. The Release clients link the CRT statically,
so they run on a clean test machine.

> If the build fails with `Unable to load DLL 'x86\InfVerif.dll'`, your WDK installation is missing
> InfVerif. Repair or reinstall the WDK. As a temporary workaround, add `-p:SkipPackageVerification=true`.

## Running (test machine or VM only)

Kernel drivers can crash the whole system. **Use a virtual machine or a dedicated test machine.**

1. **Enable test signing** (elevated prompt), then reboot:
   ```bat
   bcdedit /set testsigning on
   ```
2. **Copy** the driver package and `UM_APP.exe` to the machine.
3. **Install and start the driver.** Pick one method.
   - Quick (service control manager):
     ```bat
     sc create MyFirstDriver type= kernel binPath= C:\path\to\MyFirstDriver.sys
     sc start  MyFirstDriver
     ```
   - Driver package (INF):
     ```bat
     pnputil /add-driver C:\path\to\MyFirstDriver.inf /install
     sc start MyFirstDriver
     ```
4. **Run the client from an elevated prompt.** The device is restricted to Administrators.
   ```
   > UM_APP.exe
   --- User Mode App ---
   Device opened successfully.
   Enter a message (max 127 characters): hello kernel
   [REQ 1] Sending to KM: hello kernel
   [REQ 1] KM Echoed back: hello kernel

   [REQ 2] Starting 1 s periodic request. Press 'ESC' to stop...
   [REQ 2] KM Echoed back: hello kernel | Timestamp: 14:52:07
   [REQ 2] KM Echoed back: hello kernel | Timestamp: 14:52:08
   Disconnected from Driver.
   ```
5. **Stop and remove the driver:**
   ```bat
   sc stop MyFirstDriver
   sc delete MyFirstDriver                       & rem if installed with sc create
   pnputil /delete-driver oemNN.inf /uninstall    & rem if installed with pnputil (see pnputil /enum-drivers)
   ```

### Kernel debug output

The **Debug** build logs through `KdPrint` (for example `MyDriver: echoing once: ...`). The Release
build compiles these calls out. To see the messages in WinDbg or Sysinternals DebugView (*Capture Kernel*),
set the Debug Print Filter and reboot:

```bat
reg add "HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\Debug Print Filter" /v DEFAULT /t REG_DWORD /d 0xF
```

## Possible extensions

- Inverted-call / pending-IRP model so the driver can push events to user mode without polling
- `METHOD_IN_DIRECT` / `METHOD_OUT_DIRECT` for large transfers
- Porting to KMDF for PnP and power-management plumbing
