#ifndef __ZZLAB_H__
#define __ZZLAB_H__

#ifdef ZZMAIN_EXPORTS
#define ZZLAB_API __declspec(dllexport)
#else
#define ZZLAB_API __declspec(dllimport)
#endif

#include <boost/filesystem.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/deadline_timer.hpp>
#include <boost/function.hpp>

#include <log4cplus/logger.h>
#include <log4cplus/loggingmacros.h>

#include <rapidxml/rapidxml.hpp>
#include <rapidxml/rapidxml_utils.hpp>

// declare log4cplus logger
#define ZZLAB_USE_LOG4CPLUS(n) \
	static log4cplus::Logger __log4cplus__ = log4cplus::Logger::getInstance(_T(#n))

// log4cplus logging macros
#define ZZLAB_TRACE(x) LOG4CPLUS_TRACE(__log4cplus__, x)
#define ZZLAB_DEBUG(x) LOG4CPLUS_DEBUG(__log4cplus__, x)
#define ZZLAB_INFO(x) LOG4CPLUS_INFO(__log4cplus__, x)
#define ZZLAB_WARN(x) LOG4CPLUS_WARN(__log4cplus__, x)
#define ZZLAB_ERROR(x) LOG4CPLUS_ERROR(__log4cplus__, x)
#define ZZLAB_FATAL(x) LOG4CPLUS_FATAL(__log4cplus__, x)

// logging helpers
#define ZZLAB_STEP() ZZLAB_TRACE(__FILE__ << L'@' << __LINE__)
#define ZZLAB_STEP1(x) ZZLAB_TRACE(__FILE__ << L'@' << __LINE__ << L" | " << x)
#define ZZLAB_STEP_THIS() ZZLAB_TRACE(L"| " << std::hex << this << L" | " << __FILE__ << L'@' << std::dec << __LINE__)
#define ZZLAB_STEP_THIS1() ZZLAB_TRACE(L"| " << std::hex << this << L" | " << __FILE__ << L'@' << std::dec << __LINE__ << L" | " << logEvent)
#define ZZLAB_TRACE_EXCEPTION(ex) ZZLAB_ERROR(boost::diagnostic_information(ex))
#define ZZLAB_TRACE_FUNCTION() ZZLAB_TRACE(__FUNCTION__)
#define ZZLAB_TRACE_FUNCTION1(x) ZZLAB_TRACE(__FUNCTION__ << ':' << x)
#define ZZLAB_TRACE_THIS() ZZLAB_TRACE(L"| " << std::hex << this << L" | " << __FUNCTION__)
#define ZZLAB_TRACE_THIS1(x) ZZLAB_TRACE(L"| " << std::hex << this << L" | " << __FUNCTION__ << L" | " << std::dec << x)
#define ZZLAB_TRACE_VALUE(x) ZZLAB_TRACE(#x << L" = " << (x))

// error code handler
#define HR(x) zzlab::_HR((x), __FILE__, __LINE__)
#define WIN(x) zzlab::_WIN((x), __FILE__, __LINE__)

namespace zzlab
{
	enum LogFlags
	{
		LOG_TO_FILE = 0x0001,
		LOG_TO_WIN32DEBUG = 0x0002,
		LOG_TO_CONSOLE = 0x0004,
	};

	// log system flags
	extern ZZLAB_API int _LogFlags; // combine of LogFlags

	// global path
	extern ZZLAB_API boost::filesystem::wpath _RootPath;
	extern ZZLAB_API boost::filesystem::wpath _DataPath;
	extern ZZLAB_API boost::filesystem::wpath _AssetsPath;

	// boost asio service objects
	extern ZZLAB_API boost::asio::io_service _MainService;
	extern ZZLAB_API boost::asio::io_service _WorkerService;

	// settings
	typedef rapidxml::xml_document<wchar_t> XmlDocument;
	typedef rapidxml::xml_node<wchar_t> XmlNode;
	typedef rapidxml::xml_attribute<wchar_t> XmlAttribute;
	typedef rapidxml::file<wchar_t> XmlFile;

	extern ZZLAB_API XmlDocument _Settings;

	struct Plugin
	{
		std::wstring name;
		boost::function<bool()> init;
		boost::function<void()> uninit;
	};

	// zzlab service management
	ZZLAB_API void addPlugin(const Plugin& p);
	ZZLAB_API void initialize();
	ZZLAB_API void uninitialize();
	ZZLAB_API void startWorkerService(size_t workerThreads = 0);
	ZZLAB_API void stopWorkerService();
	ZZLAB_API void startMainService();
	ZZLAB_API void stopMainService();
	ZZLAB_API bool isMainServiceRunning();
	ZZLAB_API size_t pollMainService();
	ZZLAB_API size_t pollOneMainService();

	ZZLAB_API void _HR(HRESULT hr, const char *filename, int line);
	ZZLAB_API void _WIN(BOOL t, const char *filename, int line);

	class ZZLAB_API IdleForever
	{
	public:
		IdleForever(boost::asio::io_service& io_service);
		~IdleForever();

		void init();

	protected:
		boost::asio::deadline_timer mTimer;
		void main(boost::system::error_code err = boost::system::error_code());
	};
}

#endif __ZZLAB_H__