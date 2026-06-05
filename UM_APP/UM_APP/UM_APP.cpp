#include <iostream>
#include <windows.h>
#include "Shared.h"

int main()
{
	std::cout << "--- User Mode App ---" << std::endl;
	// Simulate some work in the user-mode application
	HANDLE hDevice = CreateFile(
		L"\\\\.\\MyDriver",
		GENERIC_READ | GENERIC_WRITE,
		0,
		nullptr,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		nullptr
	);

	if (hDevice == INVALID_HANDLE_VALUE)
	{
		std::cerr << "Failed to open device: " << GetLastError() << std::endl;
		system("pause");
		return 1;
	}
	std::cout << "Device opened successfully." << std::endl;
	
	MESSAGE_PAYLOAD inputData = { 0 };
	char temp[MAX_MSG_LENGTH];
	DWORD bytesReturned = 0;

	std::cin.getline(temp, MAX_MSG_LENGTH); // Wait for user input before sending the first request
	std::cout << "[REQ 1] Sending to KM: " << temp << std::endl;

	if (DeviceIoControl(
		hDevice,
		IOCTL_ECHO_ONCE,
		&inputData,
		sizeof(inputData),
		&inputData,
		sizeof(inputData),
		&bytesReturned,
		nullptr
	))
	{
		std::cout << "[REQ 1] KM Echoed back: " << inputData.Message << "\n\n";
	}

	std::cout << "[REQ 2] Starting 10ms periodic request. Press 'ESC' to stop..." << inputData.Message << std::endl;

	while (!(GetAsyncKeyState(VK_ESCAPE) & 0x8000))
	{
		memset(inputData.Message, 0, sizeof(inputData.Message));

		strcpy_s(inputData.Message, temp);
		
		if (DeviceIoControl(
			hDevice,
			IOCTL_ECHO_REPEAT,
			&inputData,
			sizeof(inputData),
			&inputData,
			sizeof(inputData),
			&bytesReturned,
			nullptr
		))
		{
			std::cout << "[REQ 2] KM Echoed back: " << inputData.Message << std::endl;
		}
		Sleep(1000);
	}

	CloseHandle(hDevice);
	system("Disconnected from Driver.");
	return 0;
}