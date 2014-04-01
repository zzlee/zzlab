#include "stdafx.h"

#include "zzlab.h"
#include "zzlab/av.h"
#include "zzlab/utils.h"
#include "zzlab/pystring.h"
#include "zzlab/net.h"
#include "zzlab/wx.h"
#include "zzlab/d3d9.h"

#include <sstream>
#include <utility>
#include <boost/array.hpp>
#include <boost/format.hpp>
#include <boost/filesystem.hpp>

#include <wx/listctrl.h>

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
	ID_StopAll = 1,

	ID_Schedule = 0x1000,
};

static cv::Rect loadROI(XmlNode* node)
{
	cv::Rect roi;

	XmlAttribute* attr = node->first_attribute("position");
	if (attr)
	{
		std::vector<std::string> tokens;
		pystring::split(attr->value(), tokens, ",");
		if (tokens.size() == 2)
		{
			roi.x = atoi(tokens[0].c_str());
			roi.y = atoi(tokens[1].c_str());
		}
	}

	attr = node->first_attribute("size");
	if (attr)
	{
		std::vector<std::string> tokens;
		pystring::split(attr->value(), tokens, ",");
		if (tokens.size() == 2)
		{
			roi.width = atoi(tokens[0].c_str());
			roi.height = atoi(tokens[1].c_str());
		}
	}

	return roi;
}

static void getROI_YUV420P(AVFrame* src, AVFrame* dst, const cv::Rect& roi)
{
	dst->width = roi.width;
	dst->height = roi.height;
	dst->data[0] = src->data[0] + roi.x + roi.y * src->linesize[0];
	dst->data[1] = src->data[1] + roi.x / 2 + roi.y / 2 * src->linesize[1];
	dst->data[2] = src->data[2] + roi.x / 2 + roi.y / 2 * src->linesize[2];
	dst->linesize[0] = src->linesize[0];
	dst->linesize[1] = src->linesize[1];
	dst->linesize[2] = src->linesize[2];
}

struct FileNameMapping : public boost::unordered_map<std::string, boost::filesystem::wpath>
{
	void load(XmlNode* node)
	{
		filesystem::wpath root = _AssetsPath / "Videos";

		for (XmlNode* file = node->first_node("File"); file; file = file->next_sibling("File"))
		{
			insert(value_type(file->first_attribute("name")->value(),
				root / file->first_attribute("dst")->value()));
		}
	}
};
FileNameMapping _FileNameMapping;

struct Cut {
	size_t id;
	std::string A;
	std::string A_ending;
	std::string B;
	std::string C;
	std::string BC_ending;
	std::string fadeOut;

	void load(XmlNode* node)
	{
		A = node->first_attribute("A")->value();
		A_ending = node->first_attribute("A-ending")->value();
		B = node->first_attribute("B")->value();
		C = node->first_attribute("C")->value();
		BC_ending = node->first_attribute("BC-ending")->value();
		fadeOut = node->first_attribute("fade-out")->value();
	}
};

struct Schedule : public std::vector<Cut>
{
	void load(XmlNode* node)
	{
		size_t id = 0;
		for (XmlNode* cut = node->first_node("Cut"); cut; cut = cut->next_sibling("Cut"))
		{
			push_back(Cut());
			back().id = id++;
			back().load(cut);
		}
	}
};
Schedule _Schedule;

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

static void dummy() {}

