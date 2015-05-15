#pragma once
#include <wtypes.h>


#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
//
extern int	appDebugf(const wchar_t* fmt, ...);
extern int	appMsgBox(const wchar_t* caption, const wchar_t* text, int type, HWND hWnd = NULL);
extern int	appGetLastErrorMsg();
extern int appInit();
extern void	appExit();
extern void	appOpenConsole();
extern void	appCloseConsole();
extern double appGetTimeMs();

class Mutex{
public:
	Mutex();
	~Mutex();
	bool Open(const wchar_t* name = L"");
	void Close();
private:
	HANDLE	m_hMutex;
};

class Window{
public:
	Window();
	~Window();
	int Create(const wchar_t* title, const RECT& rect, WNDPROC msgCallback, bool centered_to_screen);
	void Destroy();
	HWND GetHandle();

private:
	HWND m_hWnd;
};

