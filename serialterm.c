/*
 * Serial line terminal program
 * characters can be displayed as in hex, decimal and ascii
 * characters can be seperated by space, tab, newline or 'nothing'
 * local echo can be switched on or off
 *
 * by Albrecht Schmidt, Lancaster University - Oct 2001
 * http://www.comp.lancs.ac.uk/~albrecht
 * albrecht@comp.lancs.ac.uk
 * based on an example from Robert Mashlan -
 *                    see http://r2m.com/~rmashlan/
 *
 * Modifications 2014-2022 Benjamin Green
 * https://e42.uk/
 */

#include "StdAfx.h"

// global vars - to avoid parameter passing over thread
// I am lazy I know ...
char exename[1024];
char logname[1024];
int DisplayMode, separator, echo, logtofile;

// https://msdn.microsoft.com/en-us/library/windows/desktop/ms682022%28v=vs.85%29.aspx

void cls(HANDLE hConsole) {
	COORD coordScreen = { 0, 0 }; // home for the cursor
	DWORD cCharsWritten;
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	DWORD dwConSize;

	// Get the number of character cells in the current buffer.
	if (!GetConsoleScreenBufferInfo(hConsole, &csbi)) {
		printf("GetConsoleScreenBufferInfo fail GetLastError() %i\n", (int)GetLastError());
		return;
	}

	dwConSize = csbi.dwSize.X * csbi.dwSize.Y;

	// Fill the entire screen with blanks.
	if (!FillConsoleOutputCharacter(hConsole, (TCHAR) ' ', dwConSize, coordScreen, &cCharsWritten)) {
		printf("FillConsoleOutputCharacter fail GetLastError() %i\n", (int)GetLastError());
		return;
	}

	// Get the current text attribute.
	if (!GetConsoleScreenBufferInfo( hConsole, &csbi)) {
		printf("GetConsoleScreenBufferInfo fail GetLastError() %i\n", (int)GetLastError());
		return;
	}

	// Set the buffer's attributes accordingly.
	if (!FillConsoleOutputAttribute(hConsole, csbi.wAttributes, dwConSize, coordScreen, &cCharsWritten)) {
		printf("FillConsoleOutputAttribute fail GetLastError() %i\n", (int)GetLastError());
		return;
	}
	// Put the cursor at its home coordinates.
	SetConsoleCursorPosition(hConsole, coordScreen);
}

/*
	HANDLE hStdout;
	hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
	cls(hStdout);
	return 0;
*/

void help() {
	printf("%s port [speed] [DisplayMode] [Separator] [Echo] [logfilename]\n", exename);
	printf("        port ::= com1 | com2 | com3 | com4 | com5 | com6\n");
	printf("        speed::= 300 | 4800 | 9600 | 19200 | 38400 | 57600 | 115200 | 230400\n");
	printf("                                     ^^^^^\n");
	printf("        DisplayMode::= ascii | hex | decimal\n");
	printf("                       ^^^^^\n");
	printf("        Separator::= empty | space | newline | tab\n");
	printf("                     ^^^^^\n");
	printf("        Echo::= no | yes\n");
	printf("                ^^\n");
	printf("        logfilename::= <anyname> (if not provided no log is written)\n\n");
	printf("Example: %s com1 115200 hex space no log.txt\n", exename);
	printf("    open the terminal on port com1 with 115200 bit/s, print hex code of\n");
	printf("    incoming characters, seperate them by space, no local echo, log to log.txt\n");
	printf("Example: %s com2 19200 decimal tab yes\n", exename);
	printf("    open the terminal on port com2 with 19200 bit/s, print decimal code\n");
	printf("    of incoming characters, seperate them by tabs, do local echo, no logfile\n");
}


void PrintError(LPCSTR str) {
	LPVOID lpMessageBuffer;
	int error = GetLastError();
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM,
		NULL,
		error,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), //The user default language
		(LPTSTR) &lpMessageBuffer,
		0,
		NULL
	);
	printf("%s: (%d) %s\n\n", str, error, (char *)lpMessageBuffer);
	help();
	LocalFree(lpMessageBuffer);
}

