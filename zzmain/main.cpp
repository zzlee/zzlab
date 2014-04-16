#include "stdafx.h"
#include "zzlab.h"

#include <vector>

#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <boost/function.hpp>

#include <log4cplus/logger.h>
#include <log4cplus/fileappender.h>
#include <log4cplus/win32debugappender.h>
#include <log4cplus/consoleappender.h>
#include <log4cplus/layout.h>
#include <log4cplus/ndc.h>
#include <log4cplus/helpers/loglog.h>
#include <log4cplus/loggingmacros.h>
#include <log4cplus/configurator.h>

#include <comdef.h>

ZZLAB_USE_LOG4CPLUS(zzlab);

using namespace boost;

namespace zzlab
{
	ZZLAB_API int _LogFlags = LOG_TO_CONSOLE;

	ZZLAB_API filesystem::wpath _RootPath;
	ZZLAB_API filesystem::wpath _DataPath;
	ZZLAB_API filesystem::wpath _AssetsPath;
	ZZLAB_API asio::io_service _MainService;
	ZZLAB_API asio::io_service _WorkerService;

	ZZLAB_API XmlDocument _Settings;
	static XmlFile* settingsFile = NULL;

	static void initLogSystem()
	{
		log4cplus::initialize();

		// log to daily rolling log file
		if (_LogFlags & LOG_TO_FILE)
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

		// log to Win32 debug
		if (_LogFlags & LOG_TO_WIN32DEBUG)
		{
			log4cplus::SharedAppenderPtr appender(new log4cplus::Win32DebugAppender());
			appender->setLayout(std::auto_ptr<log4cplus::Layout>(new log4cplus::TTCCLayout()));
			log4cplus::Logger::getRoot().addAppender(appender);
		}

		// log to console
		if (_LogFlags & LOG_TO_CONSOLE)
		{
			log4cplus::SharedAppenderPtr appender(new log4cplus::ConsoleAppender());
			appender->setLayout(std::auto_ptr<log4cplus::Layout>(new log4cplus::TTCCLayout()));
			log4cplus::Logger::getRoot().addAppender(appender);
		}
	}

	namespace detail
	{
		struct Plugin
		{
			std::wstring name;
			boost::function<bool()> init;
			boost::function<void()> uninit;
			bool initialized;
		};
	}

	static std::vector<detail::Plugin> plugins;

	void addPlugin(const Plugin& p)
	{
		detail::Plugin p_ = { p.name, p.init, p.uninit, false };
		plugins.push_back(p_);
	}

	static bool initialized = false;
	static bool mainServiceRunning = false;
	static bool workerServiceRunning = false;

	void initialize()
	{
		if (initialized) return;

		_RootPath = filesystem::absolute(filesystem::current_path());
		_DataPath = _RootPath / L"Data";
		_AssetsPath = _RootPath / L"Assets";

		initLogSystem();

		ZZLAB_TRACE_FUNCTION();

		for (std::vector<detail::Plugin>::iterator i = plugins.begin(); i != plugins.end(); ++i)
		{
			detail::Plugin& p = *i;

			ZZLAB_INFO("Initialize plugin: " << p.name);

			if (p.init())
				p.initialized = true;
			else
				ZZLAB_ERROR("Failed to initialize plugin: " << p.name);
		}

		filesystem::wpath settingPath = (_DataPath / L"settings.xml");
		ZZLAB_INFO("Loading " << settingPath.wstring() << " ...");
		settingsFile = new XmlFile(settingPath.string().c_str());
		_Settings.parse<0>(settingsFile->data());

		initialized = true;

		ZZLAB_INFO("Initialize succeedded");
	}

	void uninitialize()
	{
		if (!initialized) return;

		ZZLAB_TRACE_FUNCTION();

		_MainService.reset();
		_WorkerService.reset();

		for (std::vector<detail::Plugin>::iterator i = plugins.begin(); i != plugins.end(); ++i)
		{
			detail::Plugin& p = *i;

			ZZLAB_INFO("Uninitialize plugin: " << p.name);
			if (p.initialized) {
				p.uninit();
				p.initialized = false;
			}
		}

		initialized = false;

		ZZLAB_INFO("Uninitialize succeedded");
	}

	static thread_group threadPool;

	static void workerInit(size_t i)
	{
		ZZLAB_INFO("Worker thread started.");
		_WorkerService.run();
		ZZLAB_INFO("Worker thread stopped.");
	}

	static IdleForever* workerIdle = NULL;

	IdleForever::IdleForever(boost::asio::io_service& io_service) :
		mTimer(io_service)
	{
		ZZLAB_TRACE_THIS();
	}

	IdleForever::~IdleForever()
	{
		ZZLAB_TRACE_THIS();
	}

	void IdleForever::init()
	{
		main();
	}

	void IdleForever::main(system::error_code err)
	{
		if (err)
			return;

		mTimer.expires_from_now(posix_time::seconds(86400)); // 1 day !!!
		mTimer.async_wait(bind(&IdleForever::main, this, asio::placeholders::error));
	}

	void startWorkerService(size_t workerThreads)
	{
		ZZLAB_TRACE_FUNCTION();

		if (workerServiceRunning)
		{
			ZZLAB_WARN("Worker service is RUNNING.");
		}
		else
		{
			if (workerThreads == 0) 
				workerThreads = thread::hardware_concurrency();

			workerIdle = new IdleForever(_WorkerService);
			workerIdle->init();

			ZZLAB_INFO("Create " << workerThreads << " threads for worker service.");
			for (size_t i = 0; i < workerThreads; ++i)
			{
				threadPool.create_thread(bind(workerInit, i));
			}
			workerServiceRunning = true;

			ZZLAB_INFO("Worker service started.");
		}
	}

	void stopWorkerService()
	{
		ZZLAB_TRACE_FUNCTION();

		if (workerServiceRunning)
		{
			_WorkerService.stop();
			threadPool.join_all();
			workerServiceRunning = false;

			delete workerIdle;
			workerIdle = NULL;

			ZZLAB_INFO("Worker service stopped.");
		}
		else
		{
			ZZLAB_WARN("Worker service is NOT running.");
		}
	}

	void startMainService()
	{
		ZZLAB_TRACE_FUNCTION();

		if (mainServiceRunning)
		{
			ZZLAB_WARN("Main service is RUNNING.");
		}
		else
		{
			ZZLAB_INFO("Main service is running...");

			mainServiceRunning = true;
			_MainService.run();
		}
	}

	void stopMainService()
	{
		ZZLAB_TRACE_FUNCTION();

		if (mainServiceRunning)
		{
			_MainService.stop();
			mainServiceRunning = false;

			ZZLAB_INFO("Main service stopped.");
		}
		else
		{
			ZZLAB_WARN("Main service is NOT running.");
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

	void _HR(HRESULT x, const char *filename, int line)
	{
		if (FAILED(x))
		{
			ZZLAB_ERROR(filename << '@' << line << ", hr=0x" << std::hex << x);

			_com_raise_error(x);
		}
	}

	void _WIN(BOOL x, const char *filename, int line)
	{
		if (!x)
		{
			_HR(HRESULT_FROM_WIN32(GetLastError()), filename, line);
		}
	}

}