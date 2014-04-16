#include "stdafx.h"

#include "zzlab.h"
#include "zzlab/av.h"
#include "zzlab/utils.h"
#include "zzlab/pystring.h"
#include "zzlab/net.h"
#include "zzlab/wx.h"
#include "zzlab/d3d9.h"

#include "eb.pb.h"

#include <sstream>
#include <utility>
#include <boost/array.hpp>
#include <boost/format.hpp>
#include <boost/filesystem.hpp>
#include <boost/lockfree/spsc_queue.hpp>

#include <rapidxml/rapidxml_utils.hpp>

#include <wx/listctrl.h>

#include <utility>

using namespace boost;
using namespace zzlab;
using namespace google;

ZZLAB_USE_LOG4CPLUS(wxTest1);

d3d9::IDirect3D9ExPtr d3d9ex;
utils::Timer* _Timer = NULL;

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

	ID_Schedule = 0x1000,
};

static void dummy() {}

static std::pair<std::string, D3DXCOLOR> defaultColours[] =
{
	std::make_pair("white", D3DXCOLOR(1, 1, 1, 1)), 
	std::make_pair("yellow", D3DXCOLOR(1, 1, 0, 1)),
	std::make_pair("fuchsia", D3DXCOLOR(1, 0, 1, 1)), 
	std::make_pair("red", D3DXCOLOR(1, 0, 0, 1)),
	std::make_pair("silver", D3DXCOLOR(0.75f, 0.75f, 0.75f, 1)),
	std::make_pair("gray", D3DXCOLOR(0.5f, 0.5f, 0.5f, 1)),
	std::make_pair("olive", D3DXCOLOR(0.5f, 0.5f, 0, 1)),
	std::make_pair("purple", D3DXCOLOR(0.5f, 0, 0.5f, 1)),
	std::make_pair("maroon", D3DXCOLOR(0.5f, 0, 0, 1)),
	std::make_pair("aqua", D3DXCOLOR(0, 1, 1, 1)),
	std::make_pair("lime", D3DXCOLOR(0, 1, 0, 1)),
	std::make_pair("teal", D3DXCOLOR(0, 0.5f, 0.5f, 1)),
	std::make_pair("green", D3DXCOLOR(0, 0.5f, 0, 1)),
	std::make_pair("blue", D3DXCOLOR(0, 0, 1, 1)),
	std::make_pair("navy", D3DXCOLOR(0, 0, 0.5f, 1)),
	std::make_pair("black", D3DXCOLOR(0, 0, 0, 1)),
};

int defaultSubdivLevel = 3;
int defaultCPCols = 9;
int defaultCPRows = 5;
int defaultGridWidth = 9;
int defaultGridHeight = 5;

