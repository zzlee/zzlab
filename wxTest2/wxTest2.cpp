// wxTest2.cpp : 定義應用程式的進入點。
//

#include "stdafx.h"
#include "wxTest2.h"

#include <zzlab/gfx.h>

#include <d3d11.h>
#include <directxmath.h>
#include <comdef.h>

#include <boost/format.hpp>

#include <DirectXTK/CommonStates.h>
#include <DirectXTK/DDSTextureLoader.h>
#include <DirectXTK/Effects.h>
#include <DirectXTK/GeometricPrimitive.h>
#include <DirectXTK/Model.h>
#include <DirectXTK/PrimitiveBatch.h>
#include <DirectXTK/ScreenGrab.h>
#include <DirectXTK/SpriteBatch.h>
#include <DirectXTK/SpriteFont.h>
#include <DirectXTK/VertexTypes.h>
#include <algorithm>

using namespace boost;
using namespace zzlab;
using namespace DirectX;

ZZLAB_USE_LOG4CPLUS(wxTest2);

_COM_SMARTPTR_TYPEDEF(ID3D11Device, __uuidof(ID3D11Device));
_COM_SMARTPTR_TYPEDEF(ID3D11DeviceContext, __uuidof(ID3D11DeviceContext));
_COM_SMARTPTR_TYPEDEF(ID3D11Texture2D, __uuidof(ID3D11Texture2D));
_COM_SMARTPTR_TYPEDEF(IDXGISwapChain, __uuidof(IDXGISwapChain));
_COM_SMARTPTR_TYPEDEF(IDXGISwapChain1, __uuidof(IDXGISwapChain1));
_COM_SMARTPTR_TYPEDEF(ID3D11RenderTargetView, __uuidof(ID3D11RenderTargetView));
_COM_SMARTPTR_TYPEDEF(ID3D11DepthStencilView, __uuidof(ID3D11DepthStencilView));
_COM_SMARTPTR_TYPEDEF(ID3D11ShaderResourceView, __uuidof(ID3D11ShaderResourceView));
_COM_SMARTPTR_TYPEDEF(ID3D11InputLayout, __uuidof(ID3D11InputLayout));
_COM_SMARTPTR_TYPEDEF(IDXGIFactory, __uuidof(IDXGIFactory));
_COM_SMARTPTR_TYPEDEF(IDXGIFactory1, __uuidof(IDXGIFactory1));
_COM_SMARTPTR_TYPEDEF(IDXGIFactory2, __uuidof(IDXGIFactory2));
_COM_SMARTPTR_TYPEDEF(IDXGIAdapter, __uuidof(IDXGIAdapter));
_COM_SMARTPTR_TYPEDEF(IDXGIAdapter1, __uuidof(IDXGIAdapter1));
_COM_SMARTPTR_TYPEDEF(IDXGIOutput, __uuidof(IDXGIOutput));

static utils::HiPerfTimer sTimer;

class MyApp : public wx::App
{
public:
	virtual bool OnInit();
	virtual int OnExit();
};

enum
{
	ID_FULLSCREEN = 0x0010
};

namespace zzlab
{
	namespace d3d11
	{
		class RenderDevice : public gfx::ResourceManager
		{
		public:
			IDXGIAdapter1Ptr adapter;
			D3D_DRIVER_TYPE driverType;
			UINT flags;
			DXGI_SWAP_CHAIN_DESC swapChainDesc;
			UINT vsyncInterval;

			explicit RenderDevice();
			virtual ~RenderDevice();

			void load(XmlNode *node, HWND hWnd);
			void init();

			void cancel()
			{
				mMainDelegate.cancel();
			}

			// NOTICE: access ONLY after init is called
			D3D_FEATURE_LEVEL featureLevel;
			ID3D11DevicePtr dev;
			ID3D11DeviceContextPtr ctx;
			IDXGISwapChainPtr swapChain;
			gfx::RendererEvents rendererEvents;

		protected:
			boost::asio::deadline_timer mTimer;
			utils::Delegate<void(boost::system::error_code)> mMainDelegate;

