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
#include <boost/lockfree/spsc_queue.hpp>

#include "eb4.h"

#include <wx/listctrl.h>
#include <wx/grid.h>

using namespace boost;
using namespace zzlab;
using namespace google;

ZZLAB_USE_LOG4CPLUS(wxTest0);

d3d9::IDirect3D9ExPtr d3d9ex;
utils::Timer* _Timer = NULL;
av::AudioDevice* _AudioDevice = NULL;

template<class T>
void deleteAndClear(T& t)
{
	if (t)
	{
		delete t;
		t = NULL;
	}
}

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
	ID_ShowEB4Control,
	ID_SaveEB4Settings,

	ID_Schedule = 0x1000,
	ID_BlendingParams,
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

static void getROI_YUV422P(AVFrame* src, AVFrame* dst, const cv::Rect& roi)
{
	dst->width = roi.width;
	dst->height = roi.height;
	dst->data[0] = src->data[0] + roi.x + roi.y * src->linesize[0];
	dst->data[1] = src->data[1] + roi.x / 2 + roi.y * src->linesize[1];
	dst->data[2] = src->data[2] + roi.x / 2 + roi.y * src->linesize[2];
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
	std::string A_fadeOut;
	std::string A_clear;
	std::string B;
	std::string C;
	std::string BC_ending;
	std::string BC_fadeOut;
	std::string BC_clear;

	void load(XmlNode* node)
	{
		A = node->first_attribute("A")->value();
		A_ending = node->first_attribute("A-ending")->value();
		A_fadeOut = node->first_attribute("A-fade-out")->value();
		XmlAttribute* attr = node->first_attribute("A-clear");
		A_clear = attr ? attr->value() : "1";
		B = node->first_attribute("B")->value();
		C = node->first_attribute("C")->value();
		BC_ending = node->first_attribute("BC-ending")->value();
		BC_fadeOut = node->first_attribute("BC-fade-out")->value();
		attr = node->first_attribute("BC-clear");
		BC_clear = attr ? attr->value() : "1";
	}
};

struct Schedule : public std::vector<Cut>
{
	float fadeOutLength;

	void load(XmlNode* node)
	{
		fadeOutLength = atof(node->first_attribute("fade-out-length")->value());

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
	gfx::RendererEvents* rendererEvents;
	d3d9::MeshResource* mesh;
	d3d9::EffectResource* effect;
	d3d9::TextureResource* texture;
	wxSize windowSize;
	gfx::Camera camera;
	Eigen::Affine3f t2;
	Eigen::Affine3f t1;
	Eigen::Affine3f t0;
	D3DXCOLOR diffuse;

	explicit VideoQuadRenderer() : rendererEvents(NULL), mesh(NULL), effect(NULL), texture(NULL), diffuse(1, 1, 1, 1)
	{
		ZZLAB_TRACE_THIS();

		windowSize = wxSize(1024, 768);
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
		camera.proj = gfx::orthoLH(windowSize.x, windowSize.y, 1.0f, 100.0f);
		camera.updateT(
			gfx::lookAtLH(
			Eigen::Vector3f(0, 0, -10),
			Eigen::Vector3f(0.0f, 0.0f, 0.0f),
			Eigen::Vector3f::UnitY()));

		t2 = Eigen::Affine3f::Identity() *
			Eigen::Translation3f(-windowSize.x / 2.0f, windowSize.y / 2.0f, 0.0f);

		//t1 = Eigen::Affine3f::Identity() *
		//	Eigen::Translation3f(100.0f, -100.0f, 0.0f) *
		//	Eigen::Scaling(100.0f, 100.0f, 1.0f);

		t0 = Eigen::Affine3f::Identity() *
			Eigen::Translation3f(0.5f, -0.5f, 0.0f);

		mDrawDelegate.connect(bind(&VideoQuadRenderer::draw, this));

		rendererEvents->waitForDraw(mDrawDelegate());
	}

protected:
	utils::SharedEvent0 mDrawDelegate;

	d3d9::QuadMeshResource* mQuad;
	d3d9::EffectResource* mEffect;

	void draw()
	{
		//ZZLAB_TRACE_THIS();

		Eigen::Projective3f MATRIX_MVP = camera.matrix() * t2 * t1 * t0;
		effect->effect->SetMatrix("MATRIX_MVP", (const D3DXMATRIX *)MATRIX_MVP.matrix().data());
		if (texture)
		{
			effect->effect->SetTexture("MainTex_Y", texture->textures[0]);
			effect->effect->SetTexture("MainTex_U", texture->textures[1]);
			effect->effect->SetTexture("MainTex_V", texture->textures[2]);
		}
		effect->effect->SetFloatArray("Diffuse", diffuse, sizeof(diffuse));

		mesh->draw(effect->effect);

		rendererEvents->waitForDraw(mDrawDelegate());
	}
};

static void dummy() {}

class eb4Controller
{
public:
	d3d9::Device* device;

	int width;
	int height;
	D3DXVECTOR2 uvScale;
	D3DXVECTOR2 uvOffset;

	XmlNode* node;
	filesystem::wpath path;
	eb::Workspace workspace;

	d3d9::TextureResource* mainTex;

	d3d9::DynamicTextureResource* mGridTex;

	d3d9::eb4Renderer* mRenderer;
	d3d9::DynamicTextureResource* mHorTex;
	d3d9::DynamicTextureResource* mVerTex;
	d3d9::LatticeMeshResource* mLattice;

	cv::Point2f mAdjustments;
	std::vector<Vertex2D> mControlPoints;
	cv::Size mControlSize;
	cv::Size mSubdivSize;
	Vertex2DsPtr mVertices;

	d3d9::EffectResource* mBlendEffect;
	d3d9::EffectResource* mGridEffect;

	cv::Mat4b mCanvas;
	int mGridWidth;
	int mGridHeight;

	eb4Controller()
	{
	}

	~eb4Controller()
	{
	}

	void save()
	{
		protobuf::RepeatedPtrField<eb::ControlPoint> *cps =
			workspace.mutable_lattice_settings()->mutable_control_points();

		cps->Clear();

		for (int y = 0; y < mControlSize.height; ++y)
		{
			for (int x = 0; x < mControlSize.width; ++x)
			{
				size_t idx = y * mControlSize.width + x;
				const Vertex2D &pt0 = mControlPoints[y * mControlSize.width + x];

				eb::ControlPoint *cp = cps->Add();
				cp->set_x(pt0.x);
				cp->set_y(pt0.y);
				cp->set_type(pt0.flags == VertexTraits::none ? eb::CP_FREE : eb::CP_CEASE);
			}
		}

		std::fstream output(path.string().c_str(), std::ios::out | std::ios::trunc | std::ios::binary);
		workspace.SerializeToOstream(&output);
	}

