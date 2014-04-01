#include "stdafx.h"
#include "zzlab.h"

#include <log4cplus/logger.h>
#include <log4cplus/fileappender.h>
#include <log4cplus/win32debugappender.h>
#include <log4cplus/layout.h>
#include <log4cplus/ndc.h>
#include <log4cplus/helpers/loglog.h>
#include <log4cplus/loggingmacros.h>
#include <log4cplus/configurator.h>

#include <boost/thread.hpp>
#include <boost/asio.hpp>

ZZLAB_USE_LOG4CPP(zzlab);

using namespace boost;

namespace zzlab
{
	filesystem::wpath _RootPath;
	filesystem::wpath _DataPath;
	filesystem::wpath _AssetsPath;
	asio::io_service _MainService;
	asio::io_service _WorkerService;

	static void initLogSystem()
	{
		log4cplus::initialize();

		// log to daily rolling log file
		{
			// make sure path "Data/logs" exists
			filesystem::wpath p = _DataPath / L"logs";
			system::error_code ec;
			filesystem::create_directories(p, ec); // ignore this error			

			log4cplus::SharedAppenderPtr appender(
				new log4cplus::DailyRollingFileAppender((p / L"zzlab.log").wstring()));
			appender->setLayout(std::auto_ptr<log4cplus::Layout>(new log4cplus::TTCCLayout()));
			log4cplus::Logger::getRoot().addAppender(appender);
		}

		// log to DebugView
		{
			log4cplus::SharedAppenderPtr appender(new log4cplus::Win32DebugAppender());
			appender->setLayout(std::auto_ptr<log4cplus::Layout>(new log4cplus::TTCCLayout()));
			log4cplus::Logger::getRoot().addAppender(appender);
		}
	}

	void initialize()
	{
		_RootPath = filesystem::absolute(filesystem::current_path());
		_DataPath = _RootPath / L"Data";
		_AssetsPath = _RootPath / L"Assets";

		initLogSystem();

		ZZLAB_LOG_FUNCTION();

		// TODO: other initializations

		ZZLAB_INFO("Initialize succeedded");
	}

	void uninitialize()
	{
		ZZLAB_LOG_FUNCTION();

		// TODO: other uninitializtions
	}

	static bool workerServing = false;
	static thread_group threadPool;
	static bool mainServing = false;

	static void workerInit(size_t i, barrier &_barrier, asio::io_service &io_service)
	{
		// set worker thread affinity to cores other than core 1
		SetThreadAffinityMask(GetCurrentThread(), 0xFFFFFFFF ^ 1);

		_barrier.wait();

		ZZLAB_INFO("Worker thread started");
		io_service.run();
		ZZLAB_INFO("Worker thread stopped");
	}

	class IdleForever
	{
	public:
		IdleForever();
		~IdleForever();

		void init();

	protected:
		asio::coroutine _coro_main;
		asio::deadline_timer mTimer;

		void main(boost::system::error_code err = boost::system::error_code());
	};
	static IdleForever* workerIdle = NULL;

	IdleForever::IdleForever() :
		mTimer(_WorkerService)
	{
		ZZLAB_LOG_THIS();
	}

	IdleForever::~IdleForever()
	{
		ZZLAB_LOG_THIS();
	}

	void IdleForever::init()
	{
		_WorkerService.post(bind(&IdleForever::main, this, system::error_code()));
	}

#include <boost/asio/yield.hpp>
	void IdleForever::main(system::error_code err)
	{
		reenter(_coro_main) for (;;)
		{
			mTimer.expires_from_now(posix_time::seconds(86400)); // 1 day !!!
			yield mTimer.async_wait(bind(&IdleForever::main, this, asio::placeholders::error));

			if (err)
				yield break;
		}
	}
#include <boost/asio/unyield.hpp>

	void startWorkerService(size_t workerThreads)
	{
		ZZLAB_LOG_FUNCTION1(L": workerThread=" << workerThreads);

		if (!workerServing)
		{
			workerIdle = new IdleForever();
			workerIdle->init();

			if (workerThreads == 0) {
				workerThreads = thread::hardware_concurrency();
			}

			barrier _barrier(workerThreads + 1);
			ZZLAB_INFO("Create " << workerThreads << " threads for worker service.");
			for (size_t i = 0; i < workerThreads; ++i)
			{
				threadPool.create_thread(bind(workerInit, i, ref(_barrier), ref(_WorkerService)));
			}
			_barrier.wait();
			workerServing = true;

			ZZLAB_INFO("Worker service started.");
		}
	}

	void stopWorkerService()
	{
		ZZLAB_LOG_FUNCTION();

		if (workerServing)
		{
			_WorkerService.stop();
			threadPool.join_all();
			_WorkerService.reset();
			delete workerIdle;
			workerIdle = NULL;
			workerServing = false;

			ZZLAB_INFO("Worker service stopped.");
		}
	}

	void startMainService()
	{
		ZZLAB_LOG_FUNCTION();

		if (!mainServing)
		{
			ZZLAB_INFO("Main service is running...");

			mainServing = true;
			_MainService.run();
		}
	}

	void stopMainService()
	{
		ZZLAB_LOG_FUNCTION();

		if (mainServing)
		{
			_MainService.stop();
			_MainService.reset();
			mainServing = false;

			ZZLAB_INFO("Main service stopped.");
		}
	}

	bool isMainServiceRunning()
	{
		return !_MainService.stopped();
	}

	size_t pollMainService()
	{
		return _MainService.poll();
	}

	size_t pollOneMainService()
	{
		return _MainService.poll_one();
	}
}