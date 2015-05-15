#include "InputInterface.h"
#include <stdio.h>

//
__declspec(align(32))
struct DualShock4ControllerData{
	unsigned int Direction : 4;
	unsigned int Buttons : 12;
};
__forceinline int Clamp(const int& value, const int& min_value, const int& max_value){
	int result = value;
	if (value > max_value)result = max_value;
	if (value < min_value)result = min_value;
	return result;
}

inline void processKeyboard(KeyboardState& state, RAWKEYBOARD* pRawKeyboard){
	const unsigned short index = pRawKeyboard->VKey;
	if (pRawKeyboard->Flags == 3 || pRawKeyboard->Flags == RI_KEY_BREAK){
		state.Key[index] = KeyState::kUp;
	}
	else if (pRawKeyboard->Flags == 2 || pRawKeyboard->Flags == RI_KEY_MAKE){
		state.Key[index] = KeyState::kDown;
	}

	printf("keyboard key pressed : 0x%02x %s\n", index, (state.Key[index] == KeyState::kDown) ? "Down" : "Up");
}
inline void processMouse(MouseState& state, RAWMOUSE* pRawMouse){
	state.ScrollWheel = 0;
	state.Buttons = 0;
	if (MOUSE_MOVE_RELATIVE == pRawMouse->usFlags){
		state.Axis.x += pRawMouse->lLastX;
		state.Axis.y += pRawMouse->lLastY;
	}
	if (RI_MOUSE_WHEEL & pRawMouse->usButtonFlags){
		state.ScrollWheel = (short)pRawMouse->usButtonData;
	}
	else{
		state.Buttons = pRawMouse->ulButtons;
	}

	printf("Mouse state : axis(%i, %i), button(0x%04x), Scoll(%i)\n", state.Axis.x, state.Axis.y, state.Buttons, state.ScrollWheel);
}


InputInterface::InputInterface(){
	m_lastKeyboardKeyCode = 0;
}
InputInterface::~InputInterface(){

}

bool InputInterface::init(HWND hWnd){
	RAWINPUTDEVICE Rid[3];

	// this enabled mouse detection
	Rid[0].usUsagePage = 0x01;
	Rid[0].usUsage = 0x02;
	Rid[0].dwFlags = RIDEV_DEVNOTIFY;
	Rid[0].hwndTarget = hWnd;

	// this enabled keyboard detection
	Rid[1].usUsagePage = 0x01;
	Rid[1].usUsage = 0x06;
	Rid[1].dwFlags = RIDEV_DEVNOTIFY;
	Rid[1].hwndTarget = hWnd;

	// this enabled joystick detection
	Rid[2].usUsagePage = 0x01;
	Rid[2].usUsage = 0x05;
	Rid[2].dwFlags = RIDEV_DEVNOTIFY;
	Rid[2].hwndTarget = hWnd;

	//  register input
	if (!RegisterRawInputDevices(Rid, 3, sizeof(RAWINPUTDEVICE))){
		appGetLastErrorMsg();
		return false;
	}

	m_Mutex.Open();
	ZeroMemory(&m_keyboardState, sizeof(KeyboardState));
	ZeroMemory(&m_mouseState, sizeof(MouseState));
	m_Mutex.Close();
	return true;
}
void InputInterface::shutdown(){

	// close connections
	m_Mutex.Open();
	for (unsigned int i = 0; i < 4; i++){
		if (m_DualShock4Info[i].Connected){
			setDualShock4LightBarColor(i, 0, 0, 0);
			setDualShock4Rumble(i, 0, 0);
			CloseHandle(m_DualShock4Info[i].hConnection);
			m_DualShock4Info[i].hConnection = NULL;
		}
	}
	m_Mutex.Close();

	RAWINPUTDEVICE Rid[3];

	Rid[0].usUsagePage = 0x01;
	Rid[0].usUsage = 0x02;
	Rid[0].dwFlags = RIDEV_REMOVE;
	Rid[0].hwndTarget = 0;

	Rid[1].usUsagePage = 0x01;
	Rid[1].usUsage = 0x06;
	Rid[1].dwFlags = RIDEV_REMOVE;
	Rid[1].hwndTarget = 0;

	Rid[2].usUsagePage = 0x01;
	Rid[2].usUsage = 0x05;
	Rid[2].dwFlags = RIDEV_REMOVE;
	Rid[2].hwndTarget = 0;

	RegisterRawInputDevices(Rid, 3, sizeof(RAWINPUTDEVICE));
}
bool InputInterface::handleMessage(HRAWINPUT* pRawData){
	if (pRawData == NULL){
		return false;
	}
	UINT dwSize;
	GetRawInputData(*pRawData, RID_INPUT, NULL, &dwSize, sizeof(RAWINPUTHEADER));

	BYTE lpb[4096];
	int readSize = GetRawInputData((HRAWINPUT)*pRawData, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER));
	if (readSize != dwSize){
		return 0;
	}

	RAWINPUT* pRaw = (RAWINPUT*)lpb;

	if (pRaw->header.dwType == RIM_TYPEKEYBOARD){
		m_Mutex.Open();
		processKeyboard(m_keyboardState, &pRaw->data.keyboard);
		if (m_keyboardState.Key[pRaw->data.keyboard.VKey] != RI_KEY_BREAK){
			m_lastKeyboardKeyCode = pRaw->data.keyboard.VKey;
			m_LastKeyboardKeyNew = true;
			printf("last keyboard key pressed : 0x%02x\n", m_lastKeyboardKeyCode);
		}
		m_Mutex.Close();
	}
	else if (pRaw->header.dwType == RIM_TYPEMOUSE){
		m_Mutex.Open();
		processMouse(m_mouseState, &pRaw->data.mouse);
		m_Mutex.Close();
	}
	return true;
}

