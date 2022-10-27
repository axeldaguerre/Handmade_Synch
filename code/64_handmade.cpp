#include <windows.h>
#include <stdint.h>

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

#define internal static // function file scoped
#define global_variable static
#define local_persist static


struct Win32_offscreen_buffer { // Bitmap
	BITMAPINFO Info;
	void* Memory;
	HDC DeviceContext;
	int Width;
	int Height;
	int BytesPerPixel;
};

struct Win32_window_dimensions {
	int Width;
	int Height;
};

global_variable bool Running; // static initialize to zero automatically
global_variable Win32_offscreen_buffer GlobalBackBuffer;

Win32_window_dimensions GetWindowDimensions(HWND Window) {
	Win32_window_dimensions Result;
	HDC DeviceContext = GetDC(Window);
	RECT ClientRect;
	GetClientRect(Window, &ClientRect);
	Result.Width = ClientRect.right - ClientRect.left;
	Result.Height= ClientRect.bottom - ClientRect.top;
	return(Result);
}

internal void RenderWeirdGradient(Win32_offscreen_buffer Buffer, int BlueOffset, int RedOffset) {
	int Width = Buffer.Width;
	int Height = Buffer.Height;
	int Pitch = Buffer.Width * Buffer.BytesPerPixel;
	uint8 *Row = (uint8*)Buffer.Memory;
	for (int Y = 0;
		Y < Height;
		Y++)
	{
		uint32 *Pixel = (uint32 *)Row;

		for (int X = 0;
			X < Width;
			X++)
		{
			uint8 Blue = X + BlueOffset;
			uint8 Red = Y + RedOffset;

			*Pixel++ = (Red << 16 | Blue);
		}
			Row += Pitch;
	}
}

internal void Win32ResizeDIBSection(Win32_offscreen_buffer *Buffer, int Width, int Height)
{
	if (Buffer->Memory)
	{
		VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
	}
	Buffer->Width = Width;
	Buffer->Height = Height;
	Buffer->BytesPerPixel = 4;

	Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
	Buffer->Info.bmiHeader.biHeight = -Buffer->Height;
	Buffer->Info.bmiHeader.biWidth = Buffer->Width;
	Buffer->Info.bmiHeader.biPlanes = 1;
	Buffer->Info.bmiHeader.biBitCount = 32;
	Buffer->Info.bmiHeader.biCompression = BI_RGB;

	int BitmapMemorySize = (Buffer->Width * Buffer->Height) * Buffer->BytesPerPixel;
	Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);
}

internal void Win32DisplayBufferInWindow(HDC DeviceContext, 
										 int WindowWidth, int WindowHeight,
										 Win32_offscreen_buffer Buffer)
{
	StretchDIBits(DeviceContext,
				  0, 0, Buffer.Width, Buffer.Height,
				  0, 0, WindowWidth, WindowHeight,
				  Buffer.Memory,
				  &Buffer.Info,
				  DIB_RGB_COLORS, SRCCOPY);
}

LRESULT Win32MainWindowCallBack(
	HWND Window,
	UINT Message,
	WPARAM WParam,
	LPARAM LParam)
{
	LRESULT result = 0;
	switch (Message)
	{
		case WM_DESTROY:
		{
			Running = false;
			OutputDebugStringA("DESTROY\n");
		} break;
		case WM_SIZE:
		{
			Win32_window_dimensions WindowDimensions = GetWindowDimensions(Window);
			Win32ResizeDIBSection(&GlobalBackBuffer, WindowDimensions.Width, WindowDimensions.Width);
			OutputDebugStringA("SIZE\n");
		} break;
		case WM_CLOSE:
		{
			OutputDebugStringA("CLOSE\n");
			Running = false;
		} break;
		case WM_ACTIVATEAPP:
		{
			OutputDebugStringA("ACTIVATEAPP\n");
		} break;
		case WM_PAINT:
		{
			OutputDebugStringA("PAINT\n");
			PAINTSTRUCT Paint;
			HDC DeviceContext = BeginPaint(Window, &Paint);

			int X = Paint.rcPaint.left;
			int Y = Paint.rcPaint.top;

			int Width = Paint.rcPaint.right - Paint.rcPaint.left;
			int Height = Paint.rcPaint.bottom - Paint.rcPaint.top;

			Win32_window_dimensions Dimensions = GetWindowDimensions(Window);
			Win32DisplayBufferInWindow(DeviceContext, Dimensions.Height, Dimensions.Width, GlobalBackBuffer);
			EndPaint(Window, &Paint);
		}
		default:
		{
			OutputDebugStringA("Default\n");
			result = DefWindowProc(Window, Message, WParam, LParam);
		} break;
	}
	return result;
}

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
					 PSTR lpCmdLine, INT nCmdShow)
{
	WNDCLASS WindowClass = {};
	WindowClass.hInstance = hInstance;
	WindowClass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
	WindowClass.lpfnWndProc = Win32MainWindowCallBack;
	WindowClass.lpszClassName = "EngineWindowClassName";
	if (RegisterClass(&WindowClass))
	{
		HWND Window = CreateWindowExA(
			0,
			WindowClass.lpszClassName,
			"Engine",
			WS_OVERLAPPEDWINDOW | WS_VISIBLE,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			0,
			0,
			hInstance,
			0);
		if (Window)
		{
			HDC DeviceContext = GetDC(Window);
			int XOffset = 0;
			int YOffset = 0;

			Running = true;
			while (Running)
			{
				MSG Message;
				while(PeekMessageA(&Message, 0, 0, 0, PM_REMOVE))
				{
					if (Message.message == WM_QUIT)
					{
						Running = false;
					}
					TranslateMessage(&Message);
					DispatchMessage(&Message);
				}

				RenderWeirdGradient(GlobalBackBuffer, XOffset, YOffset);
				Win32_window_dimensions Dimensions = GetWindowDimensions(Window);
				Win32DisplayBufferInWindow(DeviceContext, Dimensions.Width, Dimensions.Height, GlobalBackBuffer);
				ReleaseDC(Window, DeviceContext);

				++XOffset;
			}
		}
	}
	return 0;
}