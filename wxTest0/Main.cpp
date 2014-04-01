#include "stdafx.h"

#include "zzlab.h"
#include "zzlab/av.h"
#include "zzlab/utils.h"
#include "zzlab/pystring.h"
#include "zzlab/net.h"
#include "zzlab/wx.h"
#include "zzlab/d3d9.h"

#include <sstream>

using namespace boost;
using namespace zzlab;

ZZLAB_USE_LOG4CPLUS(wxTest0);

d3d9::IDirect3D9ExPtr d3d9ex;
utils::Timer* _Timer = NULL;
av::AudioDevice* _AudioDevice = NULL;

class MyApp : public wx::App
{
public:
	virtual bool OnInit();
	virtual int OnExit();

protected:
	void initResources();
};

enum
{
	ID_Hello = 1,
	ID_Close,
};

class ClearScene
{
public:
	d3d9::Device* dev;

	explicit ClearScene() : dev(NULL)
	{
		ZZLAB_TRACE_THIS();
	}

	virtual ~ClearScene()
	{
		ZZLAB_TRACE_THIS();
	}

	void init()
	{
		mDrawDelegate.connect(bind(&ClearScene::draw, this));

		dev->rendererEvents.waitForDraw(mDrawDelegate(), gfx::RendererEvents::LAYER_0);
	}

protected:
	utils::SharedEvent0 mDrawDelegate;

	void draw()
	{
		//ZZLAB_TRACE_THIS();

		HR(dev->dev->Clear(0, NULL,
			D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0));

		dev->rendererEvents.waitForDraw(mDrawDelegate());
	}
};

class VideoQuadRenderer
{
public:
	d3d9::Device* dev;
	d3d9::DynamicYV12TextureResource* videoTexture;
	gfx::Camera camera;
	Eigen::Affine3f t2;
	Eigen::Affine3f t1;
	Eigen::Affine3f t0;

	explicit VideoQuadRenderer() : dev(NULL), videoTexture(NULL)
	{
		ZZLAB_TRACE_THIS();
	}

	virtual ~VideoQuadRenderer()
	{
		ZZLAB_TRACE_THIS();
	}

	void load(XmlNode* node)
	{
		t1 = Eigen::Affine3f::Identity();

		XmlAttribute* attr = node->first_attribute("position");
		if (attr)
		{
			std::vector<std::string> tokens;
			pystring::split(attr->value(), tokens, ",");
			if (tokens.size() == 2)
			{
				t1 = t1 * Eigen::Translation3f(
					atof(tokens[0].c_str()), 
					-atof(tokens[1].c_str()), 0.0f);
			}
		}

		attr = node->first_attribute("size");
		if (attr)
		{
			std::vector<std::string> tokens;
			pystring::split(attr->value(), tokens, ",");
			if (tokens.size() == 2)
			{
				t1 = t1 * Eigen::Scaling(
					(float)atof(tokens[0].c_str()),
					(float)atof(tokens[1].c_str()), 0.0f);
			}
		}
	}

	void init()
	{
		mQuad = dev->resources->get<d3d9::QuadMeshResource>("Quad");
		mEffect = dev->resources->get<d3d9::EffectResource>("Unlit_YUV.fx");

		camera.proj = gfx::orthoLH(1024.0f, 768.0f, 1.0f, 100.0f);
		camera.updateT(
			gfx::lookAtLH(
			Eigen::Vector3f(0, 0, -10),
			Eigen::Vector3f(0.0f, 0.0f, 0.0f),
			Eigen::Vector3f::UnitY()));

		t2 = Eigen::Affine3f::Identity() *
			Eigen::Translation3f(-1024.0f / 2.0f, 768.0f / 2.0f, 0.0f);
		
		//t1 = Eigen::Affine3f::Identity() *
		//	Eigen::Translation3f(100.0f, -100.0f, 0.0f) *
		//	Eigen::Scaling(100.0f, 100.0f, 1.0f);

		t0 = Eigen::Affine3f::Identity() *
			Eigen::Translation3f(0.5f, -0.5f, 0.0f);

		mDrawDelegate.connect(bind(&VideoQuadRenderer::draw, this));

		dev->rendererEvents.waitForDraw(mDrawDelegate());
	}

protected:
	utils::SharedEvent0 mDrawDelegate;

	d3d9::QuadMeshResource* mQuad;
	d3d9::EffectResource* mEffect;

