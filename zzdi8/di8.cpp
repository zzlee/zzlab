#include "stdafx.h"
#include "zzlab/di8.h"

#include <boost/asio/placeholders.hpp>
#include <boost/bind.hpp>

using namespace boost;
using namespace zzlab;

ZZLAB_USE_LOG4CPLUS(di8);

namespace zzlab
{
	namespace di8
	{
		static bool initialize()
		{
			ZZLAB_TRACE_FUNCTION();

			return true;
		}

		static void uninitialize()
		{
			ZZLAB_TRACE_FUNCTION();
		}

		void install(void)
		{
			Plugin plugin = {
				L"zzdi8", initialize, uninitialize
			};

			addPlugin(plugin);
		}

		IDirectInput8Ptr create(HINSTANCE inst)
		{
			IDirectInput8 *ptr;
			HR(DirectInput8Create(inst ? inst : GetModuleHandle(NULL), DIRECTINPUT_VERSION, IID_IDirectInput8, (void **)&ptr, NULL));
			return IDirectInput8Ptr(ptr, false);
		}

		Input::Input()
		{
			ZZLAB_TRACE_THIS();

			keyboardCooperativeLevel = DISCL_FOREGROUND | DISCL_EXCLUSIVE;
			mouseCooperativeLevel = DISCL_FOREGROUND | DISCL_NONEXCLUSIVE;

			mKeyStates[0] = new boost::array<BYTE, 256>;
			std::fill(mKeyStates[0]->begin(), mKeyStates[0]->end(), 0);

			mKeyStates[1] = new boost::array<BYTE, 256>;
			std::fill(mKeyStates[1]->begin(), mKeyStates[1]->end(), 0);

			mMouseStates[0] = new DIMOUSESTATE;
			ZeroMemory(mMouseStates[0], sizeof(DIMOUSESTATE));

			mMouseStates[1] = new DIMOUSESTATE;
			ZeroMemory(mMouseStates[1], sizeof(DIMOUSESTATE));
		}

		Input::~Input()
		{
			ZZLAB_TRACE_THIS();

			if (mKeyboard)
				mKeyboard->Unacquire();

			if (mMouse)
				mMouse->Unacquire();

			delete mKeyStates[0];
			delete mKeyStates[1];
			delete mMouseStates[0];
			delete mMouseStates[1];
		}

		void Input::init()
		{
			IDirectInputDevice8 *did;

			HR(dinput->CreateDevice(GUID_SysKeyboard, &did, NULL));
			mKeyboard = IDirectInputDevice8Ptr(did, false);
			HR(mKeyboard->SetDataFormat(&c_dfDIKeyboard));
			HR(mKeyboard->SetCooperativeLevel(hwnd, keyboardCooperativeLevel));

			HR(dinput->CreateDevice(GUID_SysMouse, &did, NULL));
			mMouse = IDirectInputDevice8Ptr(did, false);
			HR(mMouse->SetDataFormat(&c_dfDIMouse));
			HR(mMouse->SetCooperativeLevel(hwnd, mouseCooperativeLevel));
		}

		void Input::update()
		{
			std::swap(mKeyStates[0], mKeyStates[1]);
			std::swap(mMouseStates[0], mMouseStates[1]);

			readKeyboard();
			readMouse();
		}

		void Input::readKeyboard()
		{
			HRESULT hr;
			hr = mKeyboard->GetDeviceState(256, (LPVOID)& (*mKeyStates[0])[0]);
			if (FAILED(hr))
			{
				if ((hr == DIERR_INPUTLOST) || (hr == DIERR_NOTACQUIRED))
				{
					if (SUCCEEDED(mKeyboard->Acquire()))
						return;
				}

				std::fill(mKeyStates[0]->begin(), mKeyStates[0]->end(), 0);
			}
		}

		void Input::readMouse()
		{
			HRESULT hr;
			hr = mMouse->GetDeviceState(sizeof(DIMOUSESTATE), (LPVOID)mMouseStates[0]);
			if (FAILED(hr))
			{
				if ((hr == DIERR_INPUTLOST) || (hr == DIERR_NOTACQUIRED))
				{
					if (SUCCEEDED(mMouse->Acquire()))
						return;
				}

				ZeroMemory(mMouseStates[0], sizeof(DIMOUSESTATE));
			}
		}
	}
}