DWORD CALLBACK ConInThread(HANDLE h) {
	// starts a user input thread using the console
	//
	// takes console characters and sends them to the com port

	OVERLAPPED ov;
	HANDLE hconn = GetStdHandle(STD_INPUT_HANDLE);
	BOOL quit = FALSE;
	char kb;
	INPUT_RECORD b1;

	ZeroMemory(&ov,sizeof(ov));
	ov.hEvent = CreateEvent(NULL,FALSE,FALSE,NULL);
	if(ov.hEvent == INVALID_HANDLE_VALUE) {
		PrintError("E001_CreateEvent failed");
		SetCommMask(h,0);
		return 0;
	}

	SetConsoleMode(hconn,0);
	printf("press Esc or Ctrl+C to terminate\n");

	do {
		char buf[10];
		DWORD read = 0;

		SetConsoleMode(hconn,0);

		// wait for user to type something
		WaitForSingleObject(hconn,INFINITE);

		// read the console buffer
		if (!ReadConsoleInput(hconn,&b1,1,&read)) {
			PrintError("E002_ReadConsoleInput failed...");
			quit = TRUE;
		}

		kb = b1.Event.KeyEvent.uChar.AsciiChar;

		if (b1.EventType == KEY_EVENT) {
			//printf("read=%i, b1.keyevent: 0x%x vk=%x keyMode=%s  ", read, kb, b1.Event.KeyEvent.wVirtualKeyCode, b1.Event.KeyEvent.bKeyDown ? "DOWN" : "UP");

			// When key is released
			if (!b1.Event.KeyEvent.bKeyDown) {
				// check if it is special.
				if (b1.Event.KeyEvent.wVirtualKeyCode == 0x71) { // F2
					//Serial_setBaudRate(h, 1200);
					continue;
				}
				if (b1.Event.KeyEvent.wVirtualKeyCode == 0x74) { // F5
					//Serial_setBaudRate(h, 9600);
					continue;
				}
			}
		}

		buf[0] = kb;

		if (read) {
			DWORD write=1;
			// check for Esc
			// for debug purpose to find out the key value ...
			// printf("Val 0x%x\n", kb);

			// terminate when Esc pressed
			if (buf[0] == 0x1b /* ESC */ ||
					// buf[0] == 0x11 /* Ctrl+Q */ ||
					// buf[0] == 0x18 /* Ctrl+X */ ||
					buf[0] == 0x3 /* Ctrl+C */) {
				quit = TRUE;
				break;
			} else {
				if (buf[0] != 0x0  && b1.Event.KeyEvent.bKeyDown) {
					// send it to the com port
					if (!WriteFile(h,buf,write,&write,&ov)) {
						if (GetLastError() == ERROR_IO_PENDING) {
							if (!GetOverlappedResult(h,&ov,&write,TRUE)) {
								PrintError("E003_GetOverlappedResult failed");
								quit = TRUE;
							}
						}
						// make the local output - echo is global var ...
						if (echo == 1) printf("%c", (unsigned char)buf[0]);
					} else {
						PrintError("E004_WriteFile failed");
						quit = TRUE;
					}
				}
			}
		}
	} while (!quit);

	// tell terminal thread to quit
	if (!SetCommMask(h, 0)) {
		printf("SetCommMask-GetLastError: %i\n", (int)GetLastError());
	}
	return 0;
}