			void main(boost::system::error_code error = boost::system::error_code());
		};
	}
}

namespace zzlab
{
	namespace d3d11
	{
		RenderDevice::RenderDevice()
			//: mTimer(*gfx::_RenderService)
			: mTimer(_MainService)
		{
			ZZLAB_TRACE_THIS();

			driverType = D3D_DRIVER_TYPE_HARDWARE;
			flags = 0;

			ZeroMemory(&swapChainDesc, sizeof(swapChainDesc));
			swapChainDesc.BufferCount = 1;
			swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
			swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
			swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			swapChainDesc.SampleDesc.Count = 1;
			swapChainDesc.SampleDesc.Quality = 0;
			swapChainDesc.Windowed = TRUE;
			swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

			vsyncInterval = 1;
		}

		RenderDevice::~RenderDevice()
		{
			ZZLAB_TRACE_THIS();
		}

		void RenderDevice::load(XmlNode *node, HWND hWnd)
		{
			ZZLAB_ERROR("NOT IMPLEMENTED YET");
		}

		void RenderDevice::init()
		{
			ZZLAB_TRACE_THIS();

			if (adapter == NULL)
			{
				HR(D3D11CreateDeviceAndSwapChain(nullptr, driverType, nullptr,
					flags, NULL, 0, D3D11_SDK_VERSION, &swapChainDesc, &swapChain, &dev, &featureLevel, &ctx));
			}
			else
			{
				HR(D3D11CreateDeviceAndSwapChain(adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr,
					flags, NULL, 0, D3D11_SDK_VERSION, &swapChainDesc, &swapChain, &dev, &featureLevel, &ctx));
			}

			mMainDelegate.connect(&RenderDevice::main, this, asio::placeholders::error);
			main();
		}

		void RenderDevice::main(boost::system::error_code error)
		{
			if (error)
				return;

			rendererEvents.frameBegin();
			rendererEvents.draw();
			rendererEvents.frameEnd();

			// flip back buffer 
			HR(swapChain->Present(vsyncInterval, 0));

			// enqueue next present event
			mTimer.expires_from_now(posix_time::microseconds(4));
			mTimer.async_wait(mMainDelegate());
		}
	}
}

class MyRenderWindow : public wxTopLevelWindow
{
public:
	d3d11::RenderDevice renderDevice;
	IDXGIOutputPtr output;

	MyRenderWindow(wxWindow *parent, wxWindowID id, const wxString &title, const wxPoint &pos = wxDefaultPosition, const wxSize &size = wxDefaultSize, long style = wxDEFAULT_FRAME_STYLE);
	~MyRenderWindow();

	void init();

	void setFullscreenState(bool fullScreen)
	{
		if (fullScreen)
			HR(renderDevice.swapChain->SetFullscreenState(TRUE, output));
		else
			HR(renderDevice.swapChain->SetFullscreenState(FALSE, nullptr));
	}

	bool Destroy();

protected:
	utils::Delegate0 mFrameBeginDelegate;
	utils::Delegate0 mDrawDelegate;

	utils::ManualTimer mTimer;

	ID3D11RenderTargetViewPtr mRenderTargetView;
	ID3D11Texture2DPtr mDepthStencil;
	ID3D11DepthStencilViewPtr mDepthStencilView;

	ID3D11ShaderResourceViewPtr mTextureRV1;
	ID3D11ShaderResourceViewPtr mTextureRV2;
	ID3D11InputLayoutPtr mBatchInputLayout;

	std::unique_ptr<CommonStates>                           mStates;
	std::unique_ptr<BasicEffect>                            mBatchEffect;
	std::unique_ptr<EffectFactory>                          mFXFactory;
	std::unique_ptr<GeometricPrimitive>                     mShape;
	std::unique_ptr<Model>                                  mModel;
	std::unique_ptr<PrimitiveBatch<VertexPositionColor>>    mBatch;
	std::unique_ptr<SpriteBatch>                            mSprites;
	std::unique_ptr<SpriteFont>                             mFont;

