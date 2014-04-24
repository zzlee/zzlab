// wxTest3.cpp : 定義應用程式的進入點。
//

#include "stdafx.h"
#include "wxTest3.h"

#include <comdef.h>
#include <algorithm>
#include <boost/format.hpp>
#include <boost/asio/deadline_timer.hpp>
#include <boost/algorithm/string.hpp>

using namespace boost;
using namespace zzlab;

ZZLAB_USE_LOG4CPLUS(wxTest3);

static utils::HiPerfTimer sTimer;
static d3d9::IDirect3D9ExPtr d3d9ex;

namespace zzlab
{
	namespace d3d9
	{


	}
}

class MyApp : public wx::App
{
public:
	virtual bool OnInit();
	virtual int OnExit();
};

enum
{
	ID_FULLSCREEN = 0x0010,
	ID_STAY_ON_TOP,
};

class MyRenderWindow : public wxTopLevelWindow
{
public:
	d3d9::RenderDevice renderDevice;

	MyRenderWindow(wxWindow *parent, wxWindowID id, const wxString &title, const wxPoint &pos = wxDefaultPosition, const wxSize &size = wxDefaultSize, long style = wxDEFAULT_FRAME_STYLE);
	~MyRenderWindow();

	void init();

	bool Destroy();

protected:
	utils::Delegate0 mFrameBeginDelegate;
	utils::Delegate0 mDrawDelegate;

	utils::ManualTimer mTimer;

	d3d9::EffectResource* mEffect;
	d3d9::MeshResource* mMesh;
	d3d9::TextureResource* mTexture;
	D3DXCOLOR mDiffuse;

	gfx::Camera camera;
	Eigen::Affine3f t2;
	Eigen::Affine3f t0;

	Eigen::Affine3f transform0;
	Eigen::Affine3f transform1;

	void initRenderDevice();
	void frameBegin();
	void draw();
};

class MyFrame : public wxFrame
{
	wxDECLARE_EVENT_TABLE();

public:
	MyFrame(const wxString& title, const wxPoint& pos, const wxSize& size);
	~MyFrame();

	bool Destroy();

protected:
	std::vector<MyRenderWindow*> mRenderWindows;

	void OnFullScreen(wxCommandEvent& event);
	void _OnFullScreen();

	void OnStayOnTop(wxCommandEvent& event);
};

wxBEGIN_EVENT_TABLE(MyFrame, wxFrame)
EVT_MENU(ID_FULLSCREEN, MyFrame::OnFullScreen)
EVT_MENU(ID_STAY_ON_TOP, MyFrame::OnStayOnTop)
wxEND_EVENT_TABLE()

MyRenderWindow::MyRenderWindow(wxWindow *parent, wxWindowID id, const wxString &title, const wxPoint &pos, const wxSize &size, long style)
: wxTopLevelWindow(parent, id, title, pos, size, style)
{
	ZZLAB_TRACE_THIS();
}

MyRenderWindow::~MyRenderWindow()
{
	ZZLAB_TRACE_THIS();
}

void MyRenderWindow::init()
{
	gfx::_RenderService->post(bind(&MyRenderWindow::initRenderDevice, this));
}

void MyRenderWindow::initRenderDevice()
{
	wxSize sz = GetSize();
	renderDevice.load(_Settings.first_node(L"d3d9exdev"), (HWND)GetHandle());
	renderDevice.init(d3d9ex, -1, (HWND)GetParent()->GetHandle());

	d3d9::loadAssets(&renderDevice, _DataPath / "assets.xml");

	mFrameBeginDelegate.connect(&MyRenderWindow::frameBegin, this);
	mDrawDelegate.connect(&MyRenderWindow::draw, this);

	mEffect = renderDevice.get<d3d9::EffectResource>(L"Unlit.fx");
	mMesh = renderDevice.get<d3d9::QuadMeshResource>(L"Quad");
	mTexture = renderDevice.get<d3d9::TextureResource>(L"aa.png");
	mDiffuse = D3DXCOLOR(1, 1, 1, 1);

	camera.proj = gfx::orthoLH(GetSize().x, GetSize().y, 1.0f, 100.0f);
	camera.updateT(
		gfx::lookAtLH(
		Eigen::Vector3f(0, 0, -10),
		Eigen::Vector3f(0.0f, 0.0f, 0.0f),
		Eigen::Vector3f::UnitY()));

	t2 = Eigen::Affine3f::Identity() *
		Eigen::Translation3f(-GetSize().x / 2.0f, GetSize().y / 2.0f, 0.0f);

	t0 = Eigen::Affine3f::Identity() *
		Eigen::Translation3f(0.5f, -0.5f, 0.0f);

	renderDevice.waitForFrameBegin(mFrameBeginDelegate());
	renderDevice.waitForDraw(mDrawDelegate());
}

bool MyRenderWindow::Destroy()
{
	ZZLAB_TRACE_THIS();

	renderDevice.cancel();

	return wxTopLevelWindow::Destroy();
}

