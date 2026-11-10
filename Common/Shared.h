#pragma once

//
// Contract shared by the kernel driver (MyFirstDriver) and the user-mode client (UM_APP).
// Both projects include this single file so the IOCTL codes and payload layout cannot drift.
//

#ifndef _KERNEL_MODE
#include <windows.h>
#include <winioctl.h>
#endif

#define DRIVER_DEVICE_NAME   L"\\Device\\MyDriver"
#define DRIVER_SYMLINK_NAME  L"\\??\\MyDriver"
#define DRIVER_USER_PATH     L"\\\\.\\MyDriver"

// Callers must open the device with read and write access to send these requests.
#define IOCTL_ECHO_ONCE   CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)
#define IOCTL_ECHO_REPEAT CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)

#define MAX_MSG_LENGTH 128

// No pointers or size-dependent types, so the layout is identical for 32-bit and 64-bit callers.
typedef struct _MESSAGE_PAYLOAD
{
	char Message[MAX_MSG_LENGTH];
} MESSAGE_PAYLOAD, * PMESSAGE_PAYLOAD;
