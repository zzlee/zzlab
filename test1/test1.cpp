#include "stdafx.h"

#include "zzlab.h"
#include "zzlab/utils.h"
#include "zzlab/net.h"

#include <iostream>
#include <queue>
#include <conio.h>

#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/ref.hpp>
#include <boost/thread.hpp>

using namespace boost;
using namespace zzlab;

ZZLAB_USE_LOG4CPLUS(test1);

class Test
{
public:
	Test() : mDiscovery(NULL)
	{

	}

	~Test()
	{
		if (mDiscovery)
			delete mDiscovery;
	}

	void init()
	{
		mDiscovery = new net::ServiceDiscovery();
		mDiscovery->init();
	}

protected:
	net::ServiceDiscovery* mDiscovery;
};

int _tmain(int argc, _TCHAR* argv[])
{
	// set log level
	log4cplus::Logger::getRoot().setLogLevel(log4cplus::TRACE_LOG_LEVEL);

	// install plugins
	utils::install();
	net::install();

	// intialize zzlab (including plugins)
	initialize();

	{
		// install ctrl-c signal handler for console mode
		asio::signal_set signals(_MainService, SIGINT, SIGTERM);
		signals.async_wait(boost::bind(utils::stopAllServices));

		Test t;
		t.init();

		// start all services
		utils::startAllServices();
	}

	// unintialize zzlab (including plugins)
	uninitialize();

	return 0;
}