class MyFrame : public wxFrame
{
public:
	MyFrame(const wxString& title, const wxPoint& pos, const wxSize& size) :
		wxFrame(NULL, wxID_ANY, title, pos, size)
	{
		wxMenu *menuFile = new wxMenu;
		menuFile->Append(ID_StopAll, "&Stop all\tCtrl-S");
		menuFile->Append(wxID_SEPARATOR);
		menuFile->Append(wxID_EXIT);

		wxMenuBar *menuBar = new wxMenuBar;
		menuBar->Append(menuFile, "&File");
		SetMenuBar(menuBar);
		CreateStatusBar();

		wxListView* scheList = new wxListView(this, ID_Schedule, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL);
		scheList->AppendColumn(L"Cut");
		scheList->AppendColumn(L"A");
		scheList->AppendColumn(L"A結束動作");
		scheList->AppendColumn(L"B");
		scheList->AppendColumn(L"C");
		scheList->AppendColumn(L"BC結束動作");
		scheList->AppendColumn(L"淡出");

		for (size_t i = 0; i < _Schedule.size();++i)
		{
			const Cut& cut = _Schedule[i];
			scheList->InsertItem(i, (format("%d") % i).str());
			scheList->SetItem(i, 1, cut.A);
			scheList->SetItem(i, 2, cut.A_ending);
			scheList->SetItem(i, 3, cut.B);
			scheList->SetItem(i, 4, cut.C);
			scheList->SetItem(i, 5, cut.BC_ending);
			scheList->SetItem(i, 6, cut.fadeOut);
		}

		mScheduleList = scheList;

		for (size_t i = 0; i < mDevices.size(); ++i)
		{
			mDevices[i] = createDevice(_Settings.first_node(
				(boost::format("Window%d") % i).str().c_str()));

			mClearScenes[i] = new d3d9::ClearScene();
			mClearScenes[i]->dev = mDevices[i]->dev;
			mClearScenes[i]->rendererEvents = &mDevices[i]->rendererEvents;
			mClearScenes[i]->init();
		}

		std::fill(mMediaPlayers.begin(), mMediaPlayers.end(), nullptr);

		std::fill(mQuads.begin(), mQuads.end(), nullptr);
		std::fill(mVideoTextures.begin(), mVideoTextures.end(), nullptr);

		for (size_t i = 0; i < mVideoROIs.size(); ++i)
		{
			mVideoROIs[i] = loadROI(_Settings.first_node(
				(boost::format("VideoROI%d") % i).str().c_str()));

			mLiveROIs[i] = loadROI(_Settings.first_node(
				(boost::format("LiveROI%d") % i).str().c_str()));
		}

		mState = State_Ready;
		mCurrentCut = NULL;
		mAfterStop = bind(dummy);
	}

	~MyFrame()
	{
		for (size_t i = 0; i < mDevices.size(); ++i)
		{
			delete mDevices[i];
			delete mClearScenes[i];
		}
	}

protected:
	enum State
	{
		State_Ready,
		State_Playing,
		State_Stopping
	} mState;

	wxListView* mScheduleList;

	Cut* mCurrentCut;

	boost::array<d3d9::Device*, 5> mDevices;
	boost::array<d3d9::ClearScene*, 5> mClearScenes;

	boost::array<av::SimpleMediaPlayer*, 3> mMediaPlayers;

	boost::array<VideoQuadRenderer*, 7> mQuads;
	boost::array<d3d9::DynamicYV12TextureResource*, 7> mVideoTextures;
	boost::array<cv::Rect, 7> mVideoROIs;
	boost::array<cv::Rect, 7> mLiveROIs;

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

	void OnItemActivated(wxListEvent& event)
	{
		//ZZLAB_TRACE_THIS();

		const wxListItem& item = event.GetItem();

		ZZLAB_TRACE("Item " << item.GetId() << " is about to play...");

		switch (mState)
		{
		case State_Ready:
			// TODO: fade-out-effect
			play(item.GetId());
			break;

		case State_Playing:
			if (item.GetId() == mCurrentCut->id)
				ZZLAB_TRACE("Item " << item.GetId() << " is already playing.");
			else
				stopAll(bind(&MyFrame::play, this, item.GetId()));
			break;
		}
	}

	void play(size_t idx)
	{
		for (size_t i = 0; i < mScheduleList->GetItemCount(); ++i)
			mScheduleList->SetItemBackgroundColour(i, wxColour(255, 255, 255));

		mScheduleList->SetItemBackgroundColour(idx, wxColour(255, 0, 0));
		mScheduleList->SetItemState(idx, 0, wxLIST_STATE_SELECTED);

		mCurrentCut = &_Schedule[idx];

		int64_t playTime = _Timer->getTime();
		play0(_FileNameMapping[mCurrentCut->A], playTime);
		play1(_FileNameMapping[mCurrentCut->B], playTime);
		play2(_FileNameMapping[mCurrentCut->C], playTime);

		mState = State_Playing;
	}

	void endOfStream(size_t idx)
	{
		ZZLAB_TRACE("END OF STREAM, " << idx);
	}

	void handleStop(size_t idx)
	{
		if (mMediaPlayers[idx])
		{
			delete mMediaPlayers[idx];
			mMediaPlayers[idx] = NULL;
		}

		for (size_t i = 0;i < mMediaPlayers.size(); ++i)
		{
			if (mMediaPlayers[i])
				return;
		}

		ZZLAB_INFO("All media players are stopped now.");

		for (size_t i = 0; i < mQuads.size(); ++i)
		{
			if (mQuads[i])
			{
				delete mQuads[i];
				mQuads[i] = NULL;
			}
		}

		for (size_t i = 0; i < mVideoTextures.size(); ++i)
		{
			if (mVideoTextures[i])
			{
				delete mVideoTextures[i];
				mVideoTextures[i] = NULL;
			}
		}

		ZZLAB_TRACE_VALUE(mCurrentCut->fadeOut);

		mCurrentCut = NULL;
		mState = State_Ready;
		mAfterStop();
		mAfterStop = bind(dummy);
	}