	void updateCanvas()
	{
		mCanvas.setTo(cv::Scalar(0, 0, 0, 0));

		for (size_t j = 0; j < mGridWidth; ++j)
		{
			cv::line(mCanvas, cv::Point(float(j * width / (mGridWidth - 1)), 0), cv::Point(float(j * width / (mGridWidth - 1)), height),
				cv::Scalar(255, 255, 255, 255));
		}

		for (size_t j = 0; j < mGridHeight; ++j)
		{
			cv::line(mCanvas, cv::Point(0, float(j * height / (mGridHeight - 1))), cv::Point(width, float(j * height / (mGridHeight - 1))),
				cv::Scalar(255, 255, 255, 255));
		}

		cv::Point2i selected(workspace.editor_settings().selected().x(),
			mControlSize.height - 1 - workspace.editor_settings().selected().y());

		cv::circle(mCanvas, cv::Point(
			(float)selected.x * mCanvas.cols / (mControlSize.width - 1),
			(float)selected.y * mCanvas.rows / (mControlSize.height - 1)),
			5, cv::Scalar(255, 255, 255, 255), 2);

		if (workspace.editor_settings().select_mode() & eb::SM_ROW)
		{
			for (int i = 0; i < mControlSize.width; ++i)
			{
				cv::circle(mCanvas, cv::Point(
					(float)i * mCanvas.cols / (mControlSize.width - 1),
					(float)selected.y * mCanvas.rows / (mControlSize.height - 1)),
					5, cv::Scalar(255, 255, 255, 255), 2);
			}
		}

		if (workspace.editor_settings().select_mode() & eb::SM_COLUMN)
		{
			for (int i = 0; i < mControlSize.height; ++i)
			{
				cv::circle(mCanvas, cv::Point(
					(float)selected.x * mCanvas.cols / (mControlSize.width - 1),
					(float)i * mCanvas.rows / (mControlSize.height - 1)),
					5, cv::Scalar(255, 255, 255, 255), 2);
			}
		}

		mGridTex->update(mCanvas);
	}

	void updateHorEdge()
	{
		genHorEdge(workspace, mHorTex);
	}

	void updateVerEdge()
	{
		genVerEdge(workspace, mVerTex);
	}