	void draw()
	{
		//ZZLAB_TRACE_THIS();

		Eigen::Projective3f MATRIX_MVP = camera.matrix() * t2 * t1 * t0;
		mEffect->effect->SetMatrix("MATRIX_MVP", (const D3DXMATRIX *)MATRIX_MVP.matrix().data());
		if (videoTexture)
		{
			mEffect->effect->SetTexture("MainTex_Y", videoTexture->texture.get<0>().get<1>());
			mEffect->effect->SetTexture("MainTex_U", videoTexture->texture.get<1>().get<1>());
			mEffect->effect->SetTexture("MainTex_V", videoTexture->texture.get<2>().get<1>());
		}

		mQuad->draw(mEffect->effect);

		dev->rendererEvents.waitForDraw(mDrawDelegate());
	}
};

class MyFrame : public wxFrame
{
public:
	MyFrame(const wxString& title, const wxPoint& pos, const wxSize& size) :
		wxFrame(NULL, wxID_ANY, title, pos, size)
	{
		wxMenu *menuFile = new wxMenu;
		menuFile->Append(ID_Hello, "&Hello...\tCtrl-H",
			"Help string shown in status bar for this menu item");
		menuFile->AppendSeparator();
		menuFile->Append(wxID_EXIT);
		wxMenu *menuHelp = new wxMenu;
		menuHelp->Append(wxID_ABOUT);
		wxMenuBar *menuBar = new wxMenuBar;
		menuBar->Append(menuFile, "&File");
		menuBar->Append(menuHelp, "&Help");
		SetMenuBar(menuBar);
		CreateStatusBar();
		SetStatusText("Welcome to wxWidgets!");

		mDevice0 = createDevice(_Settings.first_node("Window0"));
		mDevice1 = createDevice(_Settings.first_node("Window1"));
		mDevice2 = createDevice(_Settings.first_node("Window2"));
		mDevice3 = createDevice(_Settings.first_node("Window3"));
		mDevice4 = createDevice(_Settings.first_node("Window4"));
		
		mClearScene0 = new ClearScene();
		mClearScene0->dev = mDevice0;
		mClearScene0->init();

		mMediaPlayer = NULL;

		mQuad0 = NULL;
		mVideoTexture0 = NULL;

		mQuad1 = NULL;
		mVideoTexture1 = NULL;
	}

	~MyFrame()
	{
		delete mDevice0;
		delete mDevice1;
		delete mDevice2;
		delete mDevice3;
		delete mDevice4;

		delete mClearScene0;
	}

private:
	d3d9::Device* mDevice0;
	d3d9::Device* mDevice1;
	d3d9::Device* mDevice2;
	d3d9::Device* mDevice3;
	d3d9::Device* mDevice4;

	ClearScene* mClearScene0;

	av::SimpleMediaPlayer* mMediaPlayer;

	VideoQuadRenderer* mQuad0;
	d3d9::DynamicYV12TextureResource* mVideoTexture0;

	VideoQuadRenderer* mQuad1;
	d3d9::DynamicYV12TextureResource* mVideoTexture1;

	d3d9::Device* createDevice(XmlNode* node)
	{
		wxTopLevelWindow* wnd = wx::loadWindow<wxTopLevelWindow>(node, this);
		d3d9::Device* dev = new d3d9::Device();
		dev->load(_Settings.first_node("d3ddev"), d3d9ex, -1, (HWND)wnd->GetHandle(), (HWND)GetHandle());
		dev->timeSource = _Timer;
		dev->init();

		{
			d3d9::EffectResource* effect = new d3d9::EffectResource();
			effect->dev = dev->dev;
			effect->deviceResourceEvents = &dev->deviceResourceEvents;
			effect->path = _AssetsPath / L"Effects" / L"Unlit_YUV.fx";
			effect->init();
			dev->resources->set("Unlit_YUV.fx", effect);
		}

		{
			d3d9::EffectResource* effect = new d3d9::EffectResource();
			effect->dev = dev->dev;
			effect->deviceResourceEvents = &dev->deviceResourceEvents;
			effect->path = _AssetsPath / L"Effects" / L"Unlit.fx";
			effect->init();
			dev->resources->set("Unlit.fx", effect);
		}

		{
			d3d9::QuadMeshResource* mesh = new d3d9::QuadMeshResource();
			mesh->dev = dev->dev;
			mesh->deviceResourceEvents = &dev->deviceResourceEvents;
			mesh->init();
			dev->resources->set("Quad", mesh);
		}

		wnd->Show();

		return dev;
	}

	void endOfStream()
	{
		ZZLAB_TRACE("END OF STREAM");
	}

	void handleStop()
	{
		if (mMediaPlayer)
		{
			delete mMediaPlayer;
			mMediaPlayer = NULL;
		}

		if (mQuad0)
		{
			delete mQuad0;
			mQuad0 = NULL;
		}

		if (mVideoTexture0)
		{
			delete mVideoTexture0;
			mVideoTexture0 = NULL;
		}

		if (mQuad1)
		{
			delete mQuad1;
			mQuad1 = NULL;
		}

		if (mVideoTexture1)
		{
			delete mVideoTexture1;
			mVideoTexture1 = NULL;
		}
	}