void Terminal(HANDLE h) {
	DWORD mask;
	DWORD id, i;
	OVERLAPPED ov;
	FILE * stream = NULL;
	HRESULT hRes = 0x80000000;
	DWORD keepGoing = (DWORD)TRUE;
	HANDLE hconin = INVALID_HANDLE_VALUE;
	HANDLE hconn = INVALID_HANDLE_VALUE;
	
	hconn = GetStdHandle(STD_OUTPUT_HANDLE);

	cls(hconn);

	if (logtofile) {
		stream = fopen(logname, "w");
	}

	hconin = CreateThread(NULL, 0, ConInThread, h, 0, &id);
	if (hconin == INVALID_HANDLE_VALUE) {
	   PrintError("E005_CreateThread failed");
	   return;
	}
	CloseHandle(hconin);  // don't need this handle

	ZeroMemory(&ov,sizeof(ov));

	// create event for overlapped I/O
	ov.hEvent = CreateEvent(NULL,FALSE,FALSE,NULL);
	if (ov.hEvent == INVALID_HANDLE_VALUE) {
		PrintError("E006_CreateEvent failed");
	}

	// wait for received characters

	while (keepGoing) {
		if (!SetCommMask(h,EV_RXCHAR))
			PrintError("E007_SetCommMask failed");
		if (!WaitCommEvent(h,&mask,&ov)) {
			//printf("COMM0 %d\n", mask);
			DWORD e = GetLastError();
			if (e == ERROR_IO_PENDING) {
				DWORD r;
				hRes = WaitForSingleObjectEx(ov.hEvent, 5000, FALSE);
				switch (hRes) {
				case WAIT_OBJECT_0:
					//printf("WaitCommEvent - WAIT_OBJECT_0\n");
					if (!GetOverlappedResult(h, &ov, &r, TRUE)) {
						PrintError("E008_GetOverlappedResult failed");
						keepGoing = (DWORD)FALSE;
					}
					//printf("COMM1 %d\n", mask);
					break;
				case WAIT_TIMEOUT:
					//printf("WaitCommEvent - WAIT_TIMEOUT\n");
					break;
				}
			} else {
				PrintError("E009_WaitCommEvent failed");
				break;
			}
		}

		//fprintf(stdout, "mask=%x ", mask);

		// if no event, then UI thread terminated with SetCommMask(h,0)
		if (mask == 0 && hRes != WAIT_TIMEOUT) {
			printf("Breaking (hRes = %x)\n", (unsigned int)hRes);
			break;
		}

		if (mask & EV_RXCHAR) {
			char buf[1024];
			DWORD read;
			HRESULT hRes;
			do {
				read = 0;
				if (!ReadFile(h, buf, sizeof(buf), &read, &ov) ) {
					if (GetLastError() == ERROR_IO_PENDING) {
						//printf("WaitForSingleObjectEx READFILE S\n");
						hRes = WaitForSingleObjectEx(ov.hEvent, 5000, FALSE);
						//printf("WaitForSingleObjectEx READFILE E\n");
						switch (hRes) {
						case WAIT_OBJECT_0:
							//printf("WAIT_OBJECT_0\n");
							if (!GetOverlappedResult(h, &ov, &read, TRUE)) {
								PrintError("E010_GetOverlappedResult failed");
							}
							break;
						case WAIT_TIMEOUT:
							printf("ReadFile WAIT_TIMEOUT\n");
							read = 0;
							break;
						}
						//printf("read %d\n", read);
					} else {
						PrintError("E011_ReadFile failed");
						break;
					}
				} else {
					printf("noasync\n");
				}
				if (read) {
					for (i = 0; i<read; i++) {
						if (DisplayMode==0) printf("0x%x", (unsigned char)buf[i]);
						if (DisplayMode==1) printf("%c", (unsigned char)buf[i]);
						if (DisplayMode==2) printf("%i", (unsigned char)buf[i]);
						if (separator==0) printf(" ");
						if (separator==1) printf("\n");
						if (separator==2) printf("\t");
						// separator==3 - do nothing!

						if (logtofile) {
							// print to the logfile
							if (DisplayMode==0) fprintf(stream,"0x%x", (unsigned char)buf[i]);
							if (DisplayMode==1) fprintf(stream,"%c", (unsigned char)buf[i]);
							if (DisplayMode==2) fprintf(stream,"%i", (unsigned char)buf[i]);
							if (separator==0) fprintf(stream," ");
							if (separator==1) fprintf(stream,"\n");
							if (separator==2) fprintf(stream,"\t");
							// separator==3 - do nothing!
						}
					}
				}
			} while(read);
		}
		mask = 0;
	}
	CloseHandle(ov.hEvent);  // close the event

	if (stream) {
		fclose(stream);
	}
}

