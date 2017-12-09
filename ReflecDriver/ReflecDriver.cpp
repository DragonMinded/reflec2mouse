#ifndef _WIN32_WINNT            // Specifies that the minimum required platform is Windows Vista.
#define _WIN32_WINNT 0x0600     // Change this to the appropriate value to target other versions of Windows.
#endif

#include <stdio.h>
#include <tchar.h>
#include <windows.h>

// Resolution of the display's input
#define HORIZONTAL_RESOLUTION 48
#define VERTICAL_RESOLUTION 76

// Define to 1 to print debug info, 0 to turn it off
#define VERBOSE 0

void GetDesktopResolution(int& horizontal, int& vertical)
{
   RECT desktop;
   const HWND hDesktop = GetDesktopWindow();
   GetWindowRect(hDesktop, &desktop);
   horizontal = desktop.right;
   vertical = desktop.bottom;
}

HANDLE OpenSerial( const _TCHAR *arg, int baud )
{
    HANDLE hSerial = CreateFile(arg, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	if (hSerial == INVALID_HANDLE_VALUE) { return hSerial; }

	int rate;

	switch( baud )
	{
		case 4800:
			rate = CBR_4800;
			break;
		case 9600:
			rate = CBR_9600;
			break;
		case 19200:
			rate = CBR_19200;
			break;
		case 38400:
			rate = CBR_38400;
			break;
		case 57600:
			rate = CBR_57600;
			break;
		case 115200:
			rate = CBR_115200;
			break;
		default:
			rate = CBR_9600;
			break;
	}

    DCB dcbSerialParams = {0};

    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
    dcbSerialParams.BaudRate = rate;
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity = NOPARITY;
	dcbSerialParams.fOutxCtsFlow = 0;
	dcbSerialParams.fOutxDsrFlow = 0;
	dcbSerialParams.fDtrControl = DTR_CONTROL_DISABLE;
	dcbSerialParams.fDsrSensitivity = 0;
	dcbSerialParams.fOutX = 0;
	dcbSerialParams.fInX = 0;
	dcbSerialParams.fRtsControl = RTS_CONTROL_DISABLE;

    SetCommState(hSerial, &dcbSerialParams);

    COMMTIMEOUTS timeouts = { 0 };

    timeouts.ReadIntervalTimeout = 1;
    timeouts.ReadTotalTimeoutConstant = 1;
    timeouts.ReadTotalTimeoutMultiplier = 1;

    SetCommTimeouts(hSerial, &timeouts);
    return hSerial;
}

bool ValidatePacket(unsigned char * packet) {
	if (packet[0] == 'U' && packet[1] == 'T' && packet[2] == 'L') {
		// Check checksum
		unsigned int check = 0xFF;
		for (unsigned int i = 3; i < 20; i++) {
			check = check + packet[i];
		}
		check = (check + 0xA0) & 0xFF;
		// Check against checksum byte
		return check == packet[20];
	} else {
		return false;
	}
}

void SeparateAxis(unsigned char * packet, bool *horizontal, bool *vertical) {
    // Bytes 3 - 11 are bits for vertical set.
	// Bytes 14 - 19 are bits for horizontal set.
	for (unsigned int y = 0; y < VERTICAL_RESOLUTION; y++) {
		vertical[VERTICAL_RESOLUTION - 1 - y] = ((packet[3 + (y / 8)] >> (y % 8)) & 0x1) != 0;
	}
	for (unsigned int x = 0; x < HORIZONTAL_RESOLUTION; x++) {
		horizontal[x] = ((packet[14 + (x / 8)] >> (x % 8)) & 0x1) != 0;
	}
}

void PrintTouchPattern(bool *horizontal, bool *vertical) {
	printf("[");

	for (unsigned int y = 0; y < VERTICAL_RESOLUTION; y++) {
		printf("%C", vertical[y] ? '#' : ' ');
	}

	printf("] [");

	for (unsigned int x = 0; x < HORIZONTAL_RESOLUTION; x++) {
		printf("%C", horizontal[x] ? '#' : ' ');
	}
	
	printf("]");
}

void GetLikelyClickPosition(bool *horizontal, bool *vertical, double *x, double *y, bool *click) {
	int minX = -1;
	int minY = -1;
	int maxX = -1;
	int maxY = -1;

	for (unsigned int yp = 0; yp < VERTICAL_RESOLUTION; yp++) {
		for (unsigned int xp = 0; xp < HORIZONTAL_RESOLUTION; xp++) {
			if (horizontal[xp] && vertical[yp]) {
				maxX = xp;
				maxY = yp;

				if (minX == -1) {
					minX = xp;
				}
				if (minY == -1) {
					minY = yp;
				}
			}
		}
	}

	if (minX == -1 || maxX == -1) {
		minX = max(minX, maxX);
		maxX = max(minX, maxX);
	}

	if (minY == -1 || maxY == -1) {
		minY = max(minY, maxY);
		maxY = max(minY, maxY);
	}

	if (minY == -1 || minX == -1) {
		*click = false;
		return;
	} else {
		*click = true;
	}

	*x = (((double)maxX + (double)minX) / 2.0) / (double)HORIZONTAL_RESOLUTION;
	*y = (((double)maxY + (double)minY) / 2.0) / (double)VERTICAL_RESOLUTION;
}

void MouseMove(int x, int y) {
	SetCursorPos(x, y);
}

void MouseDown() {
	INPUT Input = {0};
    Input.type = INPUT_MOUSE;
	Input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
	if (SendInput(1, &Input, sizeof(INPUT)) != 1) {
		fprintf( stderr, "Failed to send mouse down, error %ld!", GetLastError());
	}
}

void MouseUp() {
	INPUT Input = {0};
    Input.type = INPUT_MOUSE;
	Input.mi.dwFlags = MOUSEEVENTF_LEFTUP | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
	if (SendInput(1, &Input, sizeof(INPUT)) != 1) {
		fprintf( stderr, "Failed to send mouse up, error %ld!", GetLastError());
	}
}

int _tmain(int argc, _TCHAR* argv[])
{
    /* Ensure command is good */
    if( argc < 2 )
    {
        fprintf( stderr, "Missing serial port argument!\n" );
        return 1;
    }

	HANDLE serial = OpenSerial(argv[1], 38400);
	PurgeComm( serial, PURGE_RXABORT | PURGE_RXCLEAR | PURGE_TXABORT | PURGE_TXCLEAR );

	unsigned char packet[22] = { 0 };
	bool vertical[VERTICAL_RESOLUTION] = { false };
	bool horizontal[HORIZONTAL_RESOLUTION] = { false };

	int resX, resY;
	bool lastState = false;
	GetDesktopResolution(resX, resY);

	while( true ) {
		// Try to read the next byte
		DWORD length = 0;
		ReadFile( serial, packet + 21, 1, &length, 0 );
		if (length == 1)
		{
			// Shift it into the packet area
			memmove(packet, packet + 1, 21);
		}

		// See if the packet is good
		if (ValidatePacket(packet)) {
			// Get the touched axis
			SeparateAxis(packet, horizontal, vertical);

			if (VERBOSE) {
				// Print the grid!
				PrintTouchPattern(horizontal, vertical);
			}

			// Find the detected click point, move the mouse there.
			double x, y;
			bool click;
			GetLikelyClickPosition(horizontal, vertical, &x, &y, &click);
			if (click) {
				if (VERBOSE) {
					printf(" (%lf, %lf)", x, y);
				}

				// Have fun and set the mouse position
				MouseMove(
					(int)((double)resX * x),
					(int)((double)resY * y)
				);
			}

			// Simulate a click on press, simulate an unclick on release
			if (click != lastState) {
				if (click) {
					if (VERBOSE) {
						printf(" LEFT CLICK");
					}
					MouseDown();
				} else {
					if (VERBOSE) {
						printf(" LEFT RELEASE");
					}
					MouseUp();
				}

				// Remember that we're held or released
				lastState = click;
			}

			if (VERBOSE) {
				printf("\n");
			}

			// Wipe out the packet to start again
			memset(packet, 0, 22);
		}
	}
	return 0;
}