	void OnHello(wxCommandEvent& event)
	{
		if (mMediaPlayer)
		{
			mMediaPlayer->stop(bind(&MyFrame::handleStop, this));
			return;
		}

		mMediaPlayer = new av::SimpleMediaPlayer();
		mMediaPlayer->source = _AssetsPath / "Videos" / "main-video.mp4";
		mMediaPlayer->timeSource = _Timer;
		mMediaPlayer->audioDevice = _AudioDevice;
		mMediaPlayer->load(_Settings.first_node("MediaPlayer0"));
		mMediaPlayer->init();

		AVCodecContext* ctx = mMediaPlayer->getVideoCodecContext();
		if (ctx)
		{
			mVideoTexture0 = new d3d9::DynamicYV12TextureResource();
			mVideoTexture0->dev = mDevice0->dev;
			mVideoTexture0->width = ctx->width;
			mVideoTexture0->height = ctx->height;
			mVideoTexture0->deviceResourceEvents = &mDevice0->deviceResourceEvents;
			mVideoTexture0->init();
			
			mQuad0 = new VideoQuadRenderer();
			mQuad0->dev = mDevice0;
			mQuad0->videoTexture = mVideoTexture0;
			mQuad0->load(_Settings.first_node("Quad0"));
			mQuad0->init();

			mVideoTexture1 = new d3d9::DynamicYV12TextureResource();
			mVideoTexture1->dev = mDevice0->dev;
			mVideoTexture1->width = ctx->width;
			mVideoTexture1->height = ctx->height;
			mVideoTexture1->deviceResourceEvents = &mDevice0->deviceResourceEvents;
			mVideoTexture1->init();

			mQuad1 = new VideoQuadRenderer();
			mQuad1->dev = mDevice0;
			mQuad1->videoTexture = mVideoTexture1;
			mQuad1->load(_Settings.first_node("Quad1"));
			mQuad1->init();

			mMediaPlayer->getVideoRenderer()->rendererEvents = &mDevice0->rendererEvents;
			mMediaPlayer->getVideoRenderer()->renderFrame = bind(&MyFrame::renderFrame, this, _1);
		}

		mMediaPlayer->play(_Timer->getTime(), _MainService.wrap(bind(&MyFrame::endOfStream, this)));
	}

	void renderFrame(AVFrame* frame)
	{
		mVideoTexture0->update(frame);
		mVideoTexture1->update(frame);
	}

	void OnExit(wxCommandEvent& event)
	{
		Close();
	}

	void OnAbout(wxCommandEvent& event)
	{
		wxMessageBox("This is a wxWidgets' Hello world sample",
			"About Hello World", wxOK | wxICON_INFORMATION);
	}

	void OnClose(wxCloseEvent& event)
	{
		if (mMediaPlayer)
			mMediaPlayer->stop(bind(&MyFrame::handleClose, this));
		else
			handleClose();
	}

	void handleClose()
	{
		handleStop();

		utils::stopAllServices();

		Destroy();
	}

	wxDECLARE_EVENT_TABLE();
};

wxBEGIN_EVENT_TABLE(MyFrame, wxFrame)
EVT_MENU(ID_Hello, MyFrame::OnHello)
EVT_MENU(wxID_EXIT, MyFrame::OnExit)
EVT_MENU(wxID_ABOUT, MyFrame::OnAbout)
EVT_CLOSE(MyFrame::OnClose)
wxEND_EVENT_TABLE()

wxIMPLEMENT_APP(MyApp);

bool MyApp::OnInit()
{
	// set log level
	_LogFlags = LOG_TO_WIN32DEBUG;
	log4cplus::Logger::getRoot().setLogLevel(log4cplus::TRACE_LOG_LEVEL);

	// install plugins
	utils::install();
	av::install();
	wx::install();
	d3d9::install();

	if (!wx::App::OnInit())
		return false;

	ZZLAB_TRACE_THIS();

	av::AudioDevice::dumpInfo();

	d3d9ex = d3d9::createEx();
	_Timer = new utils::HiPerfTimer();
	_AudioDevice = new av::AudioDevice();
	_AudioDevice->load(_Settings.first_node("audio0"));
	_AudioDevice->outputParameters.sampleFormat = paFloat32;
	_AudioDevice->init();
	_AudioDevice->start();

	MyFrame *frame = new MyFrame("Hello World", wxPoint(50, 50), wxSize(450, 340));
	frame->Show(true);

	return true;
}

int MyApp::OnExit()
{
	delete _AudioDevice;
	_AudioDevice = NULL;

	delete _Timer;
	_Timer = NULL;

	d3d9ex = NULL;

	return wx::App::OnExit();
}