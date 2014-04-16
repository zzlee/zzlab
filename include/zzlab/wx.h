#ifndef __ZZLAB_WX_H__
#define __ZZLAB_WX_H__

#ifdef ZZWX_EXPORTS
#define ZZWX_API __declspec(dllexport)
#else
#define ZZWX_API __declspec(dllimport)
#endif

#include "zzlab.h"
#include "zzlab/utils.h"

#include <wx/app.h>
#include <wx/apptrait.h>
#include <wx/evtloop.h>

#include <boost/asio/deadline_timer.hpp>
#include <boost/system/error_code.hpp>
#include <boost/locale.hpp>

namespace zzlab
{
	namespace wx
	{
		ZZWX_API void install(void);

		class ZZWX_API EventLoop : public wxGUIEventLoop
		{
		public:
			EventLoop();
			virtual ~EventLoop();

			virtual int DoRun();

		protected:
			void step(boost::system::error_code err = boost::system::error_code());
			bool ProcessEvents();
			void DoExit();

		protected:
			boost::asio::deadline_timer mTimer;
			bool mNewIteration;
		};

		class ZZWX_API AppTraits : public wxGUIAppTraits
		{
			virtual wxEventLoopBase* CreateEventLoop();
		};

		class ZZWX_API App : public wxApp
		{
		public:
			virtual wxAppTraits* CreateTraits();
			virtual bool OnInit();
			virtual int OnExit();
		};

		void ZZWX_API loadWindowSettings(XmlNode* node, wxString& title, wxPoint& position, wxSize& size, long& style);

		template<class T> T* loadWindow(XmlNode* node, wxWindow* parent = NULL, wxWindowID id = wxID_ANY)
		{
			wxString title;
			wxPoint position;
			wxSize size;
			long style;
			loadWindowSettings(node, title, position, size, style);

			return new T(parent, id, title, position, size, style);
		}
	}
}

#endif // __ZZLAB_WX_H__