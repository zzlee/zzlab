// d3d9.cpp : 定義 DLL 應用程式的匯出函式。
//

#include "stdafx.h"

#include "zzlab/d3d9.h"
#include "zzlab/pystring.h"
#include <limits>

#include <boost/system/error_code.hpp>
#include <boost/asio/placeholders.hpp>
#include <boost/bind.hpp>
#include <boost/make_shared.hpp>

ZZLAB_USE_LOG4CPLUS(zzav);

using namespace boost;

namespace zzlab
{
	namespace d3d9
	{
		std::vector<AdapterInfo> adapterInfos;

		static bool initialize()
		{
			ZZLAB_TRACE_FUNCTION();

			IDirect3D9ExPtr d3d9 = createEx();

			ZZLAB_INFO("Create monitors map...");

			for (UINT i = 0; i < d3d9->GetAdapterCount(); ++i)
			{
				HMONITOR monitor = d3d9->GetAdapterMonitor(i);

				AdapterInfo info = { i };
				info.monitor.cbSize = sizeof(MONITORINFO);
				WIN(GetMonitorInfo(monitor, &info.monitor));

				ZZLAB_INFO("Adapter " << i << " ==> (" <<
					info.monitor.rcMonitor.left << ',' <<
					info.monitor.rcMonitor.top << ',' <<
					info.monitor.rcMonitor.right << ',' <<
					info.monitor.rcMonitor.bottom << ')');

				adapterInfos.push_back(info);
			}

			return true;
		}

		static void uninitialize()
		{
			ZZLAB_TRACE_FUNCTION();
		}

		void install(void)
		{
			Plugin plugin = {
				L"zzd3d9", initialize, uninitialize
			};

			addPlugin(plugin);
		}

		IDirect3D9ExPtr createEx()
		{
			IDirect3D9Ex *ptr;
			HR(Direct3DCreate9Ex(D3D_SDK_VERSION, &ptr));

			return IDirect3D9ExPtr(ptr, false);
		}

		UINT adapterFromRegion(const RECT &rect)
		{
			UINT ret = (std::numeric_limits<UINT>::max)();
			size_t size = 0;
			RECT intersection;
			for (std::vector<AdapterInfo>::const_iterator i = adapterInfos.begin();
				i != adapterInfos.end(); ++i)
			{
				if (IntersectRect(&intersection, &rect, &(*i).monitor.rcMonitor))
				{
					size_t s = (intersection.right - intersection.left) *
						(intersection.bottom - intersection.top);
					if (s > size)
					{
						size = s;
						ret = (*i).adapter;
					}
				}
			}

			return ret;
		}

		UINT adapterFromWindow(HWND hWnd)
		{
			RECT rect;
			GetWindowRect(hWnd, &rect);

			return adapterFromRegion(rect);
		}

		const RECT &regionFromAdapter(UINT adapter)
		{
			const static RECT empty = { 0, 0, 0, 0 };

			for (std::vector<AdapterInfo>::const_iterator i = adapterInfos.begin();
				i != adapterInfos.end(); ++i)
			{
				if (adapter == (*i).adapter)
				{
					return (*i).monitor.rcMonitor;
				}
			}

			return empty;
		}

		void getMonitorRegions(std::vector<AdapterInfo> &ret)
		{
			ret = adapterInfos;
		}

		IDirect3DDevice9ExPtr createDeviceEx(
			LPDIRECT3D9EX d3d,
			UINT Adapter,
			D3DDEVTYPE DeviceType,
			HWND hFocusWindow,
			DWORD BehaviorFlags,
			D3DPRESENT_PARAMETERS *pPresentationParameters,
			D3DDISPLAYMODEEX *pFullscreenDisplayMode)
		{
			IDirect3DDevice9Ex *ptr;
			HR(d3d->CreateDeviceEx(Adapter,
				DeviceType,
				hFocusWindow,
				BehaviorFlags,
				pPresentationParameters,
				pFullscreenDisplayMode,
				&ptr));
			return IDirect3DDevice9ExPtr(ptr, false);
		}