void setDefaultWorkspace(eb::Workspace& workspace)
{
	const size_t iWidth = defaultCPCols;
	const size_t iHeight = defaultCPRows;

	{
		eb::EditorSettings *editorSettings =
			workspace.mutable_editor_settings();

		editorSettings->set_display_mode(eb::DM_MESH_GRID);
		editorSettings->set_edit_mode(eb::EM_FREE);

		{
			const std::pair<std::string, D3DXCOLOR> &clr = defaultColours[8];

			eb::Colour *colour = editorSettings->mutable_line_colour();
			colour->set_r(clr.second.r);
			colour->set_g(clr.second.g);
			colour->set_b(clr.second.b);
		}

		{
			const std::pair<std::string, D3DXCOLOR> &clr = defaultColours[10];

			eb::Colour *colour = editorSettings->mutable_background_colour();
			colour->set_r(clr.second.r);
			colour->set_g(clr.second.g);
			colour->set_b(clr.second.b);
		}

		editorSettings->set_adjust_speed(30);
		editorSettings->set_show_control_point(true);

		{
			eb::Size2i *size = editorSettings->mutable_grid_size();
			size->set_width(defaultGridWidth);
			size->set_height(defaultGridHeight);
		}

		editorSettings->set_select_mode(eb::SM_SINGLE);

		{
			eb::Point2i *pt = editorSettings->mutable_selected();
			pt->set_x(iWidth / 2);
			pt->set_y(iHeight / 2);
		}
	}

	{
		const float fWidth1 = iWidth - 1;
		const float fHeight1 = iHeight - 1;

		eb::LatticeSettings *latticeSettings =
			workspace.mutable_lattice_settings();

		{
			eb::Size2i *size = latticeSettings->mutable_size();
			size->set_width(iWidth), size->set_height(iHeight);
		}

		latticeSettings->set_subdiv_level(3);

		{
			protobuf::RepeatedPtrField<eb::ControlPoint> *cps =
				latticeSettings->mutable_control_points();

			// initialize vertices
			for (size_t y = 0; y < iHeight; ++y)
			{
				for (size_t x = 0; x < iWidth; ++x)
				{
					eb::ControlPoint *cp = cps->Add();
					cp->set_x(x / fWidth1);
					cp->set_y(y / fHeight1);
					if ((x == 0 && y == 0) || (x == 0 && y == iHeight - 1)
						|| (x == iWidth - 1 && y == iHeight - 1)
						|| (x == iWidth - 1 && y == 0))
						cp->set_type(eb::CP_CEASE);
					else
						cp->set_type(eb::CP_FREE);
				}
			}
		}
	}

	{
		eb::EdgeBlendSettings *edgeBlendSettings =
			workspace.mutable_left_edge_settings();

		edgeBlendSettings->set_enable(false);
		edgeBlendSettings->set_width(256.0f);
		edgeBlendSettings->set_blending(1.0f);
		edgeBlendSettings->set_gamma(2.0f);
		edgeBlendSettings->set_center(0.5f);
	}

	{
		eb::EdgeBlendSettings *edgeBlendSettings =
			workspace.mutable_right_edge_settings();

		edgeBlendSettings->set_enable(false);
		edgeBlendSettings->set_width(256.0f);
		edgeBlendSettings->set_blending(1.0f);
		edgeBlendSettings->set_gamma(2.0f);
		edgeBlendSettings->set_center(0.5f);
	}

	{
		eb::EdgeBlendSettings *edgeBlendSettings =
			workspace.mutable_top_edge_settings();

		edgeBlendSettings->set_enable(false);
		edgeBlendSettings->set_width(192.0f);
		edgeBlendSettings->set_blending(1.0f);
		edgeBlendSettings->set_gamma(2.0f);
		edgeBlendSettings->set_center(0.5f);
	}

	{
		eb::EdgeBlendSettings *edgeBlendSettings =
			workspace.mutable_bottom_edge_settings();

		edgeBlendSettings->set_enable(false);
		edgeBlendSettings->set_width(192.0f);
		edgeBlendSettings->set_blending(1.0f);
		edgeBlendSettings->set_gamma(2.0f);
		edgeBlendSettings->set_center(0.5f);
	}
}

void genHorEdge(const eb::Workspace& workspace, d3d9::DynamicTextureResource* res)
{
	std::vector<double> left;
	{
		{
			const eb::EdgeBlendSettings &edgeBlendSettings =
				workspace.left_edge_settings();
			left.resize(edgeBlendSettings.width());
			if (edgeBlendSettings.enable())
			{
				gfx::genEdgeMatte(edgeBlendSettings.blending(),
					edgeBlendSettings.gamma(), edgeBlendSettings.center(),
					left.begin(), left.end());
			}
			else
			{
				fill(left.begin(), left.end(), 1);
			}
		}
	}

	std::vector<double> right;
	{
		const eb::EdgeBlendSettings &edgeBlendSettings =
			workspace.right_edge_settings();
		right.resize(edgeBlendSettings.width());
		if (edgeBlendSettings.enable())
		{
			gfx::genEdgeMatte(edgeBlendSettings.blending(),
				edgeBlendSettings.gamma(), edgeBlendSettings.center(),
				right.begin(), right.end());
		}
		else
		{
			fill(right.begin(), right.end(), 1);
		}
	}

	cv::Mat4b a(1, res->width);
	cv::Vec4b *p = (cv::Vec4b*)a.data;
	for (size_t i = 0; i < left.size(); ++i, ++p)
	{
		*p = cv::Vec4b(255 * left[i], 255 * left[i], 255 * left[i], 255);
	}
	for (size_t i = left.size(); i < a.cols - right.size();
		++i, ++p)
	{
		*p = cv::Vec4b(255, 255, 255, 255);
	}
	for (size_t i = a.cols - right.size(); i < a.cols;
		++i, ++p)
	{
		*p = cv::Vec4b(255 * right[a.cols - i - 1], 255 * right[a.cols - i - 1], 255 * right[a.cols - i - 1], 255);
	}

	d3d9::updateDynamicTexture(res->dev, res->texture, a);
}

