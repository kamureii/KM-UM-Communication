#include <ntddk.h>
#include <ntstrsafe.h>
#include <wdmsec.h>
#include "Shared.h"

DRIVER_INITIALIZE DriverEntry;
DRIVER_UNLOAD DriverUnload;
_Dispatch_type_(IRP_MJ_CREATE)
_Dispatch_type_(IRP_MJ_CLOSE)
DRIVER_DISPATCH DriverDispatchCreateClose;
_Dispatch_type_(IRP_MJ_DEVICE_CONTROL)
DRIVER_DISPATCH DriverDispatchIoctl;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, DriverUnload)
#pragma alloc_text(PAGE, DriverDispatchCreateClose)
#pragma alloc_text(PAGE, DriverDispatchIoctl)
#endif

// Class GUID for the device object's security settings (used by IoCreateDeviceSecure).
// {B2830AFB-1253-4FFA-B866-587BF6D47A5F}
static const GUID MYDRIVER_DEVICE_CLASS_GUID =
{ 0xb2830afb, 0x1253, 0x4ffa, { 0xb8, 0x66, 0x58, 0x7b, 0xf6, 0xd4, 0x7a, 0x5f } };

NTSTATUS DriverDispatchCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	UNREFERENCED_PARAMETER(DeviceObject);
	PAGED_CODE();

	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

NTSTATUS DriverDispatchIoctl(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	UNREFERENCED_PARAMETER(DeviceObject);
	PAGED_CODE();

	NTSTATUS status = STATUS_INVALID_DEVICE_REQUEST;
	ULONG_PTR bytesIO = 0;

	PIO_STACK_LOCATION pStackLocation = IoGetCurrentIrpStackLocation(Irp);
	ULONG controlCode = pStackLocation->Parameters.DeviceIoControl.IoControlCode;
	ULONG inLen = pStackLocation->Parameters.DeviceIoControl.InputBufferLength;
	ULONG outLen = pStackLocation->Parameters.DeviceIoControl.OutputBufferLength;

	switch (controlCode)
	{
	case IOCTL_ECHO_ONCE: {
		if (inLen == sizeof(MESSAGE_PAYLOAD) && outLen == sizeof(MESSAGE_PAYLOAD)) {
			PMESSAGE_PAYLOAD payload = (PMESSAGE_PAYLOAD)Irp->AssociatedIrp.SystemBuffer;
			// The buffer comes from user mode: never trust it to be NUL-terminated.
			payload->Message[MAX_MSG_LENGTH - 1] = '\0';
			KdPrint(("MyDriver: echoing once: %s\n", payload->Message));

			// The SystemBuffer already holds the message, so echoing it back needs no copy.
			bytesIO = sizeof(MESSAGE_PAYLOAD);
			status = STATUS_SUCCESS;
		}
		else {
			status = STATUS_INVALID_BUFFER_SIZE;
		}
		break;
	}
	case IOCTL_ECHO_REPEAT: {
		if (inLen == sizeof(MESSAGE_PAYLOAD) && outLen == sizeof(MESSAGE_PAYLOAD)) {
			PMESSAGE_PAYLOAD payload = (PMESSAGE_PAYLOAD)Irp->AssociatedIrp.SystemBuffer;
			payload->Message[MAX_MSG_LENGTH - 1] = '\0';

			LARGE_INTEGER sysTime, locTime;
			TIME_FIELDS timeFields;
			KeQuerySystemTime(&sysTime);
			ExSystemTimeToLocalTime(&sysTime, &locTime);
			RtlTimeToTimeFields(&locTime, &timeFields);

			char temp[MAX_MSG_LENGTH];
			RtlStringCbCopyA(temp, sizeof(temp), payload->Message);
			// A long message is truncated (STATUS_BUFFER_OVERFLOW) but always stays NUL-terminated.
			RtlStringCbPrintfA(payload->Message, sizeof(payload->Message), "%s | Timestamp: %02d:%02d:%02d",
				temp, timeFields.Hour, timeFields.Minute, timeFields.Second);

			bytesIO = sizeof(MESSAGE_PAYLOAD);
			status = STATUS_SUCCESS;
		}
		else
			status = STATUS_INVALID_BUFFER_SIZE;
		break;
	}
	}
	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = bytesIO;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return status;
}

void DriverUnload(PDRIVER_OBJECT pDriverObject)
{
	PAGED_CODE();

	UNICODE_STRING symLink;
	RtlInitUnicodeString(&symLink, DRIVER_SYMLINK_NAME);

	IoDeleteSymbolicLink(&symLink);
	if (pDriverObject->DeviceObject != NULL) 	{
		IoDeleteDevice(pDriverObject->DeviceObject);
	}
	KdPrint(("MyDriver: driver unloaded\n"));
}

NTSTATUS DriverEntry(PDRIVER_OBJECT pDriverObject, PUNICODE_STRING pRegistryPath)
{
	UNREFERENCED_PARAMETER(pRegistryPath);
	NTSTATUS status;
	PDEVICE_OBJECT DeviceObject;
	UNICODE_STRING devName;
	UNICODE_STRING symLink;

	pDriverObject->MajorFunction[IRP_MJ_CREATE] = DriverDispatchCreateClose;
	pDriverObject->MajorFunction[IRP_MJ_CLOSE] = DriverDispatchCreateClose;
	pDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DriverDispatchIoctl;
	pDriverObject->DriverUnload = DriverUnload;

	RtlInitUnicodeString(&devName, DRIVER_DEVICE_NAME);
	RtlInitUnicodeString(&symLink, DRIVER_SYMLINK_NAME);

	// Only SYSTEM and Administrators may open the device. FILE_DEVICE_SECURE_OPEN applies
	// that ACL to every open, including paths below the device name (\\.\MyDriver\anything).
	status = IoCreateDeviceSecure(pDriverObject, 0, &devName, FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN,
		FALSE, &SDDL_DEVOBJ_SYS_ALL_ADM_ALL, &MYDRIVER_DEVICE_CLASS_GUID, &DeviceObject);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("MyDriver: failed to create device: 0x%08X\n", status));
		return status;
	}

	status = IoCreateSymbolicLink(&symLink, &devName);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("MyDriver: failed to create symbolic link: 0x%08X\n", status));
		IoDeleteDevice(DeviceObject);
		return status;
	}

	KdPrint(("MyDriver: driver loaded successfully\n"));

	return STATUS_SUCCESS;
}