void MyRenderWindow::frameBegin()
{
	mTimer.now = sTimer.getTime();

	HR(renderDevice.dev->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 
		D3DXCOLOR(0.098039225f, 0.098039225f, 0.439215720f, 1.000000000f), 1.0f, 0.0f));

	// Update our time
	static float t = 0.0f;
	static float dt = 0.f;
	{
		static int64_t dwTimeStart = 0;
		static int64_t dwTimeLast = 0;
		int64_t dwTimeCur = mTimer.getTime();
		if (dwTimeStart == 0)
			dwTimeStart = dwTimeCur;
		t = (dwTimeCur - dwTimeStart) / utils::Timer::fTimeUnit;
		dt = (dwTimeCur - dwTimeLast) / utils::Timer::fTimeUnit;
		dwTimeLast = dwTimeCur;
	}

	transform0 = Eigen::Affine3f::Identity() *
		Eigen::Translation3f(300.0f, -300.0f, 0.0f) *
		Eigen::AngleAxisf(t, Eigen::Vector3f::UnitZ()) *
		Eigen::Scaling(400.0f, 400.0f, 1.0f);

	transform1 = Eigen::Affine3f::Identity() *
		Eigen::Translation3f(800.0f, -800.0f, 0.0f) *
		Eigen::AngleAxisf(t, Eigen::Vector3f::UnitZ()) *
		Eigen::Scaling(400.0f, 400.0f, 1.0f);

	renderDevice.waitForFrameBegin(mFrameBeginDelegate());
}

void MyRenderWindow::draw()
{
	mEffect->effect->SetTexture("MainTex", mTexture->textures[0]);
	mEffect->effect->SetFloatArray("Diffuse", mDiffuse, sizeof(mDiffuse));

	{
		Eigen::Projective3f MATRIX_MVP = camera.matrix() * t2 * transform0 * t0;
		mEffect->effect->SetMatrix("MATRIX_MVP", (const D3DXMATRIX *)MATRIX_MVP.matrix().data());
		mMesh->draw(mEffect->effect);
	}

	{
		Eigen::Projective3f MATRIX_MVP = camera.matrix() * t2 * transform1 * t0;
		mEffect->effect->SetMatrix("MATRIX_MVP", (const D3DXMATRIX *)MATRIX_MVP.matrix().data());
		mMesh->draw(mEffect->effect);
	}

	renderDevice.waitForDraw(mDrawDelegate());
}

bool MyApp::OnInit()
{
	// set log level
	_LogFlags = LOG_TO_WIN32DEBUG;
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

	d3d9ex = d3d9::createEx();
	zzlab::startWorkerService();
	gfx::startRenderService();

	MyFrame *frame = new MyFrame("Show Control", wxPoint(0, 0), wxSize(500, 300));
	frame->Show(true);

	return true;
}

int MyApp::OnExit()
{
	gfx::stopRenderService();
	zzlab::stopWorkerService();
	d3d9ex = NULL;

	return wx::App::OnExit();
}

MyFrame::MyFrame(const wxString& title, const wxPoint& pos, const wxSize& size) :
wxFrame(NULL, wxID_ANY, title, pos, size)
{
	ZZLAB_TRACE_THIS();

	wxMenu *menuFile = new wxMenu;
	menuFile->Append(ID_FULLSCREEN, "&Toggle Full Screen\tCtrl+F");
	menuFile->Append(ID_STAY_ON_TOP, "&Toggle Stay-on-top");
	menuFile->Append(wxID_EXIT);

	wxMenuBar *menuBar = new wxMenuBar;
	menuBar->Append(menuFile, "&File");

	SetMenuBar(menuBar);
	CreateStatusBar();

	if (0)
	{
		MyRenderWindow* rw = wx::loadWindow<MyRenderWindow>(_Settings.first_node(L"test0"), this);
		rw->init();

		mRenderWindows.push_back(rw);
		rw->Show();
	}

	if (1)
	{
		MyRenderWindow* rw = wx::loadWindow<MyRenderWindow>(_Settings.first_node(L"test1"), this);
		rw->init();

		mRenderWindows.push_back(rw);
		rw->Show();
	}
}

MyFrame::~MyFrame()
{
	ZZLAB_TRACE_THIS();
}

bool MyFrame::Destroy()
{
	ZZLAB_TRACE_THIS();

	for (auto w : mRenderWindows)
	{
		w->Destroy();
	}

	return wxFrame::Destroy();
}

void MyFrame::OnFullScreen(wxCommandEvent& event)
{
	gfx::_RenderService->post(boost::bind(&MyFrame::_OnFullScreen, this));
}

void MyFrame::_OnFullScreen()
{
	ZZLAB_INFO("Toggle full screen mode");

	for (auto x : mRenderWindows)
	{
		x->renderDevice.toggleFullscreen();
	}
}

void MyFrame::OnStayOnTop(wxCommandEvent& event)
{
	ZZLAB_INFO("Toggle stay on top mode");

	for (auto x : mRenderWindows)
	{
		x->SetWindowStyleFlag(x->GetWindowStyleFlag() ^ wxSTAY_ON_TOP);
	}
}

wxIMPLEMENT_APP(MyApp);