	XMMATRIX                            mWorld;
	XMMATRIX                            mView;
	XMMATRIX                            mProjection;

	void initRenderDevice();
	void DrawGrid(PrimitiveBatch<VertexPositionColor>& batch, FXMVECTOR xAxis, FXMVECTOR yAxis, FXMVECTOR origin, size_t xdivs, size_t ydivs, GXMVECTOR color);
	void frameBegin();
	void draw();
};

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
	//gfx::_RenderService->post(bind(&MyRenderWindow::initRenderDevice, this));
	_MainService.post(bind(&MyRenderWindow::initRenderDevice, this));
}

void MyRenderWindow::initRenderDevice()
{
	// setup render device
	renderDevice.swapChainDesc.BufferDesc.Width = GetSize().x;
	renderDevice.swapChainDesc.BufferDesc.Height = GetSize().y;
	renderDevice.swapChainDesc.OutputWindow = (HWND)GetHandle();
	renderDevice.init();

	// Create a render target view
	ID3D11Texture2DPtr pBackBuffer;
	HR(renderDevice.swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer));
	HR(renderDevice.dev->CreateRenderTargetView(pBackBuffer, nullptr, &mRenderTargetView));

	// Create depth stencil texture
	D3D11_TEXTURE2D_DESC descDepth;
	ZeroMemory(&descDepth, sizeof(descDepth));
	descDepth.Width = GetSize().x;
	descDepth.Height = GetSize().y;
	descDepth.MipLevels = 1;
	descDepth.ArraySize = 1;
	descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	descDepth.SampleDesc.Count = 1;
	descDepth.SampleDesc.Quality = 0;
	descDepth.Usage = D3D11_USAGE_DEFAULT;
	descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	descDepth.CPUAccessFlags = 0;
	descDepth.MiscFlags = 0;
	HR(renderDevice.dev->CreateTexture2D(&descDepth, nullptr, &mDepthStencil));

	// Create the depth stencil view
	D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
	ZeroMemory(&descDSV, sizeof(descDSV));
	descDSV.Format = descDepth.Format;
	descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	descDSV.Texture2D.MipSlice = 0;
	HR(renderDevice.dev->CreateDepthStencilView(mDepthStencil, &descDSV, &mDepthStencilView));

	ID3D11RenderTargetView* rtViews[] = { mRenderTargetView };
	renderDevice.ctx->OMSetRenderTargets(1, rtViews, mDepthStencilView);

	// Setup the viewport
	D3D11_VIEWPORT vp;
	vp.Width = (FLOAT)GetSize().x;
	vp.Height = (FLOAT)GetSize().y;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	renderDevice.ctx->RSSetViewports(1, &vp);

	// Create DirectXTK objects
	mStates.reset(new CommonStates(renderDevice.dev));
	mSprites.reset(new SpriteBatch(renderDevice.ctx));
	mFXFactory.reset(new EffectFactory(renderDevice.dev));
	mFXFactory->SetDirectory((_AssetsPath / "wxTest2").wstring().c_str());
	mBatch.reset(new PrimitiveBatch<VertexPositionColor>(renderDevice.ctx));

	mBatchEffect.reset(new BasicEffect(renderDevice.dev));
	mBatchEffect->SetVertexColorEnabled(true);

	{
		void const* shaderByteCode;
		size_t byteCodeLength;

		mBatchEffect->GetVertexShaderBytecode(&shaderByteCode, &byteCodeLength);

		HR(renderDevice.dev->CreateInputLayout(VertexPositionColor::InputElements,
			VertexPositionColor::InputElementCount,
			shaderByteCode, byteCodeLength,
			&mBatchInputLayout));
	}

	mFont.reset(new SpriteFont(renderDevice.dev, (_AssetsPath / L"wxTest2" / L"italic.spritefont").wstring().c_str()));
	mShape = GeometricPrimitive::CreateTeapot(renderDevice.ctx, 4.f, 8, false);
	mModel = Model::CreateFromSDKMESH(renderDevice.dev, (_AssetsPath / L"wxTest2" / L"tiny.sdkmesh").wstring().c_str(), *mFXFactory, true);

	// Load the Texture
	HR(CreateDDSTextureFromFile(renderDevice.dev, (_AssetsPath / L"wxTest2" / L"seafloor.dds").wstring().c_str(), nullptr, &mTextureRV1));
	HR(CreateDDSTextureFromFile(renderDevice.dev, (_AssetsPath / L"wxTest2" / L"windowslogo.dds").wstring().c_str(), nullptr, &mTextureRV2));

	// Initialize the world matrices
	mWorld = XMMatrixIdentity();

	// Initialize the view matrix
	XMVECTOR Eye = XMVectorSet(0.0f, 3.0f, -6.0f, 0.0f);
	XMVECTOR At = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	XMVECTOR Up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	mView = XMMatrixLookAtLH(Eye, At, Up);

	mBatchEffect->SetView(mView);

	// Initialize the projection matrix
	mProjection = XMMatrixPerspectiveFovLH(XM_PIDIV4,
		GetSize().x / (FLOAT)GetSize().y, 0.01f, 100.0f);

	mBatchEffect->SetProjection(mProjection);

	mFrameBeginDelegate.connect(&MyRenderWindow::frameBegin, this);
	mDrawDelegate.connect(&MyRenderWindow::draw, this);

	renderDevice.rendererEvents.waitForFrameBegin(mFrameBeginDelegate());
	renderDevice.rendererEvents.waitForDraw(mDrawDelegate());
}

