#include "platform.h"
#include "InputInterface.h"
#include <stdio.h>

//
Window				inputWindow;
InputInterface		inputInterface;
bool				running = true;

//
LRESULT CALLBACK InputWndCallback(HWND hwnd, UINT MessageID, WPARAM wParam, LPARAM lParam){
	switch (MessageID){
	default:break;
	case WM_CREATE:
		appInit();
		appOpenConsole();
		if (!inputInterface.init(hwnd)){
			return false;
		}
		break;
	case WM_CLOSE:
		inputWindow.Destroy();
		break;
	case WM_DESTROY:
		inputInterface.shutdown();
		appCloseConsole();
		running = false;
		PostQuitMessage(0);
		break;
	case WM_INPUT:
		inputInterface.handleMessage((HRAWINPUT*)&lParam);
		break;
	case WM_INPUT_DEVICE_CHANGE:
		if (wParam == GIDC_ARRIVAL){
			inputInterface.deviceAdded((HANDLE)lParam);
		}
		if (wParam == GIDC_REMOVAL){
			inputInterface.deviceRemoved((HANDLE)lParam);
		}
		break;
	}
	return DefWindowProc(hwnd, MessageID, wParam, lParam);
}

// this function is called at 120hz
void DoUpdate(){
	//  Check dual shock 4 state
	static DualShock4State old_dsState[4] = { 0 };
	for (unsigned int i = 0; i< 4; i++){
		DualShock4State new_dsState = { 0 };
		if (inputInterface.getDualShock4State(i, new_dsState)){
			if (new_dsState.packetNum > old_dsState[i].packetNum){
				printf("DualShock4(%i):DPad(0x%01x),buttons(0x%04x),sticks L(%i, %i) R(%i,%i),T(0x%x02,(0x%02x))\n",
					i,
					new_dsState.DPad,
					new_dsState.Button,
					new_dsState.StickLeft[0], new_dsState.StickLeft[1],
					new_dsState.StickRight[0], new_dsState.StickRight[1],
					new_dsState.TriggerLeft, new_dsState.TriggerLeft);
				if (new_dsState.Button == (DualShock4Button::kOptions | DualShock4Button::kShare)){
					inputWindow.Destroy();
				}
				if ((new_dsState.Button == (DualShock4Button::kSquare | DualShock4Button::kCircle))){
					inputInterface.setDualShock4Rumble(i, 255 / 3, 255 / 3);
				}
				else{
					if (new_dsState.Button & (DualShock4Button::kSquare)){
						inputInterface.setDualShock4Rumble(i, 255 / 3, 0 );
					}
					if (new_dsState.Button & (DualShock4Button::kCircle)){
						inputInterface.setDualShock4Rumble(i, 0, 255 / 3);
					}
				}
				if (!(new_dsState.Button & (DualShock4Button::kSquare | DualShock4Button::kCircle))){
					inputInterface.setDualShock4Rumble(i, 0, 0);
				}

				old_dsState[i] = new_dsState;
			}
		}
	}


	// check xinput controller state
	static XINPUT_STATE old_xState[4] = { 0 };
	for (unsigned int i = 0; i < 4; i++){
		XINPUT_STATE new_xState = { 0 };
		if (inputInterface.getXInputState(i, new_xState)){
			if (new_xState.dwPacketNumber > old_xState[i].dwPacketNumber){
				printf("360 ctlr(%i):buttons(0x%04x),sticks L(%i, %i)R(%i,%i),T(0x%x02,(0x%02x)\n",
					i,
					new_xState.Gamepad.wButtons,
					new_xState.Gamepad.sThumbLX, new_xState.Gamepad.sThumbLY,
					new_xState.Gamepad.sThumbRX, new_xState.Gamepad.sThumbRY,
					new_xState.Gamepad.bLeftTrigger, new_xState.Gamepad.bRightTrigger);
				if (new_xState.Gamepad.wButtons  == (XINPUT_GAMEPAD_BACK | XINPUT_GAMEPAD_START)){
					inputWindow.Destroy();
				}
				if ((new_xState.Gamepad.wButtons == (XINPUT_GAMEPAD_X | XINPUT_GAMEPAD_B))){
					inputInterface.setXInputRumble(i, 65535 / 3, 65535 / 3);
				}
				else{
					if (new_xState.Gamepad.wButtons & (XINPUT_GAMEPAD_X)){
						inputInterface.setXInputRumble(i, 65535 / 3, 0);
					}
					if (new_xState.Gamepad.wButtons & (XINPUT_GAMEPAD_B)){
						inputInterface.setXInputRumble(i, 0, 65535 / 3);
					}
				}
				if (!(new_xState.Gamepad.wButtons & (XINPUT_GAMEPAD_X | XINPUT_GAMEPAD_B))){
					inputInterface.setXInputRumble(i, 0, 0);
				}

				old_xState[i] = new_xState;
			}
		}
	}
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, wchar_t* cmdline, int nShow){
	MSG msg;
	RECT wndRect = { 0, 0, 500, 400 };

	double CurrentTime = appGetTimeMs();
	double LastTime = appGetTimeMs();
	const double UpdateTime = 1.0 / 120.0;
	if (!inputWindow.Create(L"Input dummy window", wndRect, InputWndCallback, true)){
		return false;
	}
	while (running){
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)){
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		CurrentTime = appGetTimeMs();
		const double dt = CurrentTime - LastTime;
		if (dt >= UpdateTime){
			LastTime = CurrentTime;
			DoUpdate();
		}
		Sleep(0);
	}
	return (int)msg.wParam;
}