void wait4keypressed() {
	HANDLE hStdin;
	DWORD cNumRead, fdwMode, fdwSaveOldMode, i, quit;
	INPUT_RECORD irInBuf[128];

	// Get the standard input handle.

	hStdin = GetStdHandle(STD_INPUT_HANDLE);
	if (hStdin == INVALID_HANDLE_VALUE) {
		printf("Problem:GetStdHandle");
	}

	// Save the current input mode, to be restored on exit.
	if (!GetConsoleMode(hStdin, &fdwSaveOldMode)) {
		printf("Problem:GetConsoleMode");

		// Enable the window and mouse input events.

		fdwMode = ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT;
		if (!SetConsoleMode(hStdin, fdwMode)) {
			printf("Problem:SetConsoleMode");
		}

		// Loop to read and handle the input events.
		quit = 1;

		while (quit) {
			// Wait for the events.
			if (!ReadConsoleInput(hStdin, irInBuf, 128, &cNumRead)) {
				printf("\nProblem:ReadConsoleInput\n");
			}

			// check if key pressed ...
			for (i = 0; i < cNumRead; i++) {
				if (irInBuf[i].EventType == KEY_EVENT) { quit=0; }
			}
		}
		// rest the console mode
		SetConsoleMode(hStdin, fdwSaveOldMode);
	}
}

int main(int argc, char ** argv) {
	// copy the name of the executable into a global variable
	strcpy(exename, argv[0]);

	if (argc > 1) {
		// open port for overlapped I/O
		HANDLE h = CreateFileA((LPCSTR)argv[1], GENERIC_READ | GENERIC_WRITE, 0, NULL,
			OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);

		if (h == INVALID_HANDLE_VALUE) {
			PrintError("E012_Failed to open port");
		} else {
			// set timeouts
			COMMTIMEOUTS cto = { 2, 1, 1, 0, 0 };
			DCB dcb;
			if (!SetCommTimeouts(h,&cto)) {
				PrintError("E013_SetCommTimeouts failed");

				// set DCB
				memset(&dcb, 0, sizeof(dcb));
				dcb.DCBlength = sizeof(dcb);
				dcb.BaudRate = 19200;
				if (argc > 2 && atoi(argv[2])) {
					dcb.BaudRate = atoi(argv[2]);
				}
				DisplayMode = 1;
				if(argc > 3) {
					if (argv[3][0] == 'h') DisplayMode = 0;
					if (argv[3][0] == 'a') DisplayMode = 1;
					if (argv[3][0] == 'd') DisplayMode = 2;
				}
				separator = 3;
				if (argc > 4) {
					if (argv[4][0] == 's') separator = 0;
					if (argv[4][0] == 'n') separator = 1;
					if (argv[4][0] == 't') separator = 2;
					if (argv[4][0] == 'e') separator = 3;
				}
				echo = 0;
				if (argc > 5) {
					if (argv[5][0] == 'n') echo = 0;
					if (argv[5][0] == 'y') echo = 1;
				}
				logtofile = 0;
				if(argc > 6) {
					logtofile = 1;
					strcpy(logname, argv[6]);
				}

				dcb.fBinary = 1;
				dcb.fDtrControl = DTR_CONTROL_ENABLE;
				dcb.fRtsControl = RTS_CONTROL_ENABLE;
				// dcb.fOutxCtsFlow = 1;
				// dcb.fRtsControl = DTR_CONTROL_HANDSHAKE;

				dcb.Parity = NOPARITY;
				dcb.StopBits = ONESTOPBIT;
				dcb.ByteSize = 8;

				if (!SetCommState(h,&dcb)) {
					PrintError("E014_SetCommState failed");
				}

				// start terminal
				Terminal(h);
				CloseHandle(h);
			}
		}
	} else {
		printf("Commandline Serial Terminal - May 2014\n");
		//printf("Commandline Serial Terminal - Version 1.1 by A. Schmidt, Oct 2001\n");
		//printf("Lancaster University - http://www.comp.lancs.ac.uk/~albrecht/\n\n");
		help();
		wait4keypressed();
	}
	return 0;
}

