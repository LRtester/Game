
#pragma warning(push, 3)

#include <stdio.h>

#include <Windows.h>	// Don't want the warnings from Windows.h

#pragma warning(pop) 

#include <stdint.h> // type definitions

#include "Main.h"

HWND gGameWindow;

BOOL gGameIsRunning;

GAMEBITMAP gBackBuffer;

GAMEPERFDATA gPerformanceData;

int __stdcall WinMain(_In_ HINSTANCE Instance, _In_opt_ HINSTANCE PreviousInstance, _In_ PSTR CommandLine, _In_ INT CmdShow) {

	UNREFERENCED_PARAMETER(Instance);
	UNREFERENCED_PARAMETER(PreviousInstance);
	UNREFERENCED_PARAMETER(CommandLine);
	UNREFERENCED_PARAMETER(CmdShow);
	
	MSG Message = { 0 };
	int64_t FrameStart = 0;
	int64_t FrameEnd = 0;
	int64_t ElapsedMicrosecondsPerFrameAccumulatorRaw = 0;
	int64_t ElapsedMicrosecondsPerFrame = 0;
	int64_t ElapsedMicrosecondsPerFrameAccumulatorCooked = 0;
	HMODULE NtDllModuleHandle;

	if ((NtDllModuleHandle = GetModuleHandleA("ntdll.dll")) == NULL) {
		MessageBoxA(NULL, "Couldn't load ntdll.dll!", "Error!", MB_ICONEXCLAMATION | MB_OK);
		goto Exit;
	}

	if ((NtQueryTimerResolution = (_NtQueryTimerResolution)GetProcAddress(NtDllModuleHandle, "NtQueryTimerResolution")) == NULL) {
		MessageBoxA(NULL, "Couldn't find NtQueryTimerResolution function in ntdll.dll!!", "Error!", MB_ICONEXCLAMATION | MB_OK);
		goto Exit;
	}

	NtQueryTimerResolution(&gPerformanceData.MinimumTimerResolution, &gPerformanceData.MaximumTimerResolution, &gPerformanceData.CurrentTimerResolution);



	if (GameIsAlreadyRunning() == TRUE) { // No extra instances of this program!!!
		MessageBoxA(NULL, "Another instance of this program is already running!", "Error!", MB_ICONEXCLAMATION | MB_OK);
		goto Exit;
	}

	if (CreateMainGameWindow() != ERROR_SUCCESS) {
		goto Exit;
	}

	QueryPerformanceFrequency((LARGE_INTEGER*)&gPerformanceData.PerfFrequency);

	gPerformanceData.DisplayDebugInfo = TRUE;

	gBackBuffer.BitmapInfo.bmiHeader.biSize = sizeof(gBackBuffer.BitmapInfo.bmiHeader);
	gBackBuffer.BitmapInfo.bmiHeader.biWidth = GAME_RES_WIDTH;
	gBackBuffer.BitmapInfo.bmiHeader.biHeight = GAME_RES_HEIGHT;
	gBackBuffer.BitmapInfo.bmiHeader.biBitCount = GAME_BPP;
	gBackBuffer.BitmapInfo.bmiHeader.biCompression = BI_RGB;
	gBackBuffer.BitmapInfo.bmiHeader.biPlanes = 1;
	if ((gBackBuffer.Memory = VirtualAlloc(NULL, GAME_DRAWING_AREA_MEMORY_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE)) == NULL) {
		// If memory fails to be allocated, 0 will be returned
		MessageBoxA(NULL, "Failed to allocate memory for drawing surface!", "Error!", MB_ICONEXCLAMATION | MB_OK);
		goto Exit;
	}

	memset(gBackBuffer.Memory, 0x7f, GAME_DRAWING_AREA_MEMORY_SIZE);

	

	// Messages
	gGameIsRunning = TRUE;

	while (gGameIsRunning == TRUE) { // Game Loop

		QueryPerformanceCounter((LARGE_INTEGER*)&FrameStart);

		while (PeekMessageA(&Message, gGameWindow, 0, 0, PM_REMOVE)) { // Message Queue Loop
			DispatchMessageA(&Message);
		}

		ProcessPlayerInput();

		RenderFrameGraphics();

		
		QueryPerformanceCounter((LARGE_INTEGER*)&FrameEnd);

		ElapsedMicrosecondsPerFrame = FrameEnd - FrameStart;

		ElapsedMicrosecondsPerFrame *= 1000000;

		ElapsedMicrosecondsPerFrame /= gPerformanceData.PerfFrequency;

		gPerformanceData.TotalFramesRendered++;

		ElapsedMicrosecondsPerFrameAccumulatorRaw += ElapsedMicrosecondsPerFrame;

		while (ElapsedMicrosecondsPerFrame <= TARGET_MICROSECONDS_PER_FRAME) { // Need to figure out what is wrong 

			ElapsedMicrosecondsPerFrame = FrameEnd - FrameStart;

			ElapsedMicrosecondsPerFrame *= 1000000;

			ElapsedMicrosecondsPerFrame /= gPerformanceData.PerfFrequency;

			QueryPerformanceCounter((LARGE_INTEGER*)&FrameEnd);

			if (ElapsedMicrosecondsPerFrame <= ((int64_t)TARGET_MICROSECONDS_PER_FRAME - gPerformanceData.CurrentTimerResolution)) {
				Sleep(1);
			}
		}

		ElapsedMicrosecondsPerFrameAccumulatorCooked += ElapsedMicrosecondsPerFrame;

		if (gPerformanceData.TotalFramesRendered % CALCULATE_AVG_FPS_EVERY_X_FRAMES == 0) {

			gPerformanceData.RawFPSAverage = 1.0f / ((ElapsedMicrosecondsPerFrameAccumulatorRaw / CALCULATE_AVG_FPS_EVERY_X_FRAMES) * 0.000001f);

			gPerformanceData.CookedFPSAverage = 1.0f / ((ElapsedMicrosecondsPerFrameAccumulatorCooked / CALCULATE_AVG_FPS_EVERY_X_FRAMES) * 0.000001f);

			ElapsedMicrosecondsPerFrameAccumulatorRaw = 0;

			ElapsedMicrosecondsPerFrameAccumulatorCooked = 0;
		}
	}

Exit:
	return 0;
}