	void init()
	{
		std::ifstream input(path.string().c_str(), std::ios::in | std::ios::binary);
		if (input)
		{
			ZZLAB_INFO("Loading " << path.wstring() << " ...");
			workspace.ParseFromIstream(&input);
			loadEdgeParameters(workspace, node);
		}
		else
		{
			ZZLAB_INFO("Use default workspace for " << path.wstring());
			setDefaultWorkspace(workspace, node);
		}

		{
			XmlNode* x = node->first_node("grid-image");
			mGridTex = new d3d9::DynamicTextureResource();
			mGridTex->dev = device->dev;
			mGridTex->deviceResourceEvents = &device->deviceResourceEvents;
			mGridTex->width = width;
			mGridTex->height = height;
			mGridTex->format = D3DFMT_X8R8G8B8;
			mGridTex->init();

			mGridWidth = atoi(x->first_attribute("width")->value());
			mGridHeight = atoi(x->first_attribute("height")->value());
			mCanvas = cv::Mat4b(mGridTex->height, mGridTex->width);

			updateCanvas();
		}

		mHorTex = new d3d9::DynamicTextureResource();
		mHorTex->dev = device->dev;
		mHorTex->deviceResourceEvents = &device->deviceResourceEvents;
		mHorTex->width = width;
		mHorTex->height = 1;
		mHorTex->format = D3DFMT_X8R8G8B8;
		mHorTex->init();

		mVerTex = new d3d9::DynamicTextureResource();
		mVerTex->dev = device->dev;
		mVerTex->deviceResourceEvents = &device->deviceResourceEvents;
		mVerTex->width = height;
		mVerTex->height = 1;
		mVerTex->format = D3DFMT_X8R8G8B8;
		mVerTex->init();

		mAdjustments.x = 0.5f / width;
		mAdjustments.y = 0.5f / height;

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
		updateHorEdge();
		updateVerEdge();

		mLattice = new d3d9::LatticeMeshResource();
		mLattice->dev = device->dev;
		mLattice->deviceResourceEvents = &device->deviceResourceEvents;

		// subdivision
		{
			mVertices = Subdivision_t::doSubdivision(mControlSize, mControlPoints,
				workspace.lattice_settings().subdiv_level(), mSubdivSize);
			const Vertex2Ds &vertices = *mVertices;

			mLattice->width = mSubdivSize.width;
			mLattice->height = mSubdivSize.height;
			mLattice->allocVertices();

			// initialize lattice
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

		mBlendEffect = device->resources->get<d3d9::EffectResource>("eb4.fx");
		mGridEffect = device->resources->get<d3d9::EffectResource>("eb4_grid.fx");

		mRenderer = new d3d9::eb4Renderer();
		mRenderer->rendererEvents = &device->rendererEvents;
		mRenderer->effect = mBlendEffect;
		mRenderer->mainTex = mainTex;
		mRenderer->verTex = mVerTex;
		mRenderer->horTex = mHorTex;
		mRenderer->lattice = mLattice;
		mRenderer->init();
	}

	void updateLattice()
	{
		// subdivision
		mVertices = Subdivision_t::doSubdivision(mControlSize, mControlPoints,
			workspace.lattice_settings().subdiv_level(), mSubdivSize);
		const Vertex2Ds &vertices = *mVertices;

		// update lattice
		double fWidth1 = mSubdivSize.width - 1;
		double fHeight1 = mSubdivSize.height - 1;
		for (int y = 0; y < mSubdivSize.height; ++y)
		{
			for (int x = 0; x < mSubdivSize.width; ++x)
			{
				size_t o = x + y * mSubdivSize.width;

				mLattice->vertices[o].POSITION = D3DXVECTOR3(vertices[o].x, vertices[o].y, 0.0f);
			}
		}

		mLattice->updateVertices();
	}

	void moveSelection(float x, float y)
	{
		cv::Point2i selected(workspace.editor_settings().selected().x(),
			workspace.editor_settings().selected().y());

		if (workspace.editor_settings().select_mode() == eb::SM_SINGLE)
		{
			size_t idx = selected.y * mControlSize.width + selected.x;
			Vertex2D &cp = mControlPoints[idx];
			cp.x += x;
			cp.y += y;
		}
		else
		{
			if (workspace.editor_settings().select_mode() & eb::SM_ROW)
			{
				for (int i = 0; i < mControlSize.width; ++i)
				{
					size_t idx = selected.y * mControlSize.width + i;
					Vertex2D &cp = mControlPoints[idx];
					cp.x += x;
					cp.y += y;
				}
			}
			if (workspace.editor_settings().select_mode() & eb::SM_COLUMN)
			{
				for (int i = 0; i < mControlSize.height; ++i)
				{
					size_t idx = i * mControlSize.width + selected.x;
					Vertex2D &cp = mControlPoints[idx];
					cp.x += x;
					cp.y += y;
				}
			}

			if (workspace.editor_settings().select_mode() == eb::SM_CROSS)
			{
				size_t idx = selected.y * mControlSize.width + selected.x;
				Vertex2D &cp = mControlPoints[idx];
				cp.x -= x;
				cp.y -= y;
			}
		}

		// edit post process
		switch (workspace.editor_settings().edit_mode())
		{
		case eb::EM_FREE:
			break;

		case eb::EM_CORNER_AVERAGE:
			doAverageCorner();
			break;

		case eb::EM_ROW_AVERAGE:
			switch (workspace.editor_settings().select_mode())
			{
			case eb::SM_SINGLE:
				doAverageRow(selected.y);
				break;

			case eb::SM_COLUMN:
			case eb::SM_CROSS:
				for (int y = 0; y < mControlSize.height; ++y)
					doAverageRow(y);
				break;

			default:
				break;
			}
			break;

		case eb::EM_COLUMN_AVERAGE:
			switch (workspace.editor_settings().select_mode())
			{
			case eb::SM_SINGLE:
				doAverageColumn(selected.x);
				break;

			case eb::SM_ROW:
			case eb::SM_CROSS:
				for (int x = 0; x < mControlSize.width; ++x)
					doAverageColumn(x);
				break;

			default:
				break;
			}
			break;

		case eb::EM_EDGE_AVERAGE:
			doAverageEdge();
			break;
		}
	}

	void processKeyDown(wxKeyEvent& event)
	{
		int code = event.GetKeyCode();

		//ZZLAB_TRACE_VALUE(code);
		switch (code)
		{
		case 'G':
			if (mRenderer->effect == mGridEffect)
			{
				{
					// update lattice texture coord
					double fWidth1 = mSubdivSize.width - 1;
					double fHeight1 = mSubdivSize.height - 1;
					for (int y = 0; y < mSubdivSize.height; ++y)
					{
						for (int x = 0; x < mSubdivSize.width; ++x)
						{
							size_t o = x + y * mSubdivSize.width;

							mLattice->vertices[o].UV0.x = uvOffset.x + (x / fWidth1) * uvScale.x;
							mLattice->vertices[o].UV0.y = uvOffset.y + (1 - y / fHeight1) * uvScale.y;
						}
					}

					mLattice->updateVertices();
				}

				mRenderer->effect = mBlendEffect;
				mRenderer->mainTex = mainTex;
			}
			else
			{
				{
					// update lattice texture coord
					double fWidth1 = mSubdivSize.width - 1;
					double fHeight1 = mSubdivSize.height - 1;
					for (int y = 0; y < mSubdivSize.height; ++y)
					{
						for (int x = 0; x < mSubdivSize.width; ++x)
						{
							size_t o = x + y * mSubdivSize.width;

							mLattice->vertices[o].UV0.x = (x / fWidth1);
							mLattice->vertices[o].UV0.y = (1 - y / fHeight1);
						}
					}

					mLattice->updateVertices();
				}

				mRenderer->effect = mGridEffect;
				mRenderer->mainTex = mGridTex;
			}
			break;

		case WXK_LEFT:
			if (event.ControlDown())
			{
				workspace.mutable_editor_settings()->set_select_mode(
					(eb::SelectMode)(workspace.editor_settings().select_mode() ^ eb::SM_ROW));
			}
			else
			{
				if (workspace.editor_settings().selected().x() > 0)
				{
					workspace.mutable_editor_settings()->mutable_selected()->set_x(
						workspace.editor_settings().selected().x() - 1);
				}
			}

			updateCanvas();
			break;

		case WXK_RIGHT:
			if (event.ControlDown())
			{
				workspace.mutable_editor_settings()->set_select_mode(
					(eb::SelectMode)(workspace.editor_settings().select_mode() ^ eb::SM_ROW));
			}
			else
			{
				if (workspace.editor_settings().selected().x() < workspace.lattice_settings().size().width() - 1)
				{
					workspace.mutable_editor_settings()->mutable_selected()->set_x(
						workspace.editor_settings().selected().x() + 1);
				}
			}

			updateCanvas();
			break;

		case WXK_DOWN:
			if (event.ControlDown())
			{
				workspace.mutable_editor_settings()->set_select_mode(
					(eb::SelectMode)(workspace.editor_settings().select_mode() ^ eb::SM_COLUMN));
			}
			else
			{
				if (workspace.editor_settings().selected().y() > 0)
				{
					workspace.mutable_editor_settings()->mutable_selected()->set_y(
						workspace.editor_settings().selected().y() - 1);
				}
			}

			updateCanvas();
			break;

		case WXK_UP:
			if (event.ControlDown())
			{
				workspace.mutable_editor_settings()->set_select_mode(
					(eb::SelectMode)(workspace.editor_settings().select_mode() ^ eb::SM_COLUMN));
			}
			else
			{
				if (workspace.editor_settings().selected().y() < workspace.lattice_settings().size().height() - 1)
				{
					workspace.mutable_editor_settings()->mutable_selected()->set_y(
						workspace.editor_settings().selected().y() + 1);
				}
			}

			updateCanvas();
			break;

		case 'W':
			moveSelection(0, mAdjustments.y * workspace.editor_settings().adjust_speed());
			updateLattice();
			break;

		case 'S':
			moveSelection(0, -mAdjustments.y * workspace.editor_settings().adjust_speed());
			updateLattice();
			break;

		case 'A':
			moveSelection(-mAdjustments.x * workspace.editor_settings().adjust_speed(), 0);
			updateLattice();
			break;

		case 'D':
			moveSelection(mAdjustments.x * workspace.editor_settings().adjust_speed(), 0);
			updateLattice();
			break;

		case '-':
		{
			float speed = workspace.editor_settings().adjust_speed() - 1;
			if (speed > 100)
				speed = 100;

			ZZLAB_TRACE_VALUE(speed);
			workspace.mutable_editor_settings()->set_adjust_speed(speed);
		}
			break;

		case '=':
		{
			float speed = workspace.editor_settings().adjust_speed() + 1;
			if (speed < 1)
				speed = 1;

			ZZLAB_TRACE_VALUE(speed);
			workspace.mutable_editor_settings()->set_adjust_speed(speed);
		}
			break;

		case WXK_F5:
			workspace.mutable_editor_settings()->set_edit_mode(eb::EM_FREE);
			ZZLAB_TRACE("EM_FREE");
			break;

		case WXK_F6:
			workspace.mutable_editor_settings()->set_edit_mode(eb::EM_CORNER_AVERAGE);
			ZZLAB_TRACE("EM_CORNER_AVERAGE");
			break;

		case WXK_F7:
			workspace.mutable_editor_settings()->set_edit_mode(eb::EM_ROW_AVERAGE);
			ZZLAB_TRACE("EM_ROW_AVERAGE");
			break;

		case WXK_F8:
			workspace.mutable_editor_settings()->set_edit_mode(eb::EM_COLUMN_AVERAGE);
			ZZLAB_TRACE("EM_COLUMN_AVERAGE");
			break;

		case WXK_F9:
			workspace.mutable_editor_settings()->set_edit_mode(eb::EM_EDGE_AVERAGE);
			ZZLAB_TRACE("EM_EDGE_AVERAGE");
			break;
		}
	}

	void processKeyUp(wxKeyEvent& event)
	{
		//ZZLAB_TRACE("KeyUp: " << event.GetKeyCode());
	}

	void doAverageRow(size_t y)
	{
		size_t base = y * mControlSize.width;
		float begin = mControlPoints[base].x;
		float dist = mControlPoints[mControlSize.width - 1 + base].x - begin;
		float cols1 = mControlSize.width - 1;

		for (int x = 0; x < mControlSize.width; ++x)
		{
			mControlPoints[x + base].x = begin + (dist * x) / cols1;
		}
	}

	void doAverageColumn(size_t x)
	{
		float begin = mControlPoints[x].y;
		float dist = mControlPoints[x
			+ (mControlSize.height - 1) * mControlSize.width].y - begin;
		float rows1 = mControlSize.height - 1;

		for (int y = 0; y < mControlSize.height; ++y)
		{
			mControlPoints[x + y * mControlSize.width].y = begin
				+ (dist * y) / rows1;
		}
	}

	void doAverageEdge()
	{
		for (int x = 1; x < mControlSize.width - 1; ++x)
			doAverageColumn(x);
		for (int y = 1; y < mControlSize.height - 1; ++y)
			doAverageRow(y);
	}

	void doAverageCorner()
	{
		float cols1 = mControlSize.width - 1;
		float rows1 = mControlSize.height - 1;

		// left column
		{
			const Vertex2D &start = mControlPoints[0];
			const Vertex2D &end = mControlPoints[(mControlSize.height - 1)
				* mControlSize.width];
			Vertex2D dist(end.x - start.x, end.y - start.y);

			for (size_t y = 1; y < (size_t)mControlSize.height; ++y)
			{
				mControlPoints[y * mControlSize.width].x = start.x
					+ (dist.x * y) / rows1;
				mControlPoints[y * mControlSize.width].y = start.y
					+ (dist.y * y) / rows1;
			}
		}

		// right column
		{
			size_t base = mControlSize.width - 1;
			const Vertex2D &start = mControlPoints[base];
			const Vertex2D &end = mControlPoints[base
				+ (mControlSize.height - 1) * mControlSize.width];
			Vertex2D dist(end.x - start.x, end.y - start.y);

			for (int y = 1; y < mControlSize.height; ++y)
			{
				mControlPoints[base + y * mControlSize.width].x = start.x
					+ (dist.x * y) / rows1;
				mControlPoints[base + y * mControlSize.width].y = start.y
					+ (dist.y * y) / rows1;
			}
		}

		// top row
		{
			const Vertex2D &start = mControlPoints[0];
			const Vertex2D &end = mControlPoints[mControlSize.width - 1];
			Vertex2D dist(end.x - start.x, end.y - start.y);

			for (int x = 1; x < mControlSize.width; ++x)
			{
				mControlPoints[x].x = start.x + (dist.x * x) / cols1;
				mControlPoints[x].y = start.y + (dist.y * x) / cols1;
			}
		}

		// bottom row
		{
			size_t base = (mControlSize.height - 1) * mControlSize.width;
			const Vertex2D &start = mControlPoints[base];
			const Vertex2D &end = mControlPoints[base + mControlSize.width - 1];
			Vertex2D dist(end.x - start.x, end.y - start.y);

			for (int x = 1; x < mControlSize.width; ++x)
			{
				mControlPoints[base + x].x = start.x + (dist.x * x) / cols1;
				mControlPoints[base + x].y = start.y + (dist.y * x) / cols1;
			}
		}

		doAverageEdge();
	}
};

class eb4ControlPanel : public wxFrame
{
public:
	boost::array<eb4Controller, 7> mControllers;
	eb4Controller* dst;