bool MyRenderWindow::Destroy()
{
	ZZLAB_TRACE_THIS();

	renderDevice.cancel();

	return wxTopLevelWindow::Destroy();
}

//--------------------------------------------------------------------------------------
// Render a grid using PrimitiveBatch
//--------------------------------------------------------------------------------------
void MyRenderWindow::DrawGrid(PrimitiveBatch<VertexPositionColor>& batch, FXMVECTOR xAxis, FXMVECTOR yAxis, FXMVECTOR origin, size_t xdivs, size_t ydivs, GXMVECTOR color)
{
	mBatchEffect->Apply(renderDevice.ctx);
	renderDevice.ctx->IASetInputLayout(mBatchInputLayout);

	batch.Begin();

	xdivs = std::max<size_t>(1, xdivs);
	ydivs = std::max<size_t>(1, ydivs);

	for (size_t i = 0; i <= xdivs; ++i)
	{
		float fPercent = float(i) / float(xdivs);
		fPercent = (fPercent * 2.0f) - 1.0f;
		XMVECTOR vScale = XMVectorScale(xAxis, fPercent);
		vScale = XMVectorAdd(vScale, origin);

		VertexPositionColor v1(XMVectorSubtract(vScale, yAxis), color);
		VertexPositionColor v2(XMVectorAdd(vScale, yAxis), color);
		batch.DrawLine(v1, v2);
	}

	for (size_t i = 0; i <= ydivs; i++)
	{
		FLOAT fPercent = float(i) / float(ydivs);
		fPercent = (fPercent * 2.0f) - 1.0f;
		XMVECTOR vScale = XMVectorScale(yAxis, fPercent);
		vScale = XMVectorAdd(vScale, origin);

		VertexPositionColor v1(XMVectorSubtract(vScale, xAxis), color);
		VertexPositionColor v2(XMVectorAdd(vScale, xAxis), color);
		batch.DrawLine(v1, v2);
	}

	batch.End();
}

void MyRenderWindow::frameBegin()
{
	mTimer.now = sTimer.getTime();

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

	// Rotate cube around the origin
	mWorld = XMMatrixRotationY(t);

	renderDevice.rendererEvents.waitForFrameBegin(mFrameBeginDelegate());
}