		IDirect3DVertexBuffer9Ptr createVertexBuffer(
			LPDIRECT3DDEVICE9 pDevice,
			UINT Length,
			DWORD Usage,
			DWORD FVF,
			D3DPOOL Pool,
			HANDLE *pSharedHandle
			)
		{
			IDirect3DVertexBuffer9 *ptr;
			HR(pDevice->CreateVertexBuffer(Length,
				Usage,
				FVF,
				Pool,
				&ptr,
				pSharedHandle));
			return IDirect3DVertexBuffer9Ptr(ptr, false);
		}

		IDirect3DVertexDeclaration9Ptr createVertexDeclaration(
			LPDIRECT3DDEVICE9 pDevice,
			const D3DVERTEXELEMENT9 *pVertexElements
			)
		{
			IDirect3DVertexDeclaration9 *ptr;
			HR(pDevice->CreateVertexDeclaration(pVertexElements, &ptr));
			return IDirect3DVertexDeclaration9Ptr(ptr, false);
		}

		IDirect3DTexture9Ptr createTexture(
			LPDIRECT3DDEVICE9 pDevice,
			UINT Width,
			UINT Height,
			UINT Levels,
			DWORD Usage,
			D3DFORMAT Format,
			D3DPOOL Pool,
			HANDLE *pSharedHandle
			)
		{
			IDirect3DTexture9 *ptr;
			HR(pDevice->CreateTexture(Width,
				Height,
				Levels,
				Usage,
				Format,
				Pool,
				&ptr,
				pSharedHandle));
			return IDirect3DTexture9Ptr(ptr, false);
		}

		IDirect3DIndexBuffer9Ptr createIndexBuffer(
			LPDIRECT3DDEVICE9 pDevice,
			UINT Length,
			DWORD Usage,
			D3DFORMAT Format,
			D3DPOOL Pool,
			HANDLE *pSharedHandle
			)
		{
			IDirect3DIndexBuffer9 *ptr;
			HR(pDevice->CreateIndexBuffer(Length,
				Usage,
				Format,
				Pool,
				&ptr,
				pSharedHandle));
			return IDirect3DIndexBuffer9Ptr(ptr, false);
		}

		ID3DXEffectPtr createEffectFromFile(
			LPDIRECT3DDEVICE9 pDevice,
			LPCWSTR pSrcFile,
			const D3DXMACRO *pDefines,
			LPD3DXINCLUDE pInclude,
			DWORD Flags,
			LPD3DXEFFECTPOOL pPool
			)
		{
			ID3DXEffect *ptr = NULL;
			ID3DXBuffer *pBufferErrors = NULL;

			HRESULT hr = D3DXCreateEffectFromFile(pDevice, pSrcFile, pDefines, pInclude,
				Flags, pPool, &ptr, &pBufferErrors);
			if (FAILED(hr))
			{
				if (pBufferErrors)
				{
					const char *pCompileErrors = (const char *)pBufferErrors->GetBufferPointer();
					ZZLAB_ERROR("Effect error: \n" << pCompileErrors << "\n");

					pBufferErrors->Release();
				}
				else
				{
					ZZLAB_ERROR("Effect error: hr=0x" << std::hex << hr << "\n");

				}

				return NULL;
			}

			return ID3DXEffectPtr(ptr, false);
		}

		ID3DXFontPtr createFontIndirect(
			LPDIRECT3DDEVICE9 pDevice,
			const D3DXFONT_DESC *pDesc
			)
		{
			ID3DXFont *ptr;
			HR(D3DXCreateFontIndirect(pDevice, pDesc, &ptr));
			return ID3DXFontPtr(ptr, false);
		}

		IDirect3DTexture9Ptr createTextureFromFile(
			LPDIRECT3DDEVICE9 pDevice,
			LPCWSTR pSrcFile
			)
		{
			IDirect3DTexture9 *ptr;
			HR(D3DXCreateTextureFromFile(pDevice, pSrcFile, &ptr));
			return IDirect3DTexture9Ptr(ptr, false);
		}

