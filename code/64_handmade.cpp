#include <windows.h>
#include <stdint.h>
#include <xinput.h>
// NOTE For a refresher you can go to https://youtu.be/w7ay7QXmo_o?t=4582 
// explainning well all basic code here
typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;
// NOTE(axel): When using bool, compiler during optimization has to convert 
// the binary to 1 or 0 value. With this type we only care about any bit value or 0.
typedef uint32 bool32;
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

//NOTE(axel): RARE TRICK to swap a function from a library to your own, id est if 
// you can't be 100% be sure that the function will be available and cause an error
// you create function in your file that will swap the real one to your own, tipically 
// you will have an if statement to know if its available then if not provide yours, otherwise just 
// use the real one. As C doesn't allowed several function on the same name and signature, you have 
// to trick the compiler. (i.e XInputGetState_ / XInputGetState)
#define X_INPUT_GET_STATE(name) DWORD name(DWORD dwUserIndex,  XINPUT_STATE* pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{
	return(ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub;
#define  XInputGetState XInputGetState_


// XInputSetState
#define X_INPUT_SET_STATE(name) DWORD name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub)
{
	return(ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_set_state  *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

global_variable bool GlobalRunning; // static initialize to zero automatically
global_variable Win32_offscreen_buffer GlobalBackBuffer;

internal void LoadXInput(void)
{
	HMODULE XInputLibrary = LoadLibraryA("xinput1_3.dll");
	if (XInputLibrary)
	{
		XInputGetState = (x_input_get_state *) GetProcAddress(XInputLibrary, "XInputGetState");
		XInputSetState = (x_input_set_state *) GetProcAddress(XInputLibrary, "XInputSetState");

	}
}

Win32_window_dimensions Win32GetWindowDimensions(HWND Window) {
	Win32_window_dimensions Result;
	HDC DeviceContext = GetDC(Window);
	RECT ClientRect;
	GetClientRect(Window, &ClientRect);
	Result.Width = ClientRect.right - ClientRect.left;
	Result.Height= ClientRect.bottom - ClientRect.top;
	return(Result);
}

internal void RenderWeirdGradient(Win32_offscreen_buffer Buffer, int BlueOffset, int RedOffset) {
	int Width  = Buffer.Width;
	int Height = Buffer.Height;
	int Pitch  = Buffer.Width * Buffer.BytesPerPixel;
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

internal void 
Win32ResizeDIBSection(Win32_offscreen_buffer *Buffer, int Width, int Height)
{
	if (Buffer->Memory)
	{
		VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
	}
	Buffer->Width = Width;
	Buffer->Height = Height;
	Buffer->BytesPerPixel = 4;
	// NOTE(axel) When biHeight is negative it's a clue to Windows that 
	// its a top-down DIB (see doc). Meaning that the first three bytes 
	// are the color for the top left pixel, not the bottom-left.
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
				  0, 0, WindowWidth, WindowHeight,
				  0, 0, Buffer.Width, Buffer.Height,
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
		// NOTE(axel): Normally SYSKEY case is handle by DEFAULT but if you handle it here
		// you loose the default handle.
		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		case WM_KEYDOWN:
		case WM_KEYUP:
		{
			uint32 VKCode = WParam;
			// NOTE(axel): easy mistake -> Watch out on precedence operations '!=' evaluate
			// before '&' so without proper parenthesis your evaluation will be wrong.
			bool WasDown = ((LParam & (1 << 30)) != 0);
			bool IsDown = ((LParam & (1 << 31)) == 0);

			if (WasDown != IsDown)
				{
				if (VKCode == 'A') 
				{
					OutputDebugStringA("A\n");
				}
				else if (VKCode == 'W')
				{
					OutputDebugStringA("W\n");
				}
				else if (VKCode == 'D')
				{
					OutputDebugString("D\n");
				}
				else if (VKCode == 'S')
				{
					OutputDebugStringA("S\n");
				}
				else if (VKCode == VK_SPACE)
				{
					OutputDebugStringA("SPACE\n");
				}
				else if (VKCode == VK_ESCAPE)
				{
					GlobalRunning = false;
					OutputDebugStringA("ESC\n");
				}

				bool32 AltKeyIsDown = LParam & (1 << 29);

				if (VKCode == VK_F4 && AltKeyIsDown)
				{
					OutputDebugString("ALT");
					GlobalRunning = false;
				}
			}
			break;
		}
		case WM_DESTROY:
		{
			GlobalRunning = false;
			OutputDebugStringA("DESTROY\n");
		} break;
		case WM_SIZE:
		{
			OutputDebugStringA("SIZE\n");
		} break;
		case WM_CLOSE:
		{
			OutputDebugStringA("CLOSE\n");
			GlobalRunning = false;
		} break;
		case WM_ACTIVATEAPP:
		{
			OutputDebugStringA("ACTIVATEAPP\n");
		} break;
		case WM_PAINT:
		{
			// NOTE(axel): Windows has the right to call it whenever it needs to.
			// EndPaint say to Windows that you have finish to Paint it, if you don't 
			// it will keep calling WM_PAINT. https://youtu.be/w7ay7QXmo_o?t=4867
			PAINTSTRUCT Paint;
			HDC DeviceContext = BeginPaint(Window, &Paint);
			int X = Paint.rcPaint.left;
			int Y = Paint.rcPaint.top;
			int Width = Paint.rcPaint.right - Paint.rcPaint.left;
			int Height = Paint.rcPaint.bottom - Paint.rcPaint.top;

			Win32_window_dimensions Dimensions = Win32GetWindowDimensions(Window);
			Win32DisplayBufferInWindow(DeviceContext, Dimensions.Width, Dimensions.Height, 
										GlobalBackBuffer);
			EndPaint(Window, &Paint);
		}
		default:
		{
			result = DefWindowProc(Window, Message, WParam, LParam);
		} break;
	}
	return result;
}

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
					 PSTR lpCmdLine, INT nCmdShow)
{
	LoadXInput();
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

		Win32ResizeDIBSection(&GlobalBackBuffer, 800, 500);

		if (Window)
		{
			HDC DeviceContext = GetDC(Window);
			int XOffset = 0;
			int YOffset = 0;

			GlobalRunning = true;
			while (GlobalRunning)
			{
				MSG Message;
				while(PeekMessageA(&Message, 0, 0, 0, PM_REMOVE))
				{
					if (Message.message == WM_QUIT)
					{
						GlobalRunning = false;
					}
					TranslateMessage(&Message);
					DispatchMessage(&Message);
				}

				for(uint16 ControllerIndex = 0;
					 ControllerIndex < XUSER_MAX_COUNT;
					 ++ControllerIndex)
				{

					XINPUT_STATE ControllerState;

					if (XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
					{
						// MessageText:
						// The operation completed successfully (Controller connected).
						XINPUT_GAMEPAD* Pad = &ControllerState.Gamepad;

						bool DPadUp = Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP;
						bool DPadDown = Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN;
						bool DPadRight = Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;
						bool DPadLeft = Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT;
						bool Start = Pad->wButtons & XINPUT_GAMEPAD_START;
						bool Back = Pad->wButtons & XINPUT_GAMEPAD_BACK;
						bool LeftThumb = Pad->wButtons & XINPUT_GAMEPAD_LEFT_THUMB;
						bool RightThumb = Pad->wButtons & XINPUT_GAMEPAD_RIGHT_THUMB;
						bool LeftShoulder = Pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER;
						bool RightShoulder = Pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER;	
						bool AButton = Pad->wButtons & XINPUT_GAMEPAD_A;
						bool BButton = Pad->wButtons & XINPUT_GAMEPAD_B;
						bool XButton = Pad->wButtons & XINPUT_GAMEPAD_X;
						bool YButton = Pad->wButtons & XINPUT_GAMEPAD_Y;

						if (AButton)
						{
							OutputDebugStringA("A \n");
						} 
						else if(BButton)
						{
						}
						else if (XButton)
						{
						}
						else if (DPadUp)
						{
						}
						else if (DPadDown)
						{
						}
						else if (DPadRight)
						{
						}
						else if (DPadLeft)
						{
						}
						else if (Start)
						{
						}
						else if (Back)
						{
						}
						else if (LeftThumb)
						{
						}
						else if (RightThumb)
						{
						}
						else if (LeftShoulder)
						{
						}
						else if (RightShoulder)
						{
						}

						uint16 StickX = Pad->sThumbLX;
						uint16 StickY = Pad->sThumbLY;

					}
					else
					{
						// Controller is not connected
					}
					
				}
				RenderWeirdGradient(GlobalBackBuffer, XOffset, YOffset);
				Win32_window_dimensions Dimensions = Win32GetWindowDimensions(Window);
				Win32DisplayBufferInWindow(DeviceContext, 
				Dimensions.Width, Dimensions.Height, 
											GlobalBackBuffer);
				ReleaseDC(Window, DeviceContext);

				++XOffset;
				YOffset += 2;
			}
		}
	}
	return 0;
}