void genVerEdge(const eb::Workspace& workspace, d3d9::DynamicTextureResource* res)
{
	std::vector<double> top;
	{
		const eb::EdgeBlendSettings &edgeBlendSettings =
			workspace.top_edge_settings();
		top.resize(edgeBlendSettings.width());
		if (edgeBlendSettings.enable())
		{
			gfx::genEdgeMatte(edgeBlendSettings.blending(),
				edgeBlendSettings.gamma(), edgeBlendSettings.center(),
				top.begin(), top.end());
		}
		else
		{
			fill(top.begin(), top.end(), 1);
		}
	}

	std::vector<double> bottom;
	{
		const eb::EdgeBlendSettings &edgeBlendSettings =
			workspace.bottom_edge_settings();
		bottom.resize(edgeBlendSettings.width());
		if (edgeBlendSettings.enable())
		{
			gfx::genEdgeMatte(edgeBlendSettings.blending(),
				edgeBlendSettings.gamma(), edgeBlendSettings.center(),
				bottom.begin(), bottom.end());
		}
		else
		{
			fill(bottom.begin(), bottom.end(), 1);
		}
	}

	cv::Mat4b a(1, res->width);
	cv::Vec4b *p = (cv::Vec4b*)a.data;
	for (size_t i = 0; i < top.size(); ++i, ++p)
	{
		*p = cv::Vec4b(255 * top[i], 255 * top[i], 255 * top[i], 255);
	}
	for (size_t i = top.size(); i < a.cols - bottom.size();
		++i, ++p)
	{
		*p = cv::Vec4b(255, 255, 255, 255);
	}
	for (size_t i = a.cols - bottom.size(); i < a.cols;
		++i, ++p)
	{
		*p = cv::Vec4b(255 * bottom[a.cols - i - 1], 255 * bottom[a.cols - i - 1], 255 * bottom[a.cols - i - 1], 255);
	}

	d3d9::updateDynamicTexture(res->dev, res->texture, a);

}

struct VertexTraits
{
	enum
	{
		none = 0,
		cease = 1,
	};
};

typedef gfx::Subdivision2D<double, VertexTraits> Subdivision_t;
typedef Subdivision_t::Vertex2D Vertex2D;
typedef Subdivision_t::Vertex2Ds Vertex2Ds;
typedef Subdivision_t::Vertex2DsPtr Vertex2DsPtr;

class QuadRenderer
{
public:
	gfx::RendererEvents* rendererEvents;

	d3d9::MeshResource* mesh;
	d3d9::EffectResource* effect;

	d3d9::TextureResource* mainTex;
	gfx::Camera camera;
	Eigen::Affine3f t2;
	Eigen::Affine3f t1;
	Eigen::Affine3f t0;
	D3DXCOLOR diffuse;

	explicit QuadRenderer() : rendererEvents(NULL), mesh(NULL), effect(NULL), mainTex(NULL), diffuse(1, 1, 1, 1)
	{
		ZZLAB_TRACE_THIS();

		t1 = Eigen::Affine3f::Identity();
	}

	virtual ~QuadRenderer()
	{
		ZZLAB_TRACE_THIS();
	}

	void init()
	{
		camera.proj = gfx::orthoLH(1024.0f, 768.0f, 1.0f, 100.0f);
		camera.updateT(
			gfx::lookAtLH(
			Eigen::Vector3f(0, 0, -10),
			Eigen::Vector3f(0.0f, 0.0f, 0.0f),
			Eigen::Vector3f::UnitY()));

		t2 = Eigen::Affine3f::Identity() *
			Eigen::Translation3f(-1024.0f / 2.0f, 768.0f / 2.0f, 0.0f);

		t0 = Eigen::Affine3f::Identity() *
			Eigen::Translation3f(0.5f, -0.5f, 0.0f);

		mDrawDelegate.connect(bind(&QuadRenderer::draw, this));

		rendererEvents->waitForDraw(mDrawDelegate());
	}

protected:
	utils::SharedEvent0 mDrawDelegate;

