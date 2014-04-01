#include "stdafx.h"

#include "zzlab.h"
#include "zzlab/av.h"
#include "zzlab/utils.h"

#include <iostream>
#include <queue>
#include <conio.h>

#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/ref.hpp>
#include <boost/thread.hpp>
#include <portaudio.h>

#include <comdef.h>
#include <dshow.h>

#import "qedit.dll"

using namespace boost;
using namespace zzlab;

_COM_SMARTPTR_TYPEDEF(IUnknown, __uuidof(IUnknown));

_COM_SMARTPTR_TYPEDEF(IMoniker, __uuidof(IMoniker));
_COM_SMARTPTR_TYPEDEF(IEnumMoniker, __uuidof(IEnumMoniker));
_COM_SMARTPTR_TYPEDEF(IPropertyBag, __uuidof(IPropertyBag));
_COM_SMARTPTR_TYPEDEF(IRunningObjectTable, __uuidof(IRunningObjectTable));

_COM_SMARTPTR_TYPEDEF(ICreateDevEnum, __uuidof(ICreateDevEnum));
_COM_SMARTPTR_TYPEDEF(IAMStreamConfig, __uuidof(IAMStreamConfig));
_COM_SMARTPTR_TYPEDEF(ICaptureGraphBuilder2, __uuidof(ICaptureGraphBuilder2));
_COM_SMARTPTR_TYPEDEF(IBaseFilter, __uuidof(IBaseFilter));

ZZLAB_USE_LOG4CPLUS(test2);

int _tmain(int argc, _TCHAR* argv[])
{
	// set logger
	log4cplus::Logger::getRoot().setLogLevel(log4cplus::TRACE_LOG_LEVEL);

	// install plugins
	utils::install();
	av::install();

	// intialize zzlab (including plugins)
	initialize();

	{
		// install ctrl-c signal handler for console mode
		asio::signal_set signals(_MainService, SIGINT, SIGTERM);
		signals.async_wait(boost::bind(utils::stopAllServices));

		// start all services
		utils::startAllServices();
	}

	// unintialize zzlab (including plugins)
	uninitialize();

	return 0;
}