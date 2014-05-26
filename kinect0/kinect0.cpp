// kinect0.cpp : 定義應用程式的進入點。
//

#include "stdafx.h"

#include "Resource.h"

#include "zzlab.h"
#include "zzlab/av.h"
#include "zzlab/utils.h"
#include "zzlab/gfx.h"
#include "zzlab/net.h"
#include "zzlab/wx.h"
#include "zzlab/d3d9.h"
#include "zzlab/di8.h"

using namespace boost;
using namespace zzlab;

ZZLAB_USE_LOG4CPLUS(Main);

utils::Timer* _Timer = nullptr;
d3d9::IDirect3D9ExPtr _d3d9ex;
di8::IDirectInput8Ptr _di8;

extern void init();
extern void uninit();

class MyApp : public wx::App
{
public:
	virtual bool OnInit();
	virtual int OnExit();
};

bool MyApp::OnInit()
{
	// set log level
	_LogFlags = LOG_TO_WIN32DEBUG | LOG_TO_FILE;
	log4cplus::Logger::getRoot().setLogLevel(log4cplus::TRACE_LOG_LEVEL);

	// install plugins
	utils::install();
	av::install();
	wx::install();
	gfx::install();
	d3d9::install();

	if (!wx::App::OnInit())
		return false;

	ZZLAB_TRACE_THIS();

	_Timer = new utils::HiPerfTimer();
	_d3d9ex = d3d9::createEx();
	_di8 = di8::create();

	zzlab::startWorkerService();
	gfx::startRenderService();

	init();

	return true;
}

int MyApp::OnExit()
{
	uninit();

	gfx::stopRenderService();
	zzlab::stopWorkerService();
	_d3d9ex = NULL;
	_di8 = nullptr;
	delete _Timer;
	_Timer = nullptr;

	return wx::App::OnExit();
}

wxIMPLEMENT_APP(MyApp);