		boost::tuple<ID3DXMeshPtr, ID3DXBufferPtr, ID3DXBufferPtr, DWORD> loadMeshFromX(
			LPDIRECT3DDEVICE9 pD3DDevice,
			LPCWSTR pSrcFile,
			DWORD Options
			)
		{
			LPD3DXBUFFER pAdjacency = NULL;
			LPD3DXBUFFER pMaterials = NULL;
			DWORD numMaterials = 0;
			LPD3DXMESH pMesh = NULL;
			HR(D3DXLoadMeshFromX(
				pSrcFile,
				Options,
				pD3DDevice,
				&pAdjacency,
				&pMaterials,
				NULL,
				&numMaterials,
				&pMesh));

			return make_tuple(
				ID3DXMeshPtr(pMesh, false),
				ID3DXBufferPtr(pAdjacency, false),
				ID3DXBufferPtr(pMaterials, false),
				numMaterials
				);
		}


		void updateTexture(
			LPDIRECT3DTEXTURE9 pTexture,
			cv::Mat src,
			UINT Level
			)
		{
			D3DLOCKED_RECT rect;
			HR(pTexture->LockRect(Level, &rect, NULL, D3DLOCK_DISCARD));

			src.copyTo(cv::Mat(src.size(), src.type(), (uint8_t *)rect.pBits, rect.Pitch));

			pTexture->UnlockRect(Level);
		}

		DynamicTexture createDynamicTexture(
			LPDIRECT3DDEVICE9 pDevice,
			UINT Width,
			UINT Height,
			D3DFORMAT Format
			)
		{
			return make_tuple(
				createTexture(pDevice, Width, Height, 1, D3DUSAGE_DYNAMIC, Format, D3DPOOL_SYSTEMMEM),
				createTexture(pDevice, Width, Height, 1, D3DUSAGE_DYNAMIC, Format, D3DPOOL_DEFAULT));
		}

		void updateDynamicTexture(
			LPDIRECT3DDEVICE9 pDevice,
			const DynamicTexture &tex,
			cv::Mat src
			)
		{
			updateTexture(tex.get<0>(), src);
			pDevice->UpdateTexture(tex.get<0>(), tex.get<1>());
		}

		BeginEffect::BeginEffect(ID3DXEffect *effect, DWORD flags)
		{
			HR(effect->Begin(&mPasses, flags));
			mEffect = effect;
			mCurrentPass = 0;
		}

		BeginEffect::~BeginEffect()
		{
			HR(mEffect->End());
		}

		BeginEffect::Pass::Pass(BeginEffect &b) : _impl(b)
		{
			HR(b.mEffect->BeginPass(b.mCurrentPass));
		}

		BeginEffect::Pass::~Pass()
		{
			HR(_impl.mEffect->EndPass());
		}

		Device::Device() :
			timeSource(NULL),
			resources(NULL),
			doNotWait(true),
			rate(240.0f),
			mTimer(_MainService)
		{
			ZZLAB_TRACE_THIS();
		}

		Device::~Device()
		{
			ZZLAB_TRACE_THIS();

			mTimer.cancel();

			if (resources)
				delete resources;
		}

		static HWND sFocus = NULL;