	void stopMediaPlayer(size_t idx)
	{
		mMediaPlayers[idx]->stop(_MainService.wrap(bind(&MyFrame::handleStop, this, idx)));
	}

	function<void()> mAfterStop;
	void stopAll(function<void()> afterStop)
	{
		mAfterStop = afterStop;
		mState = State_Stopping;

		stopMediaPlayer(0);
		stopMediaPlayer(1);
		stopMediaPlayer(2);
	}
	
	void initQuad(size_t device_index, size_t quad_index)
	{
		d3d9::Device* device = mDevices[device_index];
		const cv::Rect& roi = mVideoROIs[quad_index];

		d3d9::DynamicYV12TextureResource* texture = new d3d9::DynamicYV12TextureResource();
		texture->dev = device->dev;
		texture->width = roi.width;
		texture->height = roi.height;
		texture->deviceResourceEvents = &device->deviceResourceEvents;
		texture->init();
		texture->set(d3d9::ScalarYUV(0, 128, 128));

		VideoQuadRenderer* quad = new VideoQuadRenderer();
		quad->dev = device;
		quad->videoTexture = texture;
		quad->load(_Settings.first_node((format("Quad%d") % quad_index).str().c_str()));
		quad->init();

		mVideoTextures[quad_index] = texture;
		mQuads[quad_index] = quad;
	}

	void initMediaPlayer(size_t mp_index, boost::filesystem::wpath path, int64_t playTime)
	{
		av::SimpleMediaPlayer* mp = new av::SimpleMediaPlayer();
		mp->source = path;
		mp->timeSource = _Timer;
		mp->audioDevice = _AudioDevice;
		mp->load(_Settings.first_node("MediaPlayer0"));
		mp->init();

		mp->getVideoRenderer()->rendererEvents = &mDevices[0]->rendererEvents;
		mp->getVideoRenderer()->renderFrame = bind(&MyFrame::renderFrame, this, _1, mp_index);

		mp->play(playTime, _MainService.wrap(bind(&MyFrame::endOfStream, this, mp_index)));

		mMediaPlayers[mp_index] = mp;
	}

	void updateTexture(AVFrame* frame, size_t idx)
	{
		AVFrame tmp;

		getROI_YUV420P(frame, &tmp, mVideoROIs[idx]);
		mVideoTextures[idx]->update(&tmp);
	}

	void renderFrame(AVFrame* frame, size_t idx)
	{
		switch (idx)
		{
		case 0:
			updateTexture(frame, 0);
			updateTexture(frame, 1);
			break;

		case 1:
			updateTexture(frame, 2);
			updateTexture(frame, 3);
			updateTexture(frame, 4);
			break;

		case 2:
			updateTexture(frame, 5);
			updateTexture(frame, 6);
			break;
		}
	}

	void play0(boost::filesystem::wpath path, int64_t playTime)
	{
		if (path.filename() == "LIVE")
		{
			// TODO: LIVE stream
		}
		else
		{
			initQuad(0, 0);
			initQuad(0, 1);

			// create media player
			initMediaPlayer(0, path, playTime);
		}
	}

	void play1(boost::filesystem::wpath path, int64_t playTime)
	{
		initQuad(1, 2);
		initQuad(2, 3);
		initQuad(3, 4);

		// create media player
		initMediaPlayer(1, path, playTime);
	}

	void play2(boost::filesystem::wpath path, int64_t playTime)
	{
		// renderer for device 2
		initQuad(4, 5);
		initQuad(4, 6);

		// create media player
		initMediaPlayer(2, path, playTime);
	}

	void OnStopAll(wxCommandEvent& event)
	{
		stopAll(bind(dummy));
	}

	void OnExit(wxCommandEvent& event)
	{
		Close();
	}

	void OnClose(wxCloseEvent& event)
	{
		if (mState == State_Playing)
			stopAll(bind(&MyFrame::shutdown, this));
		else
			shutdown();
	}

	void shutdown()
	{
		utils::stopAllServices();

		Destroy();
	}

	wxDECLARE_EVENT_TABLE();
};

wxBEGIN_EVENT_TABLE(MyFrame, wxFrame)
EVT_MENU(ID_StopAll, MyFrame::OnStopAll)
EVT_MENU(wxID_EXIT, MyFrame::OnExit)
EVT_CLOSE(MyFrame::OnClose)
EVT_LIST_ITEM_ACTIVATED(ID_Schedule, MyFrame::OnItemActivated)
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

	_FileNameMapping.load(_Settings.first_node("FileNameMapping"));
	_Schedule.load(_Settings.first_node("Schedule"));

	MyFrame *frame = new MyFrame("Show Control", wxPoint(0, 0), wxSize(650, 440));
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