void MyRenderWindow::draw()
{
	//DXGI_FRAME_STATISTICS stat;
	//renderDevice.swapChain->GetFrameStatistics(&stat);

	BOOL fullScreen;
	IDXGIOutputPtr target;
	HR(renderDevice.swapChain->GetFullscreenState(&fullScreen, &target));

	renderDevice.ctx->ClearRenderTargetView(mRenderTargetView, Colors::MidnightBlue);
	renderDevice.ctx->ClearDepthStencilView(mDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);

	// Draw procedurally generated dynamic grid
	const XMVECTORF32 xaxis = { 20.f, 0.f, 0.f };
	const XMVECTORF32 yaxis = { 0.f, 0.f, 20.f };
	DrawGrid(*mBatch, xaxis, yaxis, g_XMZero, 20, 20, Colors::Gray);

	// Draw sprite
	mSprites->Begin(SpriteSortMode_Deferred);
	mSprites->Draw(mTextureRV2, XMFLOAT2(10, 75), nullptr, Colors::White);
	mFont->DrawString(mSprites.get(), 
		(wformat(L"FullScreen: %d") % fullScreen).str().c_str(), XMFLOAT2(100, 10), Colors::Yellow);
	mSprites->End();

	// Draw 3D object
	XMMATRIX local = XMMatrixMultiply(mWorld, XMMatrixTranslation(-2.f, -2.f, 4.f));
	mShape->Draw(local, mView, mProjection, Colors::White, mTextureRV1);

	XMVECTOR qid = XMQuaternionIdentity();
	const XMVECTORF32 scale = { 0.01f, 0.01f, 0.01f };
	const XMVECTORF32 translate = { 3.f, -2.f, 4.f };
	XMVECTOR rotate = XMQuaternionRotationRollPitchYaw(0, XM_PI / 2.f, XM_PI / 2.f);
	local = XMMatrixMultiply(mWorld, XMMatrixTransformation(g_XMZero, qid, scale, g_XMZero, rotate, translate));
	mModel->Draw(renderDevice.ctx, *mStates, local, mView, mProjection);

	renderDevice.rendererEvents.waitForDraw(mDrawDelegate());
}

class MyFrame : public wxFrame
{
	wxDECLARE_EVENT_TABLE();

public:
	MyFrame(const wxString& title, const wxPoint& pos, const wxSize& size);
	~MyFrame();

	bool Destroy();

protected:
	IDXGIFactory1Ptr mFactory;
	std::vector<MyRenderWindow*> mRenderWindows;

	void OnFullScreen(wxCommandEvent& event);
	void _OnFullScreen();
};

wxBEGIN_EVENT_TABLE(MyFrame, wxFrame)
	EVT_MENU(ID_FULLSCREEN, MyFrame::OnFullScreen)
wxEND_EVENT_TABLE()

bool MyApp::OnInit()
{
	// set log level
	_LogFlags = LOG_TO_WIN32DEBUG | LOG_TO_FILE;
	log4cplus::Logger::getRoot().setLogLevel(log4cplus::TRACE_LOG_LEVEL);

	// install plugins
	utils::install();
	av::install();
	wx::install();

	if (!wx::App::OnInit())
		return false;

	ZZLAB_TRACE_THIS();

	zzlab::startWorkerService();
	//gfx::startRenderService();

	MyFrame *frame = new MyFrame("Show Control", wxPoint(0, 0), wxSize(500, 300));
	frame->Show(true);

	return true;
}

int MyApp::OnExit()
{
	//gfx::stopRenderService();
	zzlab::stopWorkerService();

	return wx::App::OnExit();
}

