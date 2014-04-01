// wx.cpp : 定義 DLL 應用程式的匯出函式。
//

#include "stdafx.h"

#include "zzlab.h"
#include "zzlab/utils.h"
#include "zzlab/wx.h"

#include <boost/asio/placeholders.hpp>
#include <boost/bind.hpp>

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

			utils::startAllServices();

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
				try
				{
					if (!wxTheApp || !wxTheApp->OnExceptionInMainLoop())
					{
						utils::stopAllServices();
						OnExit();
					}
					//else: continue running the event loop
				}
				catch (...)
				{
					// OnException() throwed, possibly rethrowing the same
					// exception again: very good, but we still need OnExit() to
					// be called
					utils::stopAllServices();
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

			utils::stopAllServices();
		}

	} // namespace wx

} // namespace zzlab