LRESULT CALLBACK MainWindowProc(_In_ HWND WindowHandle, _In_  UINT Message, _In_  WPARAM WParam, _In_  LPARAM LParam) {

	LRESULT Result = 0;

	switch (Message)
	{
		case WM_CLOSE:
		{
			gGameIsRunning = FALSE;
			PostQuitMessage(0); // Passes a message with 0 which ends the program
			break;
		}
		default:
		{
			return DefWindowProc(WindowHandle, Message, WParam, LParam);
		}
	}
	return Result;
}

DWORD CreateMainGameWindow(void) {
	DWORD Result = ERROR_SUCCESS;

	// Window Class
	WNDCLASSEXA WindowClass = { 0 };

	WindowClass.cbSize = sizeof(WNDCLASSEX);
	WindowClass.style = 0;
	WindowClass.lpfnWndProc = MainWindowProc;
	WindowClass.cbClsExtra = 0;
	WindowClass.cbWndExtra = 0;
	WindowClass.hInstance = GetModuleHandleA(NULL);
	WindowClass.hIcon = LoadIconA(NULL, IDI_APPLICATION);
	WindowClass.hCursor = LoadCursorA(NULL, IDC_ARROW);
	WindowClass.hbrBackground = CreateSolidBrush(RGB(255, 0, 255)); // Hideous color in order to stand out
	WindowClass.lpszMenuName = NULL;
	WindowClass.lpszClassName = GAME_NAME "_WINDOWCLASS";
	WindowClass.hIconSm = LoadIconA(NULL, IDI_APPLICATION);

	if (RegisterClassExA(&WindowClass) == 0) {
		Result = GetLastError();
		MessageBoxA(NULL, "Window Registration Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
		goto Exit;
	}

	// Window Handle
	gGameWindow = CreateWindowExA(0, GAME_NAME "_WINDOWCLASS", "Window Title", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 640, 480, NULL, NULL, WindowClass.hInstance, NULL);

	if (gGameWindow == NULL) {
		Result = GetLastError();
		MessageBoxA(NULL, "Window Creation Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
		goto Exit;
	}

	gPerformanceData.MonitorInfo.cbSize = sizeof(MONITORINFO);

	if (GetMonitorInfoA(MonitorFromWindow(gGameWindow, MONITOR_DEFAULTTOPRIMARY), &gPerformanceData.MonitorInfo) == 0) {
		Result = ERROR_MONITOR_NO_DESCRIPTOR;
		goto Exit;
	}

	gPerformanceData.MonitorWidth = gPerformanceData.MonitorInfo.rcMonitor.right - gPerformanceData.MonitorInfo.rcMonitor.left;
	gPerformanceData.MonitorHeight = gPerformanceData.MonitorInfo.rcMonitor.bottom - gPerformanceData.MonitorInfo.rcMonitor.top;

	if (SetWindowLongPtrA(gGameWindow, GWL_STYLE, (WS_OVERLAPPEDWINDOW | WS_VISIBLE) & ~WS_OVERLAPPEDWINDOW) == 0) {
		Result = GetLastError();

		goto Exit;
	}

	if (SetWindowPos(gGameWindow,
		HWND_TOP,
		gPerformanceData.MonitorInfo.rcMonitor.left,
		gPerformanceData.MonitorInfo.rcMonitor.top,
		gPerformanceData.MonitorWidth,
		gPerformanceData.MonitorHeight,
		SWP_NOOWNERZORDER | SWP_FRAMECHANGED) == 0) {

		Result = GetLastError();

		goto Exit;
	}

Exit:
	return Result;
}

BOOL GameIsAlreadyRunning(void) {
	HANDLE Mutex = NULL;
	Mutex = CreateMutexA(NULL, FALSE, GAME_NAME "_GameMutex");
	if (GetLastError() == ERROR_ALREADY_EXISTS) {
		return TRUE;
	}
	else {
		return FALSE;
	}
}

void ProcessPlayerInput(void) {
	int16_t EscapeKeyIsDown = GetAsyncKeyState(VK_ESCAPE);

	int16_t DebugKeyIsDown = GetAsyncKeyState(VK_F1);

	static int16_t DebugKeyWasDown = 0;

	if (EscapeKeyIsDown) {
		SendMessageA(gGameWindow, WM_CLOSE, 0, 0);
	}

	if (DebugKeyIsDown && !DebugKeyWasDown) {
		gPerformanceData.DisplayDebugInfo = !gPerformanceData.DisplayDebugInfo;
	}

	DebugKeyWasDown = DebugKeyIsDown;
}

void RenderFrameGraphics(void) {

	//memset(gBackBuffer.Memory, 0xFF, (GAME_RES_WIDTH * GAME_RES_HEIGHT) * 4);

	PIXEL32 Pixel = { 0 };

	Pixel.Blue = 0x7f;
	Pixel.Green = 0;
	Pixel.Red = 0;
	Pixel.Alpha = 0xff;

	for (int x = 0; x < GAME_RES_HEIGHT * GAME_RES_WIDTH; x++) {
		memcpy_s((PIXEL32*)gBackBuffer.Memory + x, sizeof(PIXEL32), &Pixel, sizeof(PIXEL32));
	}

	static int32_t ScreenX = 25;

	static int32_t ScreenY = 25;

	int32_t StartingScreenPixel = ((GAME_RES_WIDTH * GAME_RES_HEIGHT) - GAME_RES_WIDTH) - \
		(GAME_RES_WIDTH * ScreenY) + ScreenX;

	for (int32_t y = 0; y < 16; y++) {

		for (int32_t x = 0; x < 16; x++) {

			memset((PIXEL32*)gBackBuffer.Memory + (uintptr_t)StartingScreenPixel + x - ((uintptr_t)GAME_RES_WIDTH * y), 0xff, sizeof(PIXEL32));

		}
	}

	// Screensaver junk lul
	/*ScreenX++; ScreenY++;

	if (ScreenX + 16 > GAME_RES_WIDTH) {
		ScreenX = 0;
	}
	if (ScreenY + 16 > GAME_RES_HEIGHT) {
		ScreenY = 0;
	}*/

	HDC DeviceContext = GetDC(gGameWindow);

	StretchDIBits(DeviceContext,
		0,
		0,
		gPerformanceData.MonitorWidth,
		gPerformanceData.MonitorHeight,
		0,
		0,
		GAME_RES_WIDTH,
		GAME_RES_HEIGHT,
		gBackBuffer.Memory,
		&gBackBuffer.BitmapInfo,
		DIB_RGB_COLORS,
		SRCCOPY);

	if (gPerformanceData.DisplayDebugInfo == TRUE) {

		SelectObject(DeviceContext, (HFONT)GetStockObject(ANSI_FIXED_FONT));

		char DebugTextBuffer[64] = { 0 };

		sprintf_s(DebugTextBuffer, _countof(DebugTextBuffer), "FPS Raw:    %.01f", gPerformanceData.RawFPSAverage);

		TextOutA(DeviceContext, 0, 0, DebugTextBuffer, (int)strlen(DebugTextBuffer));

		sprintf_s(DebugTextBuffer, _countof(DebugTextBuffer), "FPS Cooked: %.01f", gPerformanceData.CookedFPSAverage);

		TextOutA(DeviceContext, 0, 13, DebugTextBuffer, (int)strlen(DebugTextBuffer));

		sprintf_s(DebugTextBuffer, _countof(DebugTextBuffer), "Min Timer Res: %.02f", gPerformanceData.MinimumTimerResolution / 10000.0f);

		TextOutA(DeviceContext, 0, 26, DebugTextBuffer, (int)strlen(DebugTextBuffer));

		sprintf_s(DebugTextBuffer, _countof(DebugTextBuffer), "Max Timer Res: %.02f", gPerformanceData.MaximumTimerResolution / 10000.0f);

		TextOutA(DeviceContext, 0, 39, DebugTextBuffer, (int)strlen(DebugTextBuffer));

		sprintf_s(DebugTextBuffer, _countof(DebugTextBuffer), "Cur Timer Res: %.02f", gPerformanceData.CurrentTimerResolution / 10000.0f);

		TextOutA(DeviceContext, 0, 52, DebugTextBuffer, (int)strlen(DebugTextBuffer));
	}

	ReleaseDC(gGameWindow, DeviceContext);
}