MyFrame::MyFrame(const wxString& title, const wxPoint& pos, const wxSize& size) :
wxFrame(NULL, wxID_ANY, title, pos, size)
{
	ZZLAB_TRACE_THIS();

	wxMenu *menuFile = new wxMenu;
	menuFile->Append(wxID_EXIT);
	menuFile->Append(ID_FULLSCREEN, "&Full Screen\tCtrl+F");

	wxMenuBar *menuBar = new wxMenuBar;
	menuBar->Append(menuFile, "&File");
	SetMenuBar(menuBar);
	CreateStatusBar();

	// enumerate all outputs
	{
		IDXGIFactory1Ptr pFactory;
		HR(CreateDXGIFactory1(__uuidof(IDXGIFactory), (void**)&pFactory));

		IDXGIAdapter1* pAdapter_;
		for (UINT i = 0; pFactory->EnumAdapters1(i, &pAdapter_) != DXGI_ERROR_NOT_FOUND; ++i)
		{
			IDXGIAdapter1Ptr pAdapter(pAdapter_, false);

			DXGI_ADAPTER_DESC1 desc;
			HR(pAdapter->GetDesc1(&desc));

			ZZLAB_TRACE_VALUE(desc.DeviceId);
			ZZLAB_TRACE_VALUE(desc.Revision);
			ZZLAB_TRACE_VALUE(desc.Description);
			ZZLAB_TRACE_VALUE(desc.DedicatedSystemMemory);
			ZZLAB_TRACE_VALUE(desc.DedicatedVideoMemory);
			ZZLAB_TRACE_VALUE(desc.SharedSystemMemory);
			ZZLAB_TRACE_VALUE(desc.Flags);

			ZZLAB_TRACE("Enumerate outputs....");

			IDXGIOutput* pOutput_;
			for (UINT j = 0; pAdapter->EnumOutputs(j, &pOutput_) != DXGI_ERROR_NOT_FOUND; ++j)
			{
				IDXGIOutputPtr pOutput(pOutput_, false);

				DXGI_OUTPUT_DESC desc;
				HR(pOutput->GetDesc(&desc));

				ZZLAB_TRACE_VALUE(desc.DeviceName);
				ZZLAB_TRACE_VALUE(desc.AttachedToDesktop);
				ZZLAB_TRACE("desc.DesktopCoordinates=" << 
					desc.DesktopCoordinates.left << ',' << desc.DesktopCoordinates.top << ',' <<
					desc.DesktopCoordinates.right << ',' << desc.DesktopCoordinates.bottom);

				ZZLAB_TRACE_VALUE(desc.Monitor);

				ZZLAB_TRACE_VALUE(desc.Rotation);
			}
		}

		mFactory = pFactory;
	}

	if (1)
	{
		MyRenderWindow* rw = wx::loadWindow<MyRenderWindow>(_Settings.first_node(L"test0"), this);

		{
			IDXGIAdapter1* pAdapter_;
			HR(mFactory->EnumAdapters1(0, &pAdapter_));
			rw->renderDevice.adapter = IDXGIAdapter1Ptr(pAdapter_, false);

			//IDXGIOutput* pOutput_;
			//HR(pAdapter_->EnumOutputs(0, &pOutput_));
			//rw->output = IDXGIOutputPtr(pOutput_);
		}

		rw->init();

		mRenderWindows.push_back(rw);
		rw->Show();
	}

	if (1)
	{
		MyRenderWindow* rw = wx::loadWindow<MyRenderWindow>(_Settings.first_node(L"test1"), this);

		{
			IDXGIAdapter1* pAdapter_;
			HR(mFactory->EnumAdapters1(0, &pAdapter_));
			rw->renderDevice.adapter = IDXGIAdapterPtr(pAdapter_, false);

			//IDXGIOutput* pOutput_;
			//HR(pAdapter_->EnumOutputs(0, &pOutput_));
			//rw->output = IDXGIOutputPtr(pOutput_);
		}

		rw->init();

		mRenderWindows.push_back(rw);
		rw->Show();
	}

	HR(mFactory->MakeWindowAssociation((HWND)GetHandle(), 0));
}

MyFrame::~MyFrame()	
{
	ZZLAB_TRACE_THIS();
}

bool MyFrame::Destroy()
{
	ZZLAB_TRACE_THIS();

	for (auto x : mRenderWindows)
	{
		x->Destroy();
	}

	return wxFrame::Destroy();
}

void MyFrame::OnFullScreen(wxCommandEvent& event)
{
	//gfx::_RenderService->post(boost::bind(&MyFrame::_OnFullScreen, this));
	_MainService.post(boost::bind(&MyFrame::_OnFullScreen, this));
}

void MyFrame::_OnFullScreen()
{
	ZZLAB_INFO("Full screen mode");

	for (auto x : mRenderWindows)
	{
		x->setFullscreenState(true);
	}
}

wxIMPLEMENT_APP(MyApp);
