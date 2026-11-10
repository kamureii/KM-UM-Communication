#include <iostream>
#include <conio.h>
#include <limits>
#include <windows.h>
#include "Shared.h"

static void WaitForKey()
{
	std::cout << "Press any key to exit..." << std::endl;
	(void)_getch();
}

// Sends one request; the driver writes its reply into the same payload.
static bool SendRequest(HANDLE hDevice, DWORD ioctl, MESSAGE_PAYLOAD& payload)
{
	DWORD bytesReturned = 0;
	if (!DeviceIoControl(hDevice, ioctl, &payload, sizeof(payload), &payload, sizeof(payload), &bytesReturned, nullptr))
	{
		std::cerr << "DeviceIoControl failed: " << GetLastError() << std::endl;
		return false;
	}
	if (bytesReturned != sizeof(payload))
	{
		std::cerr << "Unexpected reply size: " << bytesReturned << " bytes" << std::endl;
		return false;
	}
	payload.Message[MAX_MSG_LENGTH - 1] = '\0';
	return true;
}

int main()
{
	std::cout << "--- User Mode App ---" << std::endl;

	HANDLE hDevice = CreateFileW(
		DRIVER_USER_PATH,
		GENERIC_READ | GENERIC_WRITE,
		0,
		nullptr,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		nullptr
	);

	if (hDevice == INVALID_HANDLE_VALUE)
	{
		DWORD error = GetLastError();
		std::cerr << "Failed to open device: " << error << std::endl;
		if (error == ERROR_FILE_NOT_FOUND)
			std::cerr << "Is the driver loaded? (sc start MyFirstDriver)" << std::endl;
		else if (error == ERROR_ACCESS_DENIED)
			std::cerr << "The device is restricted to Administrators: run this app elevated." << std::endl;
		WaitForKey();
		return 1;
	}
	std::cout << "Device opened successfully." << std::endl;

	MESSAGE_PAYLOAD inputData = { 0 };
	char temp[MAX_MSG_LENGTH] = { 0 };

	std::cout << "Enter a message (max " << MAX_MSG_LENGTH - 1 << " characters): ";
	std::cin.getline(temp, MAX_MSG_LENGTH);
	if (std::cin.fail() && !std::cin.eof())
	{
		// Input was longer than the buffer: keep the truncated part and drop the rest of the line.
		std::cin.clear();
		std::cin.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');
		std::cout << "Message truncated to " << MAX_MSG_LENGTH - 1 << " characters." << std::endl;
	}

	strcpy_s(inputData.Message, temp);
	std::cout << "[REQ 1] Sending to KM: " << inputData.Message << std::endl;

	if (SendRequest(hDevice, IOCTL_ECHO_ONCE, inputData))
	{
		std::cout << "[REQ 1] KM Echoed back: " << inputData.Message << "\n\n";
	}

	std::cout << "[REQ 2] Starting 1 s periodic request. Press 'ESC' to stop..." << std::endl;

	for (;;)
	{
		// Only react to ESC typed into this console, not to key presses in other windows.
		if (_kbhit() && _getch() == 27)
			break;

		memset(inputData.Message, 0, sizeof(inputData.Message));
		strcpy_s(inputData.Message, temp);

		if (SendRequest(hDevice, IOCTL_ECHO_REPEAT, inputData))
		{
			std::cout << "[REQ 2] KM Echoed back: " << inputData.Message << std::endl;
		}
		Sleep(1000);
	}

	CloseHandle(hDevice);
	std::cout << "Disconnected from Driver." << std::endl;
	return 0;
}
