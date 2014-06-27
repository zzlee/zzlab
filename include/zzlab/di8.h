#ifndef __ZZLAB_DI8_H__
#define __ZZLAB_DI8_H__

#ifdef ZZDI8_EXPORTS
#define ZZDI8_API __declspec(dllexport)
#else
#define ZZDI8_API __declspec(dllimport)
#endif

#include "zzlab.h"
#include "zzlab/utils.h"

#include <boost/array.hpp>
#include <boost/system/error_code.hpp>

#include <dinput.h>
#include <comdef.h>

namespace zzlab
{
	namespace di8
	{
		ZZDI8_API void install(void);

		_COM_SMARTPTR_TYPEDEF(IDirectInput8, IID_IDirectInput8);
		_COM_SMARTPTR_TYPEDEF(IDirectInputDevice8, IID_IDirectInputDevice8);

		ZZDI8_API IDirectInput8Ptr create(HINSTANCE inst = NULL);

		class ZZDI8_API Input
		{
		public:
			IDirectInput8Ptr dinput;
			HWND hwnd;
			DWORD keyboardCooperativeLevel;
			DWORD mouseCooperativeLevel;

			Input();
			~Input();

			const boost::array<BYTE, 256> &getKeyState() const
			{
				return *mKeyStates[0];
			}

			const DIMOUSESTATE &getMouseState() const
			{
				return *mMouseStates[0];
			}

			bool getKey(size_t code) const
			{
				return (getKeyState()[code] & 0x80) != 0;
			}

			bool getMouseButton(size_t code) const
			{
				return (getMouseState().rgbButtons[code] & 0x80) != 0;
			}

			bool isKeyDown(size_t code) const
			{
				return (getKeyState()[code] != mKeyStates[1]->at(code)) && getKey(code);
			}

			bool isKeyUp(size_t code) const
			{
				return (getKeyState()[code] != mKeyStates[1]->at(code)) && !getKey(code);
			}

			bool isMouseButtonDown(size_t code) const
			{
				return (getMouseState().rgbButtons[code] != mMouseStates[1]->rgbButtons[code]) && getMouseButton(code);
			}

			bool isMouseButtonUp(size_t code) const
			{
				return (getMouseState().rgbButtons[code] != mMouseStates[1]->rgbButtons[code]) && !getMouseButton(code);
			}

			void init();
			void update();

		protected:
			IDirectInputDevice8Ptr mKeyboard;
			IDirectInputDevice8Ptr mMouse;

			boost::array<BYTE, 256> *mKeyStates[2];
			DIMOUSESTATE *mMouseStates[2];

			void readKeyboard();
			void readMouse();
		};
	}
}

#endif // __ZZLAB_DI8_H__