	class BlendingParams : public wxFrame
	{
	public:
		wxGrid* mParamsGrid;

		BlendingParams(wxWindow* parent, const wxString& title, const wxPoint& pos, const wxSize& size) :
			wxFrame(parent, wxID_ANY, title, pos, size, wxRESIZE_BORDER | wxCAPTION | wxCLIP_CHILDREN)
		{
			mParamsGrid = new wxGrid(this, ID_BlendingParams, wxDefaultPosition, wxDefaultSize);

			mParamsGrid->CreateGrid(4, 4);
			mParamsGrid->SetColLabelValue(0, L"Width");
			mParamsGrid->SetColLabelValue(1, L"Blending");
			mParamsGrid->SetColLabelValue(2, L"Gamma");
			mParamsGrid->SetColLabelValue(3, L"Center");

			mParamsGrid->SetRowLabelValue(0, L"上");
			mParamsGrid->SetRowLabelValue(1, L"下");
			mParamsGrid->SetRowLabelValue(2, L"左");
			mParamsGrid->SetRowLabelValue(3, L"右");

			mParamsGrid->SetColFormatFloat(0, -1, 2);
			mParamsGrid->SetColFormatFloat(1, -1, 2);
			mParamsGrid->SetColFormatFloat(2, -1, 2);
			mParamsGrid->SetColFormatFloat(3, -1, 2);

			mParamsGrid->SetReadOnly(0, 0);
			mParamsGrid->SetReadOnly(1, 0);
			mParamsGrid->SetReadOnly(2, 0);
			mParamsGrid->SetReadOnly(3, 0);
		}
	};

	BlendingParams* mBlendingParams;

	eb4ControlPanel(wxWindow* parent, const wxString& title, const wxPoint& pos, const wxSize& size) :
		wxFrame(parent, wxID_ANY, title, pos, size)
	{
		dst = &mControllers[0];

		mBlendingParams = new BlendingParams(this, "Blending Parameters", wxPoint(pos.x, pos.y + size.y), wxSize(450, 150));
		mBlendingParams->mParamsGrid->Connect(wxEVT_GRID_CELL_CHANGED,
			wxGridEventHandler(eb4ControlPanel::OnGridCellChanged), NULL, this);

		mBlendingParams->Refresh();
	}

	~eb4ControlPanel()
	{
	}

	virtual bool Show(bool show)
	{
		mBlendingParams->Show(show);
		return wxFrame::Show(show);
	}

	void saveAll()
	{
		for (size_t i = 0; i < mControllers.size(); ++i)
		{
			mControllers[i].save();
		}
	}

	void OnClose(wxCloseEvent& event)
	{
		Show(false);
	}

	void OnSetFocus(wxFocusEvent& event)
	{
		SetBackgroundColour(*wxRED);
		ClearBackground();
	}

	void OnKillFocus(wxFocusEvent& event)
	{
		SetBackgroundColour(*wxBLACK);
		ClearBackground();
	}

	void OnKeyDown(wxKeyEvent& event)
	{
		dst->processKeyDown(event);
	}

	void OnKeyUp(wxKeyEvent& event)
	{
		dst->processKeyUp(event);

		switch (event.GetKeyCode())
		{
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
			dst = &mControllers[event.GetKeyCode() - '1'];

			{
				const eb::EdgeBlendSettings& settings = dst->workspace.top_edge_settings();
				mBlendingParams->mParamsGrid->SetCellValue(0, 0, (format("%d") % settings.width()).str());
				mBlendingParams->mParamsGrid->SetCellValue(0, 1, (format("%.2f") % settings.blending()).str());
				mBlendingParams->mParamsGrid->SetCellValue(0, 2, (format("%.2f") % settings.gamma()).str());
				mBlendingParams->mParamsGrid->SetCellValue(0, 3, (format("%.2f") % settings.center()).str());
			}

			{
				const eb::EdgeBlendSettings& settings = dst->workspace.bottom_edge_settings();
				mBlendingParams->mParamsGrid->SetCellValue(1, 0, (format("%d") % settings.width()).str());
				mBlendingParams->mParamsGrid->SetCellValue(1, 1, (format("%.2f") % settings.blending()).str());
				mBlendingParams->mParamsGrid->SetCellValue(1, 2, (format("%.2f") % settings.gamma()).str());
				mBlendingParams->mParamsGrid->SetCellValue(1, 3, (format("%.2f") % settings.center()).str());
			}

			{
				const eb::EdgeBlendSettings& settings = dst->workspace.left_edge_settings();
				mBlendingParams->mParamsGrid->SetCellValue(2, 0, (format("%d") % settings.width()).str());
				mBlendingParams->mParamsGrid->SetCellValue(2, 1, (format("%.2f") % settings.blending()).str());
				mBlendingParams->mParamsGrid->SetCellValue(2, 2, (format("%.2f") % settings.gamma()).str());
				mBlendingParams->mParamsGrid->SetCellValue(2, 3, (format("%.2f") % settings.center()).str());
			}

			{
				const eb::EdgeBlendSettings& settings = dst->workspace.right_edge_settings();
				mBlendingParams->mParamsGrid->SetCellValue(3, 0, (format("%d") % settings.width()).str());
				mBlendingParams->mParamsGrid->SetCellValue(3, 1, (format("%.2f") % settings.blending()).str());
				mBlendingParams->mParamsGrid->SetCellValue(3, 2, (format("%.2f") % settings.gamma()).str());
				mBlendingParams->mParamsGrid->SetCellValue(3, 3, (format("%.2f") % settings.center()).str());
			}

			break;
		}
	}