//
void InputInterface::deviceAdded(HANDLE hDevice){
	appDebugf(L"New Device attached\n");
	RID_DEVICE_INFO device_info = { 0 };
	unsigned int DeviceVendorID = 0;
	unsigned int DeviceProductID = 0;
	unsigned int SizeInBytes = sizeof(RID_DEVICE_INFO);

	GetRawInputDeviceInfo(hDevice, RIDI_DEVICEINFO, &device_info, &SizeInBytes);
	DeviceVendorID = device_info.hid.dwVendorId;
	DeviceProductID = device_info.hid.dwProductId;

	if (DeviceVendorID == DUALSHOCK4_DEVICE_VENDOR_ID &&
		DeviceProductID == DUALSHOCK4_DEVICE_PRODUCT_ID)
	{

		int controller_index = -1;
		for (unsigned int i = 0; i < 4; i++){
			m_Mutex.Open();
			if (!m_DualShock4Info[i].Connected){
				wchar_t DeviceName[256] = L"";
				unsigned int DeviceNameLenght = 256;

				// get device name for creating a connection
				GetRawInputDeviceInfo(hDevice, RIDI_DEVICENAME, &DeviceName, &DeviceNameLenght);
				HANDLE HIDHandle = CreateFile(DeviceName, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, NULL, NULL);

				m_DualShock4Info[i].hDevice = hDevice;
				m_DualShock4Info[i].Connected = true;
				m_DualShock4Info[i].hConnection = HIDHandle;
				if (i == 0){// first connected controller light bar is blue
					setDualShock4LightBarColor(i, 0, 0, 1);
				}
				if (i == 1){// second connected controller light bar is green
					setDualShock4LightBarColor(i, 0, 1, 0);
				}
				if (i == 2){// third connected controller light bar is red
					setDualShock4LightBarColor(i, 1, 0, 0);
				}
				if (i == 3){// fourth connected controller light bar is yellow
					setDualShock4LightBarColor(i, 1, 1, 0);
				}
				ZeroMemory(&m_DualShock4Info[i].state, sizeof(DualShock4State));
				controller_index = i;
				break;
			}
		}
		m_Mutex.Close();
	}

}
void InputInterface::deviceRemoved(HANDLE hDevice){
	appDebugf(L"New Device attached\n");
	
	m_Mutex.Open();
	for (unsigned int i = 0; i < 4; i++){
		if (m_DualShock4Info[i].hDevice == hDevice){
			// close the connection with the controller
			CloseHandle(m_DualShock4Info[i].hConnection);
			// reset the state of the dual shock 4 info
			m_DualShock4Info[i].hDevice = 0;
			m_DualShock4Info[i].Connected = false;
			m_DualShock4Info[i].hConnection = NULL;
			
		}
	}
	m_Mutex.Close();
}

// Mouse related code
void InputInterface::getKeyboardState(KeyboardState& state){
	m_Mutex.Open();
	state = m_keyboardState;
	m_Mutex.Close();
}
void InputInterface::getMouseState(MouseState& state){
	m_Mutex.Open();
	state = m_mouseState;
	m_Mutex.Close();
}

// Keyboard related code
bool InputInterface::isKeyboardKeyDown(unsigned short& KeyCode){
	m_Mutex.Open();
	bool result = (m_keyboardState.Key[KeyCode] == KeyState::kDown);
	m_Mutex.Close();
	return result;
}
bool InputInterface::getKeyboardLastPressedKey(unsigned short& KeyCode){
	m_Mutex.Open();
	bool result = m_LastKeyboardKeyNew;
	m_LastKeyboardKeyNew = false;
	KeyCode = m_lastKeyboardKeyCode;
	m_Mutex.Close();
	return result;
}