	void draw()
	{
		//ZZLAB_TRACE_THIS();

		Eigen::Projective3f MATRIX_MVP = camera.matrix() * t2 * t1 * t0;
		effect->effect->SetMatrix("MATRIX_MVP", (const D3DXMATRIX *)MATRIX_MVP.matrix().data());
		if (mainTex)
			effect->effect->SetTexture("MainTex", mainTex->textures[0]);
		effect->effect->SetFloatArray("Diffuse", diffuse, sizeof(diffuse));

		mesh->draw(effect->effect);

		rendererEvents->waitForDraw(mDrawDelegate());
	}
};

class MyFrame : public wxFrame
{
public:
	eb::Workspace workspace;

	MyFrame(const wxString& title, const wxPoint& pos, const wxSize& size) :
		wxFrame(NULL, wxID_ANY, title, pos, size)
	{
		wxMenu *menuFile = new wxMenu;
		menuFile->Append(wxID_SEPARATOR);
		menuFile->Append(wxID_EXIT);

		wxMenuBar *menuBar = new wxMenuBar;
		menuBar->Append(menuFile, "&File");
		SetMenuBar(menuBar);
		CreateStatusBar();

		setDefaultWorkspace(workspace);

		mDevice = createDevice(_Settings.first_node("eb4Window"));

		mClearScene = new d3d9::ClearScene();
		mClearScene->dev = mDevice->dev;
		mClearScene->rendererEvents = &mDevice->rendererEvents;
		mClearScene->init();

		mVerTex = new d3d9::DynamicTextureResource();
		mVerTex->dev = mDevice->dev;
		mVerTex->deviceResourceEvents = &mDevice->deviceResourceEvents;
		mVerTex->width = 768;
		mVerTex->height = 1;
		mVerTex->format = D3DFMT_X8R8G8B8;
		mVerTex->init();

		mHorTex = new d3d9::DynamicTextureResource();
		mHorTex->dev = mDevice->dev;
		mHorTex->deviceResourceEvents = &mDevice->deviceResourceEvents;
		mHorTex->width = 1024;
		mHorTex->height = 1;
		mHorTex->format = D3DFMT_X8R8G8B8;
		mHorTex->init();

		mAdjustments.x = 0.5f / 1024;
		mAdjustments.y = 0.5f / 768;

		mControlSize.width = workspace.lattice_settings().size().width();
		mControlSize.height = workspace.lattice_settings().size().height();

		// initialize control points
		mControlPoints.resize(mControlSize.width * mControlSize.height);
		for (int y = 0; y < mControlSize.height; ++y)
		{
			for (int x = 0; x < mControlSize.width; ++x)
			{
				size_t idx = y * mControlSize.width + x;

				const eb::ControlPoint &cp =
					workspace.lattice_settings().control_points().Get(idx);

				mControlPoints[idx].x = cp.x();
				mControlPoints[idx].y = cp.y();
				mControlPoints[idx].flags = cp.type() == eb::CP_FREE ? VertexTraits::none : VertexTraits::cease;
			}
		}

		// update edge blend textures
		genVerEdge(workspace, mVerTex);
		genHorEdge(workspace, mHorTex);

		mLattice = new d3d9::LatticeMeshResource();
		mLattice->dev = mDevice->dev;
		mLattice->deviceResourceEvents = &mDevice->deviceResourceEvents;

		// subdivision
		{
			mVertices = Subdivision_t::doSubdivision(mControlSize, mControlPoints,
				workspace.lattice_settings().subdiv_level(), mSubdivSize);
			const Vertex2Ds &vertices = *mVertices;

			mLattice->width = mSubdivSize.width;
			mLattice->height = mSubdivSize.height;
			mLattice->allocVertices();

			// initialize lattice
			D3DXVECTOR2 uvScale(0.5f, 1.0f);
			D3DXVECTOR2 uvOffset(0.1f, 0.0f);
			double fWidth1 = mSubdivSize.width - 1;
			double fHeight1 = mSubdivSize.height - 1;
			for (int y = 0; y < mSubdivSize.height; ++y)
			{
				for (int x = 0; x < mSubdivSize.width; ++x)
				{
					size_t o = x + y * mSubdivSize.width;

					mLattice->vertices[o].POSITION = D3DXVECTOR3(vertices[o].x, vertices[o].y, 0.0f);
					mLattice->vertices[o].UV0.x = uvOffset.x + (x / fWidth1) * uvScale.x;
					mLattice->vertices[o].UV0.y = uvOffset.y + (1 - y / fHeight1) * uvScale.y;
				}
			}
		}

		mLattice->init();

		mRenderTexture = new d3d9::RenderTexture();
		mRenderTexture->width = 1024;
		mRenderTexture->height = 768;
		mRenderTexture->format = D3DFMT_X8R8G8B8;
		mRenderTexture->device = mDevice;
		mRenderTexture->init();

		mQuadRenderer = new QuadRenderer();
		mQuadRenderer->rendererEvents = &mRenderTexture->rendererEvents;
		mQuadRenderer->mesh = mDevice->resources->get<d3d9::QuadMeshResource>("Quad");
		mQuadRenderer->effect = mDevice->resources->get<d3d9::EffectResource>("Unlit.fx");
		mQuadRenderer->mainTex = mDevice->resources->get<d3d9::TextureResource>("aa.png");
		mQuadRenderer->diffuse = D3DXCOLOR(1, 1, 1, 1);
		mQuadRenderer->t1 = Eigen::Affine3f::Identity() *
			Eigen::Translation3f(0.0f, 0.0f, 0.0f) *
			Eigen::Scaling(500.0f, 500.0f, 1.0f);
		mQuadRenderer->init();

		mRenderer = new d3d9::eb4Renderer();
		mRenderer->rendererEvents = &mDevice->rendererEvents;
		mRenderer->effect = mDevice->resources->get<d3d9::EffectResource>("eb4.fx");
		mRenderer->mainTex = mRenderTexture;
		mRenderer->verTex = mVerTex;
		mRenderer->horTex = mHorTex;
		mRenderer->lattice = mLattice;
		mRenderer->init();
	}