		void Device::load(XmlNode *node, LPDIRECT3D9EX d3d, int adapter, HWND hWnd, HWND hFocus)
		{
			ZZLAB_INFO("Loading " << node->name() << ", 0x" << std::hex << hWnd <<
				", 0x" << std::hex << hFocus << "...");

			if (sFocus == NULL)
				sFocus = hWnd;
			if (hFocus == NULL)
				hFocus = sFocus;

			RECT rc;
			GetWindowRect(hWnd, &rc);
			int width = rc.right - rc.left;
			int height = rc.bottom - rc.top;

			if (adapter == -1)
				adapter = adapterFromRegion(rc);

			XmlAttribute *attr = node->first_attribute("back-buffer-width");
			d3dpp.BackBufferWidth = attr ? atoi(attr->value()) : width;

			attr = node->first_attribute("back-buffer-height");
			d3dpp.BackBufferHeight = attr ? atoi(attr->value()) : height;

			attr = node->first_attribute("back-buffer-format");
			if (attr)
			{
#define _MATCH(x) \
	if (_stricmp(attr->value(), # x) == 0) \
	d3dpp.BackBufferFormat = x; \
			else

				_MATCH(D3DFMT_R8G8B8)
					_MATCH(D3DFMT_A8R8G8B8)
					_MATCH(D3DFMT_X8R8G8B8)
					_MATCH(D3DFMT_R5G6B5)
					_MATCH(D3DFMT_A8B8G8R8)
					_MATCH(D3DFMT_X8B8G8R8)
					d3dpp.BackBufferFormat = D3DFMT_X8R8G8B8;

#undef _MATCH
			}
			else
				d3dpp.BackBufferFormat = D3DFMT_X8R8G8B8;

			attr = node->first_attribute("back-buffer-count");
			d3dpp.BackBufferCount = attr ? atoi(attr->value()) : 1;

			attr = node->first_attribute("multi-sample-type");
			if (attr)
			{
#define _MATCH(x) \
	if (_stricmp(attr->value(), # x) == 0) \
	d3dpp.MultiSampleType = x; \
			else

				_MATCH(D3DMULTISAMPLE_NONE)
					_MATCH(D3DMULTISAMPLE_NONMASKABLE)
					_MATCH(D3DMULTISAMPLE_2_SAMPLES)
					_MATCH(D3DMULTISAMPLE_3_SAMPLES)
					_MATCH(D3DMULTISAMPLE_4_SAMPLES)
					_MATCH(D3DMULTISAMPLE_5_SAMPLES)
					_MATCH(D3DMULTISAMPLE_6_SAMPLES)
					_MATCH(D3DMULTISAMPLE_7_SAMPLES)
					_MATCH(D3DMULTISAMPLE_8_SAMPLES)
					_MATCH(D3DMULTISAMPLE_9_SAMPLES)
					_MATCH(D3DMULTISAMPLE_10_SAMPLES)
					_MATCH(D3DMULTISAMPLE_11_SAMPLES)
					_MATCH(D3DMULTISAMPLE_12_SAMPLES)
					_MATCH(D3DMULTISAMPLE_13_SAMPLES)
					_MATCH(D3DMULTISAMPLE_14_SAMPLES)
					_MATCH(D3DMULTISAMPLE_15_SAMPLES)
					_MATCH(D3DMULTISAMPLE_16_SAMPLES)
					d3dpp.MultiSampleType = D3DMULTISAMPLE_NONE;

#undef _MATCH
			}
			else
				d3dpp.MultiSampleType = D3DMULTISAMPLE_NONE;

			attr = node->first_attribute("multi-sample-quality");
			d3dpp.MultiSampleQuality = attr ? atoi(attr->value()) : 0;

			attr = node->first_attribute("swap-effect");
			if (attr)
			{
#define _MATCH(x) \
	if (_stricmp(attr->value(), # x) == 0) \
	d3dpp.SwapEffect = x; \
			else

				_MATCH(D3DSWAPEFFECT_DISCARD)
					_MATCH(D3DSWAPEFFECT_FLIP)
					_MATCH(D3DSWAPEFFECT_COPY)
					_MATCH(D3DSWAPEFFECT_OVERLAY)
					_MATCH(D3DSWAPEFFECT_FLIPEX)
					d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;

#undef _MATCH
			}
			else
				d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;

			d3dpp.hDeviceWindow = hWnd;
			attr = node->first_attribute("windowed");
			d3dpp.Windowed = attr ? (_stricmp(attr->value(), "true") == 0 ? TRUE : FALSE) : FALSE;
			attr = node->first_attribute("enable-auto-depth-stencil");
			d3dpp.EnableAutoDepthStencil = attr ? (_stricmp(attr->value(), "true") == 0 ? TRUE : FALSE) : FALSE;

			attr = node->first_attribute("auto-depth-stencil-format");
			if (attr)
			{
#define _MATCH(x) \
	if (_stricmp(attr->value(), # x) == 0) \
	d3dpp.AutoDepthStencilFormat = x; \
			else

				_MATCH(D3DFMT_D16_LOCKABLE)
					_MATCH(D3DFMT_D32)
					_MATCH(D3DFMT_D15S1)
					_MATCH(D3DFMT_D24S8)
					_MATCH(D3DFMT_D24X8)
					_MATCH(D3DFMT_D24X4S4)
					_MATCH(D3DFMT_D16)
					_MATCH(D3DFMT_D32F_LOCKABLE)
					_MATCH(D3DFMT_D24FS8)
					d3dpp.AutoDepthStencilFormat = D3DFMT_D16;

#undef _MATCH
			}
			else
				d3dpp.AutoDepthStencilFormat = D3DFMT_D16;

			d3dpp.Flags = 0;
			attr = node->first_attribute("flags");
			if (attr)
			{
				std::vector<std::string> tokens;
				pystring::split(attr->value(), tokens, "|");
				for (std::vector<std::string>::const_iterator i = tokens.begin(); i != tokens.end(); ++i)
				{
					std::string s = pystring::strip(*i);
#define _MATCH(x) \
	if (_stricmp(s.c_str(), # x) == 0) \
	d3dpp.Flags |= x; \
			else

					_MATCH(D3DPRESENTFLAG_DEVICECLIP)
						_MATCH(D3DPRESENTFLAG_DISCARD_DEPTHSTENCIL)
						_MATCH(D3DPRESENTFLAG_LOCKABLE_BACKBUFFER)
						_MATCH(D3DPRESENTFLAG_NOAUTOROTATE)
						_MATCH(D3DPRESENTFLAG_UNPRUNEDMODE)
						_MATCH(D3DPRESENTFLAG_VIDEO)
						// _MATCH(D3DPRESENTFLAG_OVERLAY_LIMITEDRGB)
						// _MATCH(D3DPRESENTFLAG_OVERLAY_YCbCr_BT709)
						// _MATCH(D3DPRESENTFLAG_OVERLAY_YCbCr_xvYCC)
						// _MATCH(D3DPRESENTFLAG_RESTRICTED_CONTENT)
						// _MATCH(D3DPRESENTFLAG_RESTRICT_SHARED_RESOURCE_DRIVER)
						NULL;

#undef _MATCH
				}
			}

			if (d3dpp.Windowed)
				d3dpp.FullScreen_RefreshRateInHz = 0;
			else
			{
				attr = node->first_attribute("refresh-rate");
				d3dpp.FullScreen_RefreshRateInHz = attr ? atoi(attr->value()) : 1;
			}

			attr = node->first_attribute("presentation-interval");
			if (attr)
			{
#define _MATCH(x) \
	if (_stricmp(attr->value(), # x) == 0) \
	d3dpp.PresentationInterval = x; \
			else

				_MATCH(D3DPRESENT_INTERVAL_DEFAULT)
					_MATCH(D3DPRESENT_INTERVAL_ONE)
					_MATCH(D3DPRESENT_INTERVAL_TWO)
					_MATCH(D3DPRESENT_INTERVAL_THREE)
					_MATCH(D3DPRESENT_INTERVAL_FOUR)
					_MATCH(D3DPRESENT_INTERVAL_IMMEDIATE)
					d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;

#undef _MATCH
			}
			else
				d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;

			DWORD behaviorFlags;
			attr = node->first_attribute("behavior-flags");
			if (attr)
			{
#define _MATCH(x) \
	if (_stricmp(attr->value(), # x) == 0) \
	behaviorFlags = x; \
			else

				_MATCH(D3DCREATE_SOFTWARE_VERTEXPROCESSING)
					_MATCH(D3DCREATE_HARDWARE_VERTEXPROCESSING)
					behaviorFlags = D3DCREATE_SOFTWARE_VERTEXPROCESSING;

#undef _MATCH
			}
			else
				behaviorFlags = D3DCREATE_SOFTWARE_VERTEXPROCESSING;

			if (!d3dpp.Windowed)
				behaviorFlags |= D3DCREATE_NOWINDOWCHANGES;

			d3ddm.Size = sizeof(D3DDISPLAYMODEEX);
			d3ddm.Width = width;
			d3ddm.Height = height;
			d3ddm.RefreshRate = d3dpp.FullScreen_RefreshRateInHz;
			d3ddm.Format = d3dpp.BackBufferFormat;

			attr = node->first_attribute("scan-line-ordering");
			if (attr)
			{
#define _MATCH(x) \
	if (_stricmp(attr->value(), # x) == 0) \
	d3ddm.ScanLineOrdering = x; \
			else

				_MATCH(D3DSCANLINEORDERING_PROGRESSIVE)
					_MATCH(D3DSCANLINEORDERING_INTERLACED)
					d3ddm.ScanLineOrdering = D3DSCANLINEORDERING_PROGRESSIVE;

#undef _MATCH
			}
			else
				d3ddm.ScanLineOrdering = D3DSCANLINEORDERING_PROGRESSIVE;

			D3DDEVTYPE deviceType;
			attr = node->first_attribute("device-type");
			if (attr)
			{
#define _MATCH(x) \
	if (_stricmp(attr->value(), # x) == 0) \
	deviceType = x; \
			else

				_MATCH(D3DDEVTYPE_HAL)
					_MATCH(D3DDEVTYPE_NULLREF)
					_MATCH(D3DDEVTYPE_REF)
					_MATCH(D3DDEVTYPE_SW)
					deviceType = D3DDEVTYPE_HAL;

#undef _MATCH
			}
			else
				deviceType = D3DDEVTYPE_HAL;

			attr = node->first_attribute("do-not-wait");
			doNotWait = attr ? (_stricmp(attr->value(), "true") == 0 ? true : false) : false;

			attr = node->first_attribute("rate");
			if (attr)
				rate = (float)atof(attr->value());

			ZZLAB_INFO("Adapter: " << adapter);
			dev = createDeviceEx(d3d, adapter, deviceType, hFocus, behaviorFlags, &d3dpp, d3dpp.Windowed ? NULL : &d3ddm);
		}

		void Device::init()
		{
			resources = new gfx::ResourceManager();

			if (doNotWait)
				mFlags = D3DPRESENT_DONOTWAIT;
			else
				mFlags = 0;

			mDuration = size_t(1000 / rate);

			// yield
			mTimer.expires_from_now(posix_time::milliseconds(0));
			mTimer.async_wait(boost::bind(&Device::main, this, asio::placeholders::error));
		}

#include <boost/asio/yield.hpp>
		void Device::main(boost::system::error_code error)
		{
			if (error)
				return;

			HRESULT hr;
			D3DPRESENT_PARAMETERS tmp;
			D3DDISPLAYMODEEX tmp1;
			int64_t diff;

			reenter(__coro_main) for (;;)
			{
				mFrameStart = timeSource->getTimeInMicroseconds();
				rendererEvents.frameBegin();

				HR(dev->BeginScene());
				rendererEvents.draw();
				HR(dev->EndScene());

				for (;;)
				{
					hr = dev->PresentEx(NULL, NULL, NULL, NULL, mFlags);
					if (SUCCEEDED(hr))
						break;

					if (hr == D3DERR_DEVICELOST)
					{
						ZZLAB_INFO("D3D9 device lost");
						deviceResourceEvents.deviceLost();

						// try to reset device
						for (;;)
						{
							hr = dev->TestCooperativeLevel();
							if (hr == D3DERR_DEVICENOTRESET)
							{
								ZZLAB_INFO("Try to reset D3D9 device");

								tmp = d3dpp;
								tmp1 = d3ddm;
								hr = dev->ResetEx(&tmp, &tmp1);
								if (SUCCEEDED(hr))
								{
									deviceResourceEvents.deviceReset();

									break;
								}
							}

							mTimer.expires_from_now(posix_time::milliseconds(100));
							yield mTimer.async_wait(boost::bind(&Device::main, this, asio::placeholders::error));
						}
					}

					// wait awhile and then try another presentation
					mTimer.expires_from_now(posix_time::milliseconds(mDuration / 4));
					yield mTimer.async_wait(boost::bind(&Device::main, this, asio::placeholders::error));
				}

				rendererEvents.frameEnd();

				diff = timeSource->getTimeInMicroseconds() - mFrameStart;

				// wait rest of time
				mTimer.expires_from_now(posix_time::microseconds(mDuration * 1000 - diff));
				yield mTimer.async_wait(boost::bind(&Device::main, this, asio::placeholders::error));
			}
		}
#include <boost/asio/unyield.hpp>

		TextureResource::TextureResource() : deviceResourceEvents(NULL)
		{
			ZZLAB_TRACE_THIS();
		}

		TextureResource::~TextureResource()
		{
			ZZLAB_TRACE_THIS();
		}

		void TextureResource::init()
		{
			initResources();

			mEvent0.connect(bind(&TextureResource::main, this));
			main();
		}

#include <boost/asio/yield.hpp>
		void TextureResource::main()
		{
			reenter(__coro_main) for (;;)
			{
				yield deviceResourceEvents->waitForDeviceLost(mEvent0());
				texture = NULL;

				yield deviceResourceEvents->waitForDeviceReset(mEvent0());
				initResources();
			}
		}
#include <boost/asio/unyield.hpp>

		void TextureResource::initResources()
		{
			ZZLAB_TRACE("Create texture from " << path.wstring());
			texture = createTextureFromFile(dev, path.wstring().c_str());
		}

		DynamicTextureResource::DynamicTextureResource() :
			deviceResourceEvents(NULL),
			width(256),
			height(256),
			format(D3DFMT_R8G8B8)
		{
			ZZLAB_TRACE_THIS();
		}

		DynamicTextureResource::~DynamicTextureResource()
		{
			ZZLAB_TRACE_THIS();
		}

		void DynamicTextureResource::init()
		{
			initResources();

			mEvent0.connect(bind(&DynamicTextureResource::main, this));
			main();
		}

#include <boost/asio/yield.hpp>
		void DynamicTextureResource::main()
		{
			reenter(__coro_main) for (;;)
			{
				yield deviceResourceEvents->waitForDeviceLost(mEvent0());
				texture = DynamicTexture();

				yield deviceResourceEvents->waitForDeviceReset(mEvent0());
				initResources();
			}
		}
#include <boost/asio/unyield.hpp>

		void DynamicTextureResource::initResources()
		{
			ZZLAB_TRACE("Create dynamic texture of " << width << 'x' << height << '@' << format);
			texture = createDynamicTexture(dev, width, height, format);
		}
		
		DynamicYV12TextureResource::DynamicYV12TextureResource() :
			deviceResourceEvents(NULL),
			width(256),
			height(256)
		{
			ZZLAB_TRACE_THIS();
		}

		DynamicYV12TextureResource::~DynamicYV12TextureResource()
		{
			ZZLAB_TRACE_THIS();
		}

		void DynamicYV12TextureResource::init()
		{
			initResources();

			mEvent0.connect(bind(&DynamicYV12TextureResource::main, this));
			main();
		}

		void DynamicYV12TextureResource::update(AVFrame* frame)
		{
			assert(frame->width == width && frame->height == height);

			updateYUVTexture(dev, texture,
				make_tuple(
				cv::Mat1b(height, width, frame->data[0], frame->linesize[0]),
				cv::Mat1b(height / 2, width / 2, frame->data[1], frame->linesize[1]),
				cv::Mat1b(height / 2, width / 2, frame->data[2], frame->linesize[2])
				));
		}

#include <boost/asio/yield.hpp>
		void DynamicYV12TextureResource::main()
		{
			reenter(__coro_main) for (;;)
			{
				yield deviceResourceEvents->waitForDeviceLost(mEvent0());
				texture = YUVTexture();

				yield deviceResourceEvents->waitForDeviceReset(mEvent0());
				initResources();
			}
		}
#include <boost/asio/unyield.hpp>

		void DynamicYV12TextureResource::initResources()
		{
			ZZLAB_TRACE("Create dynamic YV12 texture of " << width << 'x' << height);
			texture = createYV12Texture(dev, width, height);
		}

		EffectResource::EffectResource() : deviceResourceEvents(NULL)
		{
			ZZLAB_TRACE_THIS();
		}

		EffectResource::~EffectResource()
		{
			ZZLAB_TRACE_THIS();
		}

		void EffectResource::init()
		{
			ZZLAB_TRACE("Create effect from " << path.wstring());
			effect = createEffectFromFile(dev, path.wstring().c_str());

			mEvent0.connect(bind(&EffectResource::main, this));
			main();
		}

#include <boost/asio/yield.hpp>
		void EffectResource::main()
		{
			reenter(__coro_main) for (;;)
			{
				yield deviceResourceEvents->waitForDeviceLost(mEvent0());
				effect->OnLostDevice();

				yield deviceResourceEvents->waitForDeviceReset(mEvent0());
				effect->OnResetDevice();
			}
		}
#include <boost/asio/unyield.hpp>

		namespace detail
		{
			struct VERTEX_XYZ_UV0
			{
				D3DXVECTOR3 POSITION;
				D3DXVECTOR2 UV0;

				static IDirect3DVertexDeclaration9Ptr decl(LPDIRECT3DDEVICE9 dev)
				{
					static const D3DVERTEXELEMENT9 declaration[] =
					{
						{
							0, 0,
							D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0
						},
						{
							0, sizeof(D3DXVECTOR3),
							D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0
						},
						D3DDECL_END()
					};

					return createVertexDeclaration(dev, declaration);
				}
			};

		}

		MeshResource::MeshResource() : deviceResourceEvents(NULL)
		{
			ZZLAB_TRACE_THIS();
		}

		MeshResource::~MeshResource()
		{
			ZZLAB_TRACE_THIS();
		}

		void MeshResource::init()
		{
			initResources();

			mEvent0.connect(bind(&MeshResource::main, this));
			main();
		}

#include <boost/asio/yield.hpp>
		void MeshResource::main()
		{
			reenter(__coro_main) for (;;)
			{
				yield deviceResourceEvents->waitForDeviceLost(mEvent0());
				vertexBuffer = NULL;
				vertexDecl = NULL;
				indexBuffer = NULL;

				yield deviceResourceEvents->waitForDeviceReset(mEvent0());
				initResources();
			}
		}
#include <boost/asio/unyield.hpp>

		QuadMeshResource::QuadMeshResource()
		{
			ZZLAB_TRACE_THIS();
		}

		QuadMeshResource::~QuadMeshResource()
		{
			ZZLAB_TRACE_THIS();
		}

		void QuadMeshResource::initResources()
		{
			vertexBuffer = createVertexBuffer(dev,
				4 * sizeof(detail::VERTEX_XYZ_UV0));
			{
				detail::VERTEX_XYZ_UV0 src[] =
				{
					{ D3DXVECTOR3(-0.5f, -0.5f, 0.0f), D3DXVECTOR2(0, 1) },
					{ D3DXVECTOR3(0.5f, -0.5f, 0.0f), D3DXVECTOR2(1, 1) },
					{ D3DXVECTOR3(0.5f, 0.5f, 0.0f), D3DXVECTOR2(1, 0) },
					{ D3DXVECTOR3(-0.5f, 0.5f, 0.0f), D3DXVECTOR2(0, 0) },
				};

				detail::VERTEX_XYZ_UV0 *dst;
				vertexBuffer->Lock(0, 0, (void **)&dst, 0);

				memcpy(dst, src, sizeof(src));

				vertexBuffer->Unlock();
			}
			vertexDecl = detail::VERTEX_XYZ_UV0::decl(dev);

			indexBuffer = createIndexBuffer(dev, 3 * 2 * sizeof(int16_t));
			{
				int16_t *dst;
				indexBuffer->Lock(0, 0, (void **)&dst, 0);

				dst[0] = 0;
				dst[1] = 2;
				dst[2] = 1;

				dst[3] = 0;
				dst[4] = 3;
				dst[5] = 2;

				indexBuffer->Unlock();
			}
		}

		inline static void drawQuad(IDirect3DDevice9Ex *dev, ID3DXEffect *effect)
		{
			for (BeginEffect efx(effect); efx.more(); efx.next())
			{
				BeginEffect::Pass pass(efx);

				dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, 4, 0, 2);
			}
		}

		void QuadMeshResource::draw(ID3DXEffectPtr effect)
		{
			HR(dev->SetVertexDeclaration(vertexDecl));
			HR(dev->SetStreamSource(0, vertexBuffer, 0, sizeof(detail::VERTEX_XYZ_UV0)));
			HR(dev->SetIndices(indexBuffer));

			drawQuad(dev, effect);
		}

	} // namespace d3d9

} // namespace zzlab