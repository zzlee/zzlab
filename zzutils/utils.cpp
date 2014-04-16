// utils.cpp : 定義 DLL 應用程式的匯出函式。
//

#include "stdafx.h"

#include "zzlab.h"
#include "zzlab/utils.h"

#include <boost/asio/placeholders.hpp>
#include <boost/bind.hpp>

ZZLAB_USE_LOG4CPLUS(zzutils);

using namespace boost;

namespace zzlab {

	namespace utils {

		static int64_t sFrequency = 0;

		static bool initialize()
		{
			ZZLAB_TRACE_FUNCTION();

			QueryPerformanceFrequency((LARGE_INTEGER *)&sFrequency);

			return true;
		}

		static void uninitialize()
		{
			ZZLAB_TRACE_FUNCTION();
		}

		void install(void)
		{
			Plugin plugin = {
				L"zzutils", initialize, uninitialize
			};

			addPlugin(plugin);
		}

		const int64_t Timer::timeUnit = 10000000;
		const double Timer::fTimeUnit = 10000000;
		const AVRational Timer::rTimeUnitQ = { 1, 10000000 };

		Timer::Timer()
		{
		}

		Timer::~Timer()
		{
		}

		HiPerfTimer::HiPerfTimer()
		{
			ZZLAB_TRACE_THIS();

			reset();
		}

		HiPerfTimer::~HiPerfTimer()
		{
			ZZLAB_TRACE_THIS();
		}

		void HiPerfTimer::reset()
		{
			QueryPerformanceCounter((LARGE_INTEGER *)&mBegin);
			mTime = 0;
		}

		int64_t HiPerfTimer::getTime()
		{
			QueryPerformanceCounter((LARGE_INTEGER *)&mNow);
			return (mNow - mBegin) * timeUnit / sFrequency;
		}

		ManualTimer::ManualTimer() : now(0)
		{
			ZZLAB_TRACE_THIS();
		}

		ManualTimer::~ManualTimer()
		{
			ZZLAB_TRACE_THIS();
		}

		int64_t ManualTimer::getTime()
		{
			return now;
		}

		RealTimeManager::RealTimeManager(Timer& timeSource) :
			mTimer(_MainService), mTimeSource(timeSource)
		{

		}

		RealTimeManager::~RealTimeManager()
		{
			// clear all tasks
			while (!mTaskQueue.empty())
			{
				delete mTaskQueue.top();
				mTaskQueue.pop();
			}
		}

		void RealTimeManager::schedule(boost::system::error_code err)
		{
			if (err)
				return;

			int64_t now = mTimeSource.getTime();
			while (!mTaskQueue.empty())
			{
				Task* top = mTaskQueue.top();
				if (top->t > now)
				{
					mTimer.expires_from_now(posix_time::milliseconds(2));
					mTimer.async_wait(bind(&RealTimeManager::schedule, this, asio::placeholders::error));
					break;
				}

				// invoke callback and free it
				top->cb();
				delete top;
				mTaskQueue.pop();
			}
		}

	} // namespace utils
} // namespace zzlab