	void OnGridCellChanged(wxGridEvent& event)
	{
		wxString val = mBlendingParams->mParamsGrid->GetCellValue(event.GetRow(), event.GetCol());

		switch (event.GetRow())
		{
		case 0:
		{
			eb::EdgeBlendSettings* settings = dst->workspace.mutable_top_edge_settings();

			switch (event.GetCol())
			{
			case 1:
				settings->set_blending(atof(val));
				break;

			case 2:
				settings->set_gamma(atof(val));
				break;

			case 3:
				settings->set_center(atof(val));
				break;
			}

			dst->updateVerEdge();
		}
			break;

		case 1:
		{
			eb::EdgeBlendSettings* settings = dst->workspace.mutable_bottom_edge_settings();

			switch (event.GetCol())
			{
			case 1:
				settings->set_blending(atof(val));
				break;

			case 2:
				settings->set_gamma(atof(val));
				break;

			case 3:
				settings->set_center(atof(val));
				break;
			}

			dst->updateVerEdge();
		}
			break;

		case 2:
		{
			eb::EdgeBlendSettings* settings = dst->workspace.mutable_left_edge_settings();

			switch (event.GetCol())
			{
			case 1:
				settings->set_blending(atof(val));
				break;

			case 2:
				settings->set_gamma(atof(val));
				break;

			case 3:
				settings->set_center(atof(val));
				break;
			}

			dst->updateHorEdge();
		}
			break;

		case 3:
		{
			eb::EdgeBlendSettings* settings = dst->workspace.mutable_right_edge_settings();

			switch (event.GetCol())
			{
			case 1:
				settings->set_blending(atof(val));
				break;

			case 2:
				settings->set_gamma(atof(val));
				break;

			case 3:
				settings->set_center(atof(val));
				break;
			}

			dst->updateHorEdge();
		}
			break;
		}
	}

	wxDECLARE_EVENT_TABLE();
};

wxBEGIN_EVENT_TABLE(eb4ControlPanel, wxFrame)
EVT_SET_FOCUS(eb4ControlPanel::OnSetFocus)
EVT_KILL_FOCUS(eb4ControlPanel::OnKillFocus)
EVT_KEY_DOWN(eb4ControlPanel::OnKeyDown)
EVT_KEY_UP(eb4ControlPanel::OnKeyUp)
EVT_CLOSE(eb4ControlPanel::OnClose)
wxEND_EVENT_TABLE()

class MyFrame : public wxFrame
{
public:
	MyFrame(const wxString& title, const wxPoint& pos, const wxSize& size) :
		wxFrame(NULL, wxID_ANY, title, pos, size), mVideoCapTimer(_MainService), mFadeOutTimer(_MainService)
	{
		wxMenu *menuFile = new wxMenu;
		menuFile->Append(ID_StopAll, "S&top all\tCtrl-X");
		menuFile->Append(ID_ShowEB4Control, "Show &EB4 Control Panel\tCtrl-E");
		menuFile->Append(ID_SaveEB4Settings, "&Save EB4 settings\tCtrl-S");
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
		scheList->AppendColumn(L"A淡出");
		scheList->AppendColumn(L"A填黑");
		scheList->AppendColumn(L"B");
		scheList->AppendColumn(L"C");
		scheList->AppendColumn(L"BC結束動作");
		scheList->AppendColumn(L"BC淡出");
		scheList->AppendColumn(L"BC填黑");

		for (size_t i = 0; i < _Schedule.size(); ++i)
		{
			const Cut& cut = _Schedule[i];
			scheList->InsertItem(i, (format("%d") % i).str());
			scheList->SetItem(i, 1, cut.A);
			scheList->SetItem(i, 2, cut.A_ending);
			scheList->SetItem(i, 3, cut.A_fadeOut);
			scheList->SetItem(i, 4, cut.A_clear);
			scheList->SetItem(i, 5, cut.B);
			scheList->SetItem(i, 6, cut.C);
			scheList->SetItem(i, 7, cut.BC_ending);
			scheList->SetItem(i, 8, cut.BC_fadeOut);
			scheList->SetItem(i, 9, cut.BC_clear);
		}

		mScheduleList = scheList;

		for (size_t i = 0; i < mDevices.size(); ++i)
		{
			tie(mDevices[i], mWindows[i]) = createDevice(i);

			mClearScenes[i] = new d3d9::ClearScene();
			mClearScenes[i]->dev = mDevices[i]->dev;
			mClearScenes[i]->rendererEvents = &mDevices[i]->rendererEvents;
			mClearScenes[i]->init();

			mRenderTextures[i] = new d3d9::RenderTexture();
			mRenderTextures[i]->width = mDevices[i]->d3dpp.BackBufferWidth;
			mRenderTextures[i]->height = mDevices[i]->d3dpp.BackBufferHeight;
			mRenderTextures[i]->format = D3DFMT_X8R8G8B8;
			mRenderTextures[i]->device = mDevices[i];
			mRenderTextures[i]->init();
		}

		std::fill(mMediaPlayers.begin(), mMediaPlayers.end(), nullptr);

		std::fill(mQuads.begin(), mQuads.end(), nullptr);
		std::fill(mVideoTextures.begin(), mVideoTextures.end(), nullptr);
		std::fill(mLiveVideoTextures.begin(), mLiveVideoTextures.end(), nullptr);

		for (size_t i = 0; i < mVideoROIs.size(); ++i)
		{
			mQuadROIs[i] = loadROI(_Settings.first_node(
				(boost::format("Quad%d") % i).str().c_str()));

			mVideoROIs[i] = loadROI(_Settings.first_node(
				(boost::format("VideoROI%d") % i).str().c_str()));

			mLiveROIs[i] = loadROI(_Settings.first_node(
				(boost::format("LiveROI%d") % i).str().c_str()));
		}

		initLiveQuad(1, 2);
		initLiveQuad(2, 3);
		initLiveQuad(3, 4);

		initVideoQuad(0, 0);
		initVideoQuad(0, 1);
		initVideoQuad(1, 2);
		initVideoQuad(2, 3);
		initVideoQuad(3, 4);
		initVideoQuad(4, 5);
		initVideoQuad(4, 6);

		mControlPanel = new eb4ControlPanel(this, "EB4 Control", wxPoint(pos.x + size.x, pos.y), wxSize(200, 200));
		initController(0, 0);
		initController(1, 0);
		initController(2, 1);
		initController(3, 2);
		initController(4, 3);
		initController(5, 4);
		initController(6, 4);

		mUpdateLiveTextures = false;
		initVideoCap();

		mState = State_Ready;
		mCurrentCut = NULL;
		mAfterStop = bind(dummy);
	}

