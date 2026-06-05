#include <ntddk.h>
#include <ntstrsafe.h>
#include "Shared.h"

#define DEVICE_NAME L"\\Device\\MyDriver"
#define SYM_LINK_NAME L"\\??\\MyDriver"

NTSTATUS DriverDispatchCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	UNREFERENCED_PARAMETER(DeviceObject);
	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

NTSTATUS DriverDispatchIoctl(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	UNREFERENCED_PARAMETER(DeviceObject);

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
			KdPrint("Echoing once: %s\n", payload->Message);
			RtlStringCchCopyA(payload->Message, MAX_MSG_LENGTH, payload->Message);
			Irp->IoStatus.Status = STATUS_SUCCESS;
			Irp->IoStatus.Information = sizeof(MESSAGE_PAYLOAD);

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

			char temp[MAX_MSG_LENGTH - 1];
			RtlStringCbCopyA(temp, MAX_MSG_LENGTH, payload->Message);
			RtlStringCbPrintfA(payload->Message, MAX_MSG_LENGTH, "%s Timestamp: %02d : %02d : %02d", temp, timeFields.Hour, timeFields.Minute, timeFields.Second);

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
	UNICODE_STRING symLink;
	RtlInitUnicodeString(&symLink, SYM_LINK_NAME);

	IoDeleteSymbolicLink(&symLink);
	if (pDriverObject->DeviceObject != NULL) 	{
		IoDeleteDevice(pDriverObject->DeviceObject);
	}
	KdPrint("Driver Unloaded\n");
}

NTSTATUS DriverEntry(PDRIVER_OBJECT pDriverObject, PUNICODE_STRING pRegistryPath)
{
	UNREFERENCED_PARAMETER(pRegistryPath);
	NTSTATUS status;
	PDEVICE_OBJECT DeviceObject;
	UNICODE_STRING devName;
	UNICODE_STRING symLink;

	RtlInitUnicodeString(&devName, DEVICE_NAME);
	RtlInitUnicodeString(&symLink, SYM_LINK_NAME);

	status = IoCreateDevice(pDriverObject, 0, &devName, FILE_DEVICE_UNKNOWN, 0, FALSE, &DeviceObject);
	if (!NT_SUCCESS(status))
	{
		KdPrint("Failed to create device: %X\n", status);
		return status;
	}

	status = IoCreateSymbolicLink(&symLink, &devName);
	if (!NT_SUCCESS(status))
	{
		KdPrint("Failed to create symbolic link: %X\n", status);
		IoDeleteDevice(DeviceObject);
		return status;
	}

	pDriverObject->MajorFunction[IRP_MJ_CREATE] = DriverDispatchCreateClose;
	pDriverObject->MajorFunction[IRP_MJ_CLOSE] = DriverDispatchCreateClose;
	pDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DriverDispatchIoctl;
	pDriverObject->DriverUnload = DriverUnload;

	KdPrint("Calculator Driver Loaded Successfully\n");

	return STATUS_SUCCESS;
}