	~MyFrame()
	{
		delete mDevice;
	}

protected:
	d3d9::Device* mDevice;

	d3d9::ClearScene* mClearScene;

	d3d9::eb4Renderer* mRenderer;
	d3d9::DynamicTextureResource* mHorTex;
	d3d9::DynamicTextureResource* mVerTex;
	d3d9::LatticeMeshResource* mLattice;

	d3d9::RenderTexture* mRenderTexture;

	QuadRenderer* mQuadRenderer;

	cv::Point2f mAdjustments;
	std::vector<Vertex2D> mControlPoints;
	cv::Size mControlSize;
	cv::Size mSubdivSize;
	Vertex2DsPtr mVertices;

	d3d9::Device* createDevice(XmlNode* node)
	{
		wxTopLevelWindow* wnd = wx::loadWindow<wxTopLevelWindow>(node, this);
		d3d9::Device* dev = new d3d9::Device();
		dev->load(_Settings.first_node("d3ddev"), (HWND)wnd->GetHandle());
		dev->timeSource = _Timer;
		dev->init(d3d9ex, -1, (HWND)GetHandle());

		d3d9::loadAssets(dev, _DataPath / "assets.xml");

		wnd->Show();

		return dev;
	}

	void OnExit(wxCommandEvent& event)
	{
		Close();
	}

	void OnClose(wxCloseEvent& event)
	{
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
EVT_MENU(wxID_EXIT, MyFrame::OnExit)
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

	d3d9ex = d3d9::createEx();
	_Timer = new utils::HiPerfTimer();

	MyFrame *frame = new MyFrame("Edge blender", wxPoint(0, 0), wxSize(650, 440));
	frame->Show(true);

	return true;
}

int MyApp::OnExit()
{
	delete _Timer;
	_Timer = NULL;

	d3d9ex = NULL;

	return wx::App::OnExit();
}