	~MyFrame()
	{
		for (size_t i = 0; i < mQuads.size(); ++i)
			deleteAndClear(mQuads[i]);

		for (size_t i = 0; i < mLiveVideoTextures.size(); ++i)
			deleteAndClear(mLiveVideoTextures[i]);

		for (size_t i = 0; i < mVideoTextures.size(); ++i)
			deleteAndClear(mVideoTextures[i]);

		for (size_t i = 0; i < mDevices.size(); ++i)
		{
			delete mDevices[i];
			delete mClearScenes[i];
		}

		AVFrame* frame;
		while (mFreeQ.pop(frame))
			av_frame_free(&frame);

		while (mAllocQ.pop(frame))
			av_frame_free(&frame);
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
	boost::array<wxWindow*, 5> mWindows;
	boost::array<d3d9::ClearScene*, 5> mClearScenes;

	boost::array<d3d9::RenderTexture*, 5> mRenderTextures;

	boost::array<av::SimpleMediaPlayer*, 3> mMediaPlayers;

	boost::array<VideoQuadRenderer*, 7> mQuads;
	boost::array<d3d9::DynamicYUVTextureResource*, 7> mVideoTextures;
	boost::array<d3d9::DynamicYUVTextureResource*, 7> mLiveVideoTextures;
	boost::array<cv::Rect, 7> mQuadROIs;
	boost::array<cv::Rect, 7> mVideoROIs;
	boost::array<cv::Rect, 7> mLiveROIs;

	av::VideoCap mVideoCap;
	av::Scaler mScaler;

	lockfree::spsc_queue<AVFrame*, lockfree::capacity<5> > mFreeQ;
	lockfree::spsc_queue<AVFrame*, lockfree::capacity<5> > mAllocQ;
	asio::deadline_timer mVideoCapTimer;
	bool mUpdateLiveTextures;

	asio::deadline_timer mFadeOutTimer;
	double mFadeOutStart;

	eb4ControlPanel* mControlPanel;

	// used for terminating media players
	int mStopCount;

	void onFrame(double t, IMediaSample* ms)
	{
		//ZZLAB_TRACE_THIS();

		AVFrame* frame;
		if (!mFreeQ.pop(frame))
		{
			//ZZLAB_TRACE("FAILED to dequeue");
			return;
		}

		uint8_t* src;
		HR(ms->GetPointer(&src));
		int srcStride = mScaler.srcW * 2; // YUY2
		mScaler.scale(&src, &srcStride, frame->data, frame->linesize);

		if (!mAllocQ.push(frame))
		{
			//ZZLAB_TRACE("FAILED to enqueue");
			av_frame_free(&frame);
		}
	}

	void initController(size_t i, size_t device_index)
	{
		mControlPanel->mControllers[i].node = _Settings.first_node(
			(boost::format("EdgeBlend%d") % i).str().c_str());
		mControlPanel->mControllers[i].device = mDevices[device_index];
		mControlPanel->mControllers[i].mainTex = mRenderTextures[device_index];

		mControlPanel->mControllers[i].path = _DataPath / (boost::format("%d.eb4") % i).str().c_str();

		mControlPanel->mControllers[i].width = mQuadROIs[i].width;
		mControlPanel->mControllers[i].height = mQuadROIs[i].height;

		mControlPanel->mControllers[i].uvOffset = D3DXVECTOR2(
			float(mQuadROIs[i].x) / float(mDevices[device_index]->d3dpp.BackBufferWidth),
			float(mQuadROIs[i].y) / float(mDevices[device_index]->d3dpp.BackBufferHeight));
		mControlPanel->mControllers[i].uvScale = D3DXVECTOR2(
			float(mQuadROIs[i].width) / float(mDevices[device_index]->d3dpp.BackBufferWidth),
			float(mQuadROIs[i].height) / float(mDevices[device_index]->d3dpp.BackBufferHeight));

		mControlPanel->mControllers[i].init();
	}

	void initVideoCap()
	{
		av::monikers_t mks;
		av::enumerateDevices(CLSID_VideoInputDeviceCategory, mks);
		av::dumpMonikerFriendlyNames(mks);

		XmlNode* node = _Settings.first_node("VideoCap0");

		mVideoCap.onFrame = bind(&MyFrame::onFrame, this, _1, _2);

		XmlAttribute* attr = node->first_attribute("index");
		mVideoCap.init(mks[atoi(attr->value())]);

		attr = node->first_attribute("null-sync-source");
		if (_stricmp(attr->value(), "true") == 0)
			mVideoCap.setNullSyncSource();

		attr = node->first_attribute("width");
		int width = atoi(attr->value());

		attr = node->first_attribute("height");
		int height = atoi(attr->value());

		mVideoCap.setFormat(width, height, MEDIASUBTYPE_YUY2);
		mVideoCap.render();
		mVideoCap.dumpConnectedMediaType();

		mScaler.srcW = mScaler.dstW = width;
		mScaler.srcH = mScaler.dstH = height;
		mScaler.srcFormat = AV_PIX_FMT_YUYV422;
		mScaler.dstFormat = AV_PIX_FMT_YUV422P;
		mScaler.init();

		while (true)
		{
			AVFrame* frame = av_frame_alloc();
			frame->width = mScaler.dstW;
			frame->height = mScaler.dstH;
			frame->format = mScaler.dstFormat;
			av_frame_get_buffer(frame, 1);

			if (!mFreeQ.push(frame))
			{
				av_frame_free(&frame);
				break;
			}
		}

		mVideoCap.start();
		renderVideoCap();
	}

	boost::tuple<d3d9::Device*, wxWindow*> createDevice(size_t i)
	{
		XmlNode* node = _Settings.first_node(
			(boost::format("Window%d") % i).str().c_str());

		wxTopLevelWindow* wnd = wx::loadWindow<wxTopLevelWindow>(node, this);
		d3d9::Device* dev = new d3d9::Device();

		std::string devName = (boost::format("d3ddev%d") % i).str();
		node = _Settings.first_node(devName.c_str());
		if (!node) node = _Settings.first_node("d3ddev");

		dev->load(node, (HWND)wnd->GetHandle());
		dev->timeSource = _Timer;
		dev->init(d3d9ex, -1, (HWND)GetHandle());

		d3d9::loadAssets(dev, _DataPath / "assets.xml");

		wnd->Show();

		return make_tuple(dev, wnd);
	}

	void updateLiveTexture(AVFrame* frame, size_t idx)
	{
		AVFrame tmp;

		getROI_YUV422P(frame, &tmp, mLiveROIs[idx]);
		mLiveVideoTextures[idx]->update(&tmp);
	}

	void renderVideoCap(system::error_code err = system::error_code())
	{
		if (err)
			return;

		int ch = cv::waitKey(1);

		AVFrame* frame;
		if (mAllocQ.pop(frame))
		{
#if 0
			cv::Mat1b y(480, 640, frame->data[0], frame->linesize[0]);
			cv::imshow("y", y);

			cv::Mat1b u(480, 640 / 2, frame->data[1], frame->linesize[1]);
			cv::imshow("u", u);

			cv::Mat1b v(480, 640 / 2, frame->data[2], frame->linesize[2]);
			cv::imshow("v", v);
#endif
			if (mUpdateLiveTextures)
			{
				updateLiveTexture(frame, 2);
				updateLiveTexture(frame, 3);
				updateLiveTexture(frame, 4);
			}

			mFreeQ.push(frame);
		}

		mVideoCapTimer.expires_from_now(posix_time::milliseconds(4));
		mVideoCapTimer.async_wait(bind(&MyFrame::renderVideoCap, this, asio::placeholders::error));
	}

	void OnItemActivated(wxListEvent& event)
	{
		//ZZLAB_TRACE_THIS();

		const wxListItem& item = event.GetItem();
		RunItem(item.GetId());
	}

	void RunItem(size_t idx)
	{
		ZZLAB_TRACE("Item " << idx << " is about to play...");

		switch (mState)
		{
		case State_Ready:
			play(idx);
			break;

		case State_Playing:
			if (idx == mCurrentCut->id)
				ZZLAB_TRACE("Item " << idx << " is already playing.");
			else
			{
				//stopAll(bind(&MyFrame::play, this, item.GetId()));

				mAfterStop = bind(&MyFrame::play, this, idx);
				mState = State_Stopping;

				Cut* nextCut = &_Schedule[idx];
				mStopCount = 0;
				if (nextCut->A != mCurrentCut->A)
					stopMediaPlayer(1);

				if (nextCut->B != mCurrentCut->B)
					stopMediaPlayer(0);

				if (nextCut->C != mCurrentCut->C)
					stopMediaPlayer(2);

				if (mStopCount == 0)
				{
					mStopCount = 1;
					handleStop(mMediaPlayers.size());
				}
			}
			break;
		}
	}

	void OnScheduleKeyDown(wxListEvent& event)
	{
		int code = event.GetKeyCode();

		//ZZLAB_TRACE_VALUE(code);
		switch (code)
		{
		case WXK_PAGEUP:
		{
			if (mCurrentCut)
			{
				if (mCurrentCut->id > 0)
				{
					RunItem(mCurrentCut->id - 1);
				}
			}

			event.Veto();
		}
			break;

		case WXK_PAGEDOWN:
		{
			if (mCurrentCut)
			{
				if (mCurrentCut->id < _Schedule.size() - 1)
				{
					RunItem(mCurrentCut->id + 1);
				}
			}

			event.Veto();
		}
			break;
		}
	}

	void play(size_t idx)
	{
		for (size_t i = 0; i < mScheduleList->GetItemCount(); ++i)
			mScheduleList->SetItemBackgroundColour(i, wxColour(255, 255, 255));

		mScheduleList->SetItemBackgroundColour(idx, wxColour(255, 0, 0));
		mScheduleList->SetItemState(idx, 0, wxLIST_STATE_SELECTED);

		Cut* nextCut = &_Schedule[idx];		

		int64_t playTime = _Timer->getTime();
		if (!mCurrentCut || (_FileNameMapping[mCurrentCut->A].filename() == "LIVE") || (mCurrentCut->A != nextCut->A))
			playA(_FileNameMapping[nextCut->A], playTime, nextCut->A_ending == "loop");

		if (!mCurrentCut || mCurrentCut->B != nextCut->B)
			playB(_FileNameMapping[nextCut->B], playTime, nextCut->BC_ending == "loop");

		if (!mCurrentCut || mCurrentCut->C != nextCut->C)
			playC(_FileNameMapping[nextCut->C], playTime, nextCut->BC_ending == "loop");

		mCurrentCut = nextCut;
		ZZLAB_TRACE_VALUE(mCurrentCut->A_ending);
		ZZLAB_TRACE_VALUE(mCurrentCut->BC_ending);

		mState = State_Playing;
	}

	void endOfStream(size_t idx)
	{
		ZZLAB_TRACE("END OF STREAM, " << idx);
	}

	void handleStop(size_t idx)
	{
		if (idx < mMediaPlayers.size())
			deleteAndClear(mMediaPlayers[idx]);

		if (--mStopCount > 0)
			return;

		ZZLAB_INFO("All media players are stopped now.");

		if (mCurrentCut->A_fadeOut == "1" || mCurrentCut->BC_fadeOut == "1")
		{
			ZZLAB_TRACE("Fade out begins.");

			mFadeOutStart = _Timer->getTimeInSeconds();

			fadeOut();
		}
		else
			afterFadeOut();
	}

	void setFadeOutValue(float t)
	{
		D3DXCOLOR v(t, t, t, 1);

		if (mCurrentCut->A_fadeOut == "1")
		{
			mQuads[2]->diffuse = v;
			mQuads[3]->diffuse = v;
			mQuads[4]->diffuse = v;
		}

		if (mCurrentCut->BC_fadeOut == "1")
		{
			mQuads[0]->diffuse = v;
			mQuads[1]->diffuse = v;
			mQuads[5]->diffuse = v;
			mQuads[6]->diffuse = v;
		}
	}

	void fadeOut(boost::system::error_code err = boost::system::error_code())
	{
		if (err)
			return;

		double now = _Timer->getTimeInSeconds() - mFadeOutStart;
		if (now >= _Schedule.fadeOutLength)
		{
			setFadeOutValue(0);
			ZZLAB_TRACE("Fade out finished.");

			afterFadeOut();
		}
		else
		{
			float t = 1 - (float)(now / _Schedule.fadeOutLength);
			setFadeOutValue(t);

			mFadeOutTimer.expires_from_now(posix_time::milliseconds(8));
			mFadeOutTimer.async_wait(bind(&MyFrame::fadeOut, this, asio::placeholders::error));
		}
	}

	void afterFadeOut()
	{
		if (mCurrentCut->A_clear == "1")
		{
			mVideoTextures[2]->set(make_tuple(0, 128, 128));
			mVideoTextures[3]->set(make_tuple(0, 128, 128));
			mVideoTextures[4]->set(make_tuple(0, 128, 128));

			mLiveVideoTextures[2]->set(make_tuple(0, 128, 128));
			mLiveVideoTextures[3]->set(make_tuple(0, 128, 128));
			mLiveVideoTextures[4]->set(make_tuple(0, 128, 128));
		}

		if (mCurrentCut->BC_clear == "1")
		{
			mVideoTextures[0]->set(make_tuple(0, 128, 128));
			mVideoTextures[1]->set(make_tuple(0, 128, 128));
			mVideoTextures[5]->set(make_tuple(0, 128, 128));
			mVideoTextures[6]->set(make_tuple(0, 128, 128));
		}

		mUpdateLiveTextures = false;
		// keep current cut pointer
		//mCurrentCut = NULL;
		mState = State_Ready;
		mAfterStop();
		mAfterStop = bind(dummy);
	}

	void stopMediaPlayer(size_t idx)
	{
		if (mMediaPlayers[idx])
		{
			++mStopCount;
			mMediaPlayers[idx]->stop(_MainService.wrap(bind(&MyFrame::handleStop, this, idx)));
		}
	}

	function<void()> mAfterStop;
	void stopAll(function<void()> afterStop)
	{
		mAfterStop = afterStop;
		mState = State_Stopping;

		mStopCount = 0;
		stopMediaPlayer(0);
		stopMediaPlayer(1);
		stopMediaPlayer(2);
	}

	void setupVideoQuad(size_t device_index, size_t quad_index)
	{
		 mQuads[quad_index]->texture = mVideoTextures[quad_index];
		 mQuads[quad_index]->diffuse = D3DXCOLOR(1, 1, 1, 1);
	}

	void initVideoQuad(size_t device_index, size_t quad_index)
	{
		d3d9::Device* device = mDevices[device_index];
		d3d9::RenderTexture* rtt = mRenderTextures[device_index];
		const cv::Rect& roi = mVideoROIs[quad_index];

		d3d9::DynamicYUVTextureResource* texture = new d3d9::DynamicYUVTextureResource();
		texture->dev = device->dev;
		texture->width = roi.width;
		texture->height = roi.height;
		texture->uvWidth = roi.width / 2;
		texture->uvHeight = roi.height / 2;
		texture->deviceResourceEvents = &device->deviceResourceEvents;
		texture->init();
		texture->set(d3d9::ScalarYUV(0, 128, 128));

		VideoQuadRenderer* quad = new VideoQuadRenderer();	
		quad->rendererEvents = &rtt->rendererEvents;
		quad->mesh = device->resources->get<d3d9::QuadMeshResource>("Quad");
		//quad->windowSize = mWindows[device_index]->GetSize();
		quad->effect = device->resources->get<d3d9::EffectResource>("Unlit_YUV.fx");
		quad->texture = texture;
		quad->load(_Settings.first_node((format("Quad%d") % quad_index).str().c_str()));
		quad->init();

		mVideoTextures[quad_index] = texture;
		mQuads[quad_index] = quad;
	}

	void setupLiveQuad(size_t device_index, size_t quad_index)
	{
		mQuads[quad_index]->texture = mLiveVideoTextures[quad_index];
		mQuads[quad_index]->diffuse = D3DXCOLOR(1, 1, 1, 1);
	}

	void initLiveQuad(size_t device_index, size_t quad_index)
	{
		d3d9::Device* device = mDevices[device_index];
		d3d9::RenderTexture* rtt = mRenderTextures[device_index];
		const cv::Rect& roi = mLiveROIs[quad_index];

		d3d9::DynamicYUVTextureResource* texture = new d3d9::DynamicYUVTextureResource();
		texture->dev = device->dev;
		texture->width = roi.width;
		texture->height = roi.height;
		texture->uvWidth = roi.width / 2;
		texture->uvHeight = roi.height;
		texture->deviceResourceEvents = &device->deviceResourceEvents;
		texture->init();
		texture->set(d3d9::ScalarYUV(0, 128, 128));

		VideoQuadRenderer* quad = new VideoQuadRenderer();
		quad->rendererEvents = &rtt->rendererEvents;
		quad->mesh = device->resources->get<d3d9::QuadMeshResource>("Quad");
		quad->effect = device->resources->get<d3d9::EffectResource>("Unlit_YUV.fx");
		//quad->windowSize = mWindows[device_index]->GetSize();
		quad->texture = texture;
		quad->load(_Settings.first_node((format("Quad%d") % quad_index).str().c_str()));
		quad->init();

		mLiveVideoTextures[quad_index] = texture;
		mQuads[quad_index] = quad;
	}

	void initMediaPlayer(size_t mp_index, boost::filesystem::wpath path, int64_t playTime, bool loopPlay)
	{
		av::SimpleMediaPlayer* mp = new av::SimpleMediaPlayer();
		mp->source = path;
		mp->timeSource = _Timer;
		mp->audioDevice = _AudioDevice;
		mp->loopPlay = loopPlay;
		mp->load(_Settings.first_node("MediaPlayer0"));
		mp->init();

		mp->getVideoRenderer()->rendererEvents = &mDevices[0]->rendererEvents;
		switch (mp_index)
		{
		case 0:
			mp->getVideoRenderer()->renderFrame = bind(&MyFrame::renderFrame0, this, _1);
			break;

		case 1:
			mp->getVideoRenderer()->renderFrame = bind(&MyFrame::renderFrame1, this, _1);
			break;

		case 2:
			mp->getVideoRenderer()->renderFrame = bind(&MyFrame::renderFrame2, this, _1);
			break;
		}

		mp->play(playTime, _MainService.wrap(bind(&MyFrame::endOfStream, this, mp_index)));

		mMediaPlayers[mp_index] = mp;
	}

	void updateTexture(AVFrame* frame, size_t idx)
	{
		AVFrame tmp;

		getROI_YUV420P(frame, &tmp, mVideoROIs[idx]);
		mVideoTextures[idx]->update(&tmp);
	}

	void renderFrame0(AVFrame* frame)
	{
		updateTexture(frame, 0);
		updateTexture(frame, 1);
	}

	void renderFrame1(AVFrame* frame)
	{
		updateTexture(frame, 2);
		updateTexture(frame, 3);
		updateTexture(frame, 4);
	}

	void renderFrame2(AVFrame* frame)
	{
		updateTexture(frame, 5);
		updateTexture(frame, 6);
	}

	void playA(boost::filesystem::wpath path, int64_t playTime, bool loopPlay)
	{
		if (path.filename() == "LIVE")
		{
			setupLiveQuad(1, 2);
			setupLiveQuad(2, 3);
			setupLiveQuad(3, 4);

			// create live player
			mUpdateLiveTextures = true;
		}
		else
		{
			setupVideoQuad(1, 2);
			setupVideoQuad(2, 3);
			setupVideoQuad(3, 4);

			// create media player
			initMediaPlayer(1, path, playTime, loopPlay);
		}
	}

	void playB(boost::filesystem::wpath path, int64_t playTime, bool loopPlay)
	{
		setupVideoQuad(0, 0);
		setupVideoQuad(0, 1);

		// create media player
		initMediaPlayer(0, path, playTime, loopPlay);
	}

	void playC(boost::filesystem::wpath path, int64_t playTime, bool loopPlay)
	{
		setupVideoQuad(4, 5);
		setupVideoQuad(4, 6);

		// create media player
		initMediaPlayer(2, path, playTime, loopPlay);
	}

	void OnStopAll(wxCommandEvent& event)
	{
		stopAll(bind(&MyFrame::afterStopAll, this));
	}

	void afterStopAll()
	{
		mCurrentCut = NULL;
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
		mVideoCap.stop();
		utils::stopAllServices();

		Destroy();
	}

	void OnShowEB4Control(wxCommandEvent& event)
	{
		mControlPanel->Show(true);
	}

	void OnSaveEB4Settings(wxCommandEvent& event)
	{
		mControlPanel->saveAll();
		wxMessageBox("All EB4 settings are saved.", "Information", wxICON_INFORMATION);
	}

	wxDECLARE_EVENT_TABLE();
};

wxBEGIN_EVENT_TABLE(MyFrame, wxFrame)
EVT_MENU(ID_StopAll, MyFrame::OnStopAll)
EVT_MENU(ID_ShowEB4Control, MyFrame::OnShowEB4Control)
EVT_MENU(ID_SaveEB4Settings, MyFrame::OnSaveEB4Settings)
EVT_MENU(wxID_EXIT, MyFrame::OnExit)
EVT_CLOSE(MyFrame::OnClose)
EVT_LIST_ITEM_ACTIVATED(ID_Schedule, MyFrame::OnItemActivated)
EVT_LIST_KEY_DOWN(ID_Schedule, MyFrame::OnScheduleKeyDown)
wxEND_EVENT_TABLE()

wxIMPLEMENT_APP(MyApp);

bool MyApp::OnInit()
{
	// set log level
	_LogFlags = LOG_TO_WIN32DEBUG | LOG_TO_FILE;
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

	MyFrame *frame = new MyFrame("Show Control", wxPoint(0, 0), wxSize(700, 440));
	frame->Show(true);

	return true;
}

int MyApp::OnExit()
{
	deleteAndClear(_AudioDevice);
	deleteAndClear(_Timer);
	d3d9ex = NULL;

	return wx::App::OnExit();
}