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

#include "Test1.h"

using namespace boost;
using namespace zzlab;

ZZLAB_USE_LOG4CPLUS(test0);

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

		Test1 t;
		t.init();

		// start all services
		utils::startAllServices();
	}

	// unintialize zzlab (including plugins)
	uninitialize();

	return 0;
}