//	Dualshock 4 related code
bool InputInterface::getDualShock4State(const int index, DualShock4State& state){
	m_Mutex.Open();
	if (!m_DualShock4Info[index].Connected){
		m_Mutex.Close();
		return false;
	}
	int result = false;
	unsigned char reportData[78] = { 0 };
	DWORD bytes_written = 0;
	if (!ReadFile(m_DualShock4Info[index].hConnection, &reportData, sizeof(reportData), &bytes_written, NULL)){
		state = m_DualShock4Info[index].state;
		m_Mutex.Close();
		return false;
	}
	unsigned char* pRawData = &reportData[0];
	DualShock4ControllerData* cd = (DualShock4ControllerData*)&pRawData[5];
	state.StickLeft[0] = Clamp(-128 + pRawData[1], -128, 128) * 255;
	state.StickLeft[1] = -Clamp(-128 + pRawData[2], -128, 128) * 255;
	state.StickRight[0] = Clamp(-128 + pRawData[3], -128, 128) * 255;
	state.StickRight[1] = -Clamp(-128 + pRawData[4], -128, 128) * 255;
	state.TriggerLeft = pRawData[8];
	state.TriggerRight = pRawData[9];
	state.DPad = cd->Direction;
	state.Button = cd->Buttons;

	//
	if (m_DualShock4Info[index].state.packetNum == 0 ||
		state.Button != m_DualShock4Info[index].state.Button ||
		state.DPad != m_DualShock4Info[index].state.DPad ||
		state.StickLeft[0] != m_DualShock4Info[index].state.StickLeft[0] ||
		state.StickLeft[1] != m_DualShock4Info[index].state.StickLeft[1] ||
		state.StickRight[0] != m_DualShock4Info[index].state.StickRight[0] ||
		state.StickRight[1] != m_DualShock4Info[index].state.StickRight[1] ||
		state.TriggerLeft != m_DualShock4Info[index].state.TriggerLeft ||
		state.TriggerRight != m_DualShock4Info[index].state.TriggerRight)
	{
		m_DualShock4Info[index].state.packetNum++;
		state.packetNum = m_DualShock4Info[index].state.packetNum;
		m_DualShock4Info[index].state.Button = state.Button;
		m_DualShock4Info[index].state.DPad = state.DPad;
		m_DualShock4Info[index].state.StickLeft[0] = state.StickLeft[0];
		m_DualShock4Info[index].state.StickLeft[1] = state.StickLeft[1];
		m_DualShock4Info[index].state.StickRight[0] = state.StickRight[0];
		m_DualShock4Info[index].state.StickRight[1] = state.StickRight[1];
		m_DualShock4Info[index].state.TriggerLeft = state.TriggerLeft;
		m_DualShock4Info[index].state.TriggerRight = state.TriggerRight;
	}


	state.packetNum = m_DualShock4Info[index].state.packetNum;
	m_Mutex.Close();
	return m_DualShock4Info[index].Connected;
}
bool InputInterface::setDualShock4Rumble(const int index, float LeftMotor, float RightMotor){
	m_Mutex.Open();
	if (!m_DualShock4Info[index].Connected){
		m_Mutex.Close();
		return false;
	}
	DWORD byte_written = 0;
	unsigned char reportData[32] = { 0 };
	reportData[0] = 0x05;// this is the report mode
	reportData[1] = 0xF1; // this is for enabling the rumble set to the values next
	reportData[4] = (char)RightMotor * 255;
	reportData[5] = (char)LeftMotor * 255; 
	if (!WriteFile(m_DualShock4Info[index].hConnection, &reportData, sizeof(reportData), &byte_written, NULL)){
		m_Mutex.Close();
		return false;
	}
	m_Mutex.Close();
	return true;
}
bool InputInterface::setDualShock4LightBarColor(const int index, float Red, float Green, float Blue){
	m_Mutex.Open();
	if (!m_DualShock4Info[index].Connected){
		m_Mutex.Close();
		return false;
	}
	DWORD byte_written = 0;
	unsigned char reportData[32] = { 0 };
	reportData[0] = 0x05;
	reportData[1] = 0xff & ~0xf1;
	reportData[6] = (char)Red * 255;
	reportData[7] = (char)Green * 255;
	reportData[8] = (char)Blue * 255;

	if (!WriteFile(m_DualShock4Info[index].hConnection, &reportData, sizeof(reportData), &byte_written, NULL)){
		m_Mutex.Close();
		return false;
	}
	m_Mutex.Close(); 
	return true;
}

//	XInput related code
bool InputInterface::getXInputState(const int index, XINPUT_STATE& state){
	return XInputGetState(index, &state) != ERROR_DEVICE_NOT_CONNECTED;
}
bool InputInterface::setXInputRumble(const int index, const short left_motor, const short right_motor){
	XINPUT_VIBRATION rumble = { left_motor, right_motor };
	return XInputSetState(index, &rumble) != ERROR_DEVICE_NOT_CONNECTED;
}