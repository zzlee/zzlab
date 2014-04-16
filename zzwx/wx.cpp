// wx.cpp : 定義 DLL 應用程式的匯出函式。
//

#include "stdafx.h"

#include "zzlab.h"
#include "zzlab/utils.h"
#include "zzlab/wx.h"

#include <boost/asio/placeholders.hpp>
#include <boost/bind.hpp>
#include <boost/algorithm/string.hpp>

ZZLAB_USE_LOG4CPLUS(zzwx);

using namespace boost;

namespace zzlab {

	namespace wx {

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
				L"zzwx", initialize, uninitialize
			};

			addPlugin(plugin);
		}

		wxAppTraits* App::CreateTraits()
		{
			return new AppTraits();
		}
		
		bool App::OnInit()
		{
			// intialize zzlab (including plugins)
			zzlab::initialize();

			ZZLAB_TRACE_THIS();

			return true;
		}

		int App::OnExit()
		{
			ZZLAB_TRACE_THIS();

			zzlab::stopMainService();
			zzlab::uninitialize();

			return wxApp::OnExit();
		}

		wxEventLoopBase* AppTraits::CreateEventLoop()
		{
			return new EventLoop();
		}

		EventLoop::EventLoop() :
			mTimer(_MainService), 
			mNewIteration(true)
		{
		}

		EventLoop::~EventLoop()
		{
		}

		int EventLoop::DoRun()
		{
			_MainService.post(boost::bind(&EventLoop::step, this, boost::system::error_code()));

			zzlab::startMainService();

			return m_exitcode;
		}

		void EventLoop::step(boost::system::error_code err)
		{
			if (err)
				return;

#if wxUSE_EXCEPTIONS
			try
			{
#endif // wxUSE_EXCEPTIONS
				if (mNewIteration)
				{
					mNewIteration = false;

					// give them the possibility to do whatever they want
					OnNextIteration();
				}

				bool bPending;
				do
				{
					bPending = Pending();
				} while (!m_shouldExit && !bPending && ProcessIdle());

				if (m_shouldExit)
				{
					DoExit();
					return;
				}

				if (bPending)
				{
					// a message came or no more idle processing to do, dispatch
					// all the pending events and call Dispatch() to wait for the
					// next message
					if (!ProcessEvents())
					{
						// we got WM_QUIT
						DoExit();
						return;
					}

					mNewIteration = true;
				}

				mTimer.expires_from_now(posix_time::milliseconds(8));
				mTimer.async_wait(bind(&EventLoop::step, this, asio::placeholders::error));
#if wxUSE_EXCEPTIONS
			}
			catch (...)
			{
				ZZLAB_ERROR("Exception is thrown.");

				try
				{
					if (!wxTheApp || !wxTheApp->OnExceptionInMainLoop())
					{
						OnExit();
					}
					//else: continue running the event loop
				}
				catch (...)
				{
					// OnException() throwed, possibly rethrowing the same
					// exception again: very good, but we still need OnExit() to
					// be called
					OnExit();
					throw;
				}
			}
#endif // wxUSE_EXCEPTIONS
		}

		bool EventLoop::ProcessEvents()
		{
			// process pending wx events first as they correspond to low-level events
			// which happened before, i.e. typically pending events were queued by a
			// previous call to Dispatch() and if we didn't process them now the next
			// call to it might enqueue them again (as happens with e.g. socket events
			// which would be generated as long as there is input available on socket
			// and this input is only removed from it when pending event handlers are
			// executed)
			if (wxTheApp)
			{
				wxTheApp->ProcessPendingEvents();

				// One of the pending event handlers could have decided to exit the
				// loop so check for the flag before trying to dispatch more events
				// (which could block indefinitely if no more are coming).
				if (m_shouldExit)
					return false;
			}

			return Dispatch();
		}

		void EventLoop::DoExit()
		{
			// Process the remaining queued messages, both at the level of the
			// underlying toolkit level (Pending/Dispatch()) and wx level
			// (Has/ProcessPendingEvents()).
			//
			// We do run the risk of never exiting this loop if pending event
			// handlers endlessly generate new events but they shouldn't do
			// this in a well-behaved program and we shouldn't just discard the
			// events we already have, they might be important.
			for (;;)
			{
				bool hasMoreEvents = false;
				if (wxTheApp && wxTheApp->HasPendingEvents())
				{
					wxTheApp->ProcessPendingEvents();
					hasMoreEvents = true;
				}

				if (Pending())
				{
					Dispatch();
					hasMoreEvents = true;
				}

				if (!hasMoreEvents)
					break;
			}
		}

		void loadWindowSettings(XmlNode* node, wxString& title, wxPoint& position, wxSize& size, long& style)
		{
			ZZLAB_INFO("Loading " << node->name() << "...");

			XmlAttribute *attr = node->first_attribute(L"title");
			title = attr ? attr->value() : L"wxWindow";

			attr = node->first_attribute(L"position");
			if (attr)
			{
				std::wstring val = attr->value();
				std::vector<std::wstring> tokens;
				split(tokens, val, is_any_of(L","));

				if (tokens.size() != 2)
					position = wxDefaultPosition;
				else
				{
					position.x = _wtoi(tokens[0].c_str());
					position.y = _wtoi(tokens[1].c_str());
				}
			}
			else
				position = wxDefaultPosition;

			attr = node->first_attribute(L"size");
			if (attr)
			{
				std::wstring val = attr->value();
				std::vector<std::wstring> tokens;
				split(tokens, val, is_any_of(L","));

				if (tokens.size() != 2)
					size = wxDefaultSize;
				else
				{
					size.x = _wtoi(tokens[0].c_str());
					size.y = _wtoi(tokens[1].c_str());
				}
			}
			else
				size = wxDefaultSize;

			style = 0;
			attr = node->first_attribute(L"style");
			if (attr)
			{
				std::wstring val = attr->value();

				typedef split_iterator<std::wstring::iterator> wstring_split_iterator;
				for (wstring_split_iterator It = make_split_iterator(val, first_finder(L"|", is_iequal()));
					It != wstring_split_iterator(); ++It)
				{
					std::wstring s = copy_range<std::wstring>(*It);
					trim(s);
#define _MATCH(x) \
	if (_wcsicmp(s.c_str(), _T(# x)) == 0) \
	style |= x; \
			else

					_MATCH(wxBORDER_DEFAULT)
						_MATCH(wxBORDER_SIMPLE)
						_MATCH(wxBORDER_SUNKEN)
						_MATCH(wxBORDER_RAISED)
						_MATCH(wxBORDER_STATIC)
						_MATCH(wxBORDER_THEME)
						_MATCH(wxBORDER_NONE)
						_MATCH(wxBORDER_DOUBLE)
						_MATCH(wxTRANSPARENT_WINDOW)
						_MATCH(wxCLIP_CHILDREN)
						_MATCH(wxCAPTION)
						_MATCH(wxDEFAULT_DIALOG_STYLE)
						_MATCH(wxRESIZE_BORDER)
						_MATCH(wxSTAY_ON_TOP)
						_MATCH(wxDEFAULT_FRAME_STYLE)
						_MATCH(wxICONIZE)
						_MATCH(wxMINIMIZE)
						_MATCH(wxMINIMIZE_BOX)
						_MATCH(wxMAXIMIZE)
						_MATCH(wxMAXIMIZE_BOX)
						_MATCH(wxCLOSE_BOX)
						_MATCH(wxSYSTEM_MENU)
						_MATCH(wxFRAME_TOOL_WINDOW)
						_MATCH(wxFRAME_NO_TASKBAR)
						_MATCH(wxFRAME_FLOAT_ON_PARENT)
						_MATCH(wxFRAME_SHAPED)
						NULL;

#undef _MATCH
				}
			}
		}

	} // namespace wx

} // namespace zzlab