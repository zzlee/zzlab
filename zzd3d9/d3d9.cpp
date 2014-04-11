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

#include <rapidxml/rapidxml_utils.hpp>

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

		void updateTexture(
			LPDIRECT3DTEXTURE9 pTexture,
			cv::Scalar src,
			UINT Level
			)
		{
			D3DSURFACE_DESC desc;
			HR(pTexture->GetLevelDesc(Level, &desc));

			D3DLOCKED_RECT rect;
			HR(pTexture->LockRect(Level, &rect, NULL, D3DLOCK_DISCARD));

			switch (desc.Format)
			{
			case D3DFMT_R8G8B8:
				cv::Mat3b(desc.Height, desc.Width,
					(cv::Vec3b *)rect.pBits, rect.Pitch).setTo(src);
				break;

			case D3DFMT_A8R8G8B8:
			case D3DFMT_X8R8G8B8:
			case D3DFMT_A8B8G8R8:
			case D3DFMT_X8B8G8R8:
				cv::Mat4b(desc.Height, desc.Width,
					(cv::Vec4b *)rect.pBits, rect.Pitch).setTo(src);
				break;

			case D3DFMT_A8:
			case D3DFMT_L8:
				cv::Mat1b(desc.Height, desc.Width,
					(uint8_t *)rect.pBits, rect.Pitch).setTo(src);
				break;

			case D3DFMT_G16R16:
				cv::Mat2s(desc.Height, desc.Width,
					(cv::Vec2s *)rect.pBits, rect.Pitch).setTo(src);
				break;

			case D3DFMT_D16:
				cv::Mat1s(desc.Height, desc.Width,
					(short *)rect.pBits, rect.Pitch).setTo(src);
				break;

			case D3DFMT_D32:
				cv::Mat1i(desc.Height, desc.Width,
					(int *)rect.pBits, rect.Pitch).setTo(src);
				break;
			}

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

		void updateDynamicTexture(
			LPDIRECT3DDEVICE9 pDevice,
			const DynamicTexture &tex,
			cv::Scalar src
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

		void Device::load(XmlNode *node, HWND hWnd)
		{
			ZZLAB_INFO("Loading " << node->name() << ", 0x" << std::hex << hWnd << "...");

			RECT rc;
			GetWindowRect(hWnd, &rc);
			int width = rc.right - rc.left;
			int height = rc.bottom - rc.top;

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
	if (_stricmp(attr->value(), # x) == 0) { d3dpp.MultiSampleType = x; } else

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

			attr = node->first_attribute("behavior-flags");
			if (attr)
			{
#define _MATCH(x) \
	if (_stricmp(attr->value(), # x) == 0) \
	mBehaviorFlags = x; \
			else

				_MATCH(D3DCREATE_SOFTWARE_VERTEXPROCESSING)
					_MATCH(D3DCREATE_HARDWARE_VERTEXPROCESSING)
					mBehaviorFlags = D3DCREATE_SOFTWARE_VERTEXPROCESSING;

#undef _MATCH
			}
			else
				mBehaviorFlags = D3DCREATE_SOFTWARE_VERTEXPROCESSING;

			if (!d3dpp.Windowed)
				mBehaviorFlags |= D3DCREATE_NOWINDOWCHANGES;

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

			attr = node->first_attribute("device-type");
			if (attr)
			{
#define _MATCH(x) \
	if (_stricmp(attr->value(), # x) == 0) \
	mDeviceType = x; \
			else

				_MATCH(D3DDEVTYPE_HAL)
					_MATCH(D3DDEVTYPE_NULLREF)
					_MATCH(D3DDEVTYPE_REF)
					_MATCH(D3DDEVTYPE_SW)
					mDeviceType = D3DDEVTYPE_HAL;

#undef _MATCH
			}
			else
				mDeviceType = D3DDEVTYPE_HAL;

			attr = node->first_attribute("do-not-wait");
			doNotWait = attr ? (_stricmp(attr->value(), "true") == 0 ? true : false) : false;

			attr = node->first_attribute("rate");
			if (attr)
				rate = (float)atof(attr->value());
		}

		void Device::init(LPDIRECT3D9EX d3d, int adapter, HWND hFocus)
		{
			if (sFocus == NULL)
				sFocus = d3dpp.hDeviceWindow;
			if (hFocus == NULL)
				hFocus = sFocus;

			if (adapter == -1)
			{
				RECT rc;
				GetWindowRect(d3dpp.hDeviceWindow, &rc);
				adapter = adapterFromRegion(rc);
			}

			ZZLAB_INFO("Adapter: " << adapter);
			dev = createDeviceEx(d3d, adapter, mDeviceType, hFocus, mBehaviorFlags, &d3dpp, d3dpp.Windowed ? NULL : &d3ddm);

			resources = new gfx::ResourceManager();

			IDirect3DSurface9* s;
			HR(dev->GetRenderTarget(0, &s));
			mBackBuffer = IDirect3DSurface9Ptr(s, false);

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

		void loadAssets(d3d9::Device* dev, boost::filesystem::wpath path)
		{
			ZZLAB_INFO("Loading assets " << path.wstring() << " ...");

			XmlFile file(path.string().c_str());

			XmlDocument settings;
			settings.parse<0>(file.data());

			XmlNode* assets = settings.first_node("Assets");

			for (XmlNode* node = assets->first_node("Texture"); node; node = node->next_sibling("Texture"))
			{
				d3d9::FileTextureResource* res = new d3d9::FileTextureResource();
				res->dev = dev->dev;
				res->deviceResourceEvents = &dev->deviceResourceEvents;
				res->path = _AssetsPath / node->first_attribute("path")->value();
				res->init();
				dev->resources->set(node->first_attribute("name")->value(), res);
			}

			for (XmlNode* node = assets->first_node("Effect"); node; node = node->next_sibling("Effect"))
			{
				d3d9::EffectResource* res = new d3d9::EffectResource();
				res->dev = dev->dev;
				res->deviceResourceEvents = &dev->deviceResourceEvents;
				res->path = _AssetsPath / node->first_attribute("path")->value();
				res->init();
				dev->resources->set(node->first_attribute("name")->value(), res);
			}

			for (XmlNode* node = assets->first_node("Quad"); node; node = node->next_sibling("Quad"))
			{
				d3d9::QuadMeshResource* res = new d3d9::QuadMeshResource();
				res->dev = dev->dev;
				res->deviceResourceEvents = &dev->deviceResourceEvents;
				res->init();
				dev->resources->set(node->first_attribute("name")->value(), res);
			}
		}

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
				resetResources();

				yield deviceResourceEvents->waitForDeviceReset(mEvent0());
				initResources();
			}
		}
#include <boost/asio/unyield.hpp>

		FileTextureResource::FileTextureResource()
		{
			ZZLAB_TRACE_THIS();
		}

		FileTextureResource::~FileTextureResource()
		{
			ZZLAB_TRACE_THIS();
		}

		void FileTextureResource::resetResources()
		{
			textures[0] = NULL;
		}

		void FileTextureResource::initResources()
		{
			ZZLAB_TRACE("Create texture from " << path.wstring());
			textures[0] = createTextureFromFile(dev, path.wstring().c_str());
		}

		DynamicTextureResource::DynamicTextureResource() :
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

		void DynamicTextureResource::resetResources()
		{
			texture = DynamicTexture();
			textures[0] = NULL;
		}

		void DynamicTextureResource::initResources()
		{
			ZZLAB_TRACE("Create dynamic texture of " << width << 'x' << height << '@' << format);
			texture = createDynamicTexture(dev, width, height, format);
			textures[0] = texture.get<1>();
		}

		DynamicYUVTextureResource::DynamicYUVTextureResource() :
			width(256),
			height(256),
			uvWidth(256),
			uvHeight(256)
		{
			ZZLAB_TRACE_THIS();
		}

		DynamicYUVTextureResource::~DynamicYUVTextureResource()
		{
			ZZLAB_TRACE_THIS();
		}

		void DynamicYUVTextureResource::update(AVFrame* frame)
		{
			updateYUVTexture(dev, texture,
				make_tuple(
				cv::Mat1b(height, width, frame->data[0], frame->linesize[0]),
				cv::Mat1b(uvHeight, uvWidth, frame->data[1], frame->linesize[1]),
				cv::Mat1b(uvHeight, uvWidth, frame->data[2], frame->linesize[2])
				));
		}

		void DynamicYUVTextureResource::resetResources()
		{
			texture = YUVTexture();
			textures[0] = textures[1] = textures[2] = NULL;
		}

		void DynamicYUVTextureResource::initResources()
		{
			ZZLAB_TRACE("Create dynamic YUV texture, Y plane size: " << width << 'x' << height <<
				", UV plane size: " << uvWidth << 'x' << uvHeight);
			texture = createYUVTexture(dev, width, height, uvWidth, uvHeight);
			textures[0] = texture.get<0>().get<1>();
			textures[1] = texture.get<1>().get<1>();
			textures[2] = texture.get<2>().get<1>();
		}

		RenderTextureResource::RenderTextureResource() :
			width(256),
			height(256),
			format(D3DFMT_X8R8G8B8)
		{
			ZZLAB_TRACE_THIS();
		}

		RenderTextureResource::~RenderTextureResource()
		{
			ZZLAB_TRACE_THIS();
		}

		void RenderTextureResource::resetResources()
		{
			surface = NULL;
			textures[0] = NULL;
		}

		void RenderTextureResource::initResources()
		{
			textures[0] = createTexture(dev, width, height, 1, D3DUSAGE_RENDERTARGET, format);

			IDirect3DSurface9* s;
			textures[0]->GetSurfaceLevel(0, &s);
			surface = IDirect3DSurface9Ptr(s, false);
		}

		RenderTexture::RenderTexture() : device(NULL)
		{
			ZZLAB_TRACE_THIS();
		}

		RenderTexture::~RenderTexture()
		{
			ZZLAB_TRACE_THIS();
		}

		void RenderTexture::init()
		{
			dev = device->dev;
			deviceResourceEvents = &device->deviceResourceEvents;
			RenderTextureResource::init();

			mFrameBeginDelegate.connect(bind(&RenderTexture::frameBegin, this));

			device->rendererEvents.waitForFrameBegin(mFrameBeginDelegate());
		}

		void RenderTexture::frameBegin()
		{
			HR(dev->SetRenderTarget(0, surface));
			rendererEvents.frameBegin();

			HR(dev->BeginScene());
			rendererEvents.draw();
			HR(dev->EndScene());

			rendererEvents.frameEnd();
			device->restoreBackBuffer();

			device->rendererEvents.waitForFrameBegin(mFrameBeginDelegate());
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
				4 * sizeof(VERTEX_XYZ_UV0));
			{
				VERTEX_XYZ_UV0 src[] =
				{
					{ D3DXVECTOR3(-0.5f, 0.5f, 0.0f), D3DXVECTOR2(0, 0) },
					{ D3DXVECTOR3(0.5f, 0.5f, 0.0f), D3DXVECTOR2(1, 0) },
					{ D3DXVECTOR3(-0.5f, -0.5f, 0.0f), D3DXVECTOR2(0, 1) },
					{ D3DXVECTOR3(0.5f, -0.5f, 0.0f), D3DXVECTOR2(1, 1) },
				};

				VERTEX_XYZ_UV0 *dst;
				vertexBuffer->Lock(0, 0, (void **)&dst, 0);

				memcpy(dst, src, sizeof(src));

				vertexBuffer->Unlock();
			}
			vertexDecl = VERTEX_XYZ_UV0::decl(dev);

			indexBuffer = createIndexBuffer(dev, 6 * sizeof(int16_t));
			{
				int16_t *dst;
				indexBuffer->Lock(0, 0, (void **)&dst, 0);

				dst[0] = 0;
				dst[1] = 1;
				dst[2] = 2;

				dst[3] = 2;
				dst[4] = 1;
				dst[5] = 3;

				indexBuffer->Unlock();
			}
		}

		inline static void drawTriangleList(IDirect3DDevice9Ex *dev, ID3DXEffect *effect, UINT NumVertices, UINT PrimitiveCount)
		{
			for (BeginEffect efx(effect); efx.more(); efx.next())
			{
				BeginEffect::Pass pass(efx);

				dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, NumVertices, 0, PrimitiveCount);
			}
		}

		void QuadMeshResource::draw(ID3DXEffectPtr effect)
		{
			HR(dev->SetVertexDeclaration(vertexDecl));
			HR(dev->SetStreamSource(0, vertexBuffer, 0, sizeof(VERTEX_XYZ_UV0)));
			HR(dev->SetIndices(indexBuffer));

			drawTriangleList(dev, effect, 4, 2);
		}

		IDirect3DVertexDeclaration9Ptr VERTEX_XYZ_UV0::decl(LPDIRECT3DDEVICE9 dev)
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

		LatticeMeshResource::LatticeMeshResource() : width(8), height(8), vertices(8 * 8)
		{
			ZZLAB_TRACE_THIS();
		}

		LatticeMeshResource::~LatticeMeshResource()
		{
			ZZLAB_TRACE_THIS();
		}

		void LatticeMeshResource::initResources()
		{
			vertexBuffer = createVertexBuffer(dev, vertices.size() * sizeof(VERTEX_XYZ_UV0));
			updateVertices();

			vertexDecl = VERTEX_XYZ_UV0::decl(dev);

			indexBuffer = createIndexBuffer(dev, 6 * (width - 1) * (height - 1) * sizeof(int16_t));
			{
				int16_t *dst;
				indexBuffer->Lock(0, 0, (void **)&dst, 0);

				size_t stride = width - 1;
				for (size_t x = 0; x < width - 1; ++x)
				{
					for (size_t y = 0; y < height - 1; ++y)
					{
						int16_t o = x + y * width;
						int16_t *p = &dst[(x + y * stride) * 6];

						p[0] = o + 0;
						p[1] = o + 1;
						p[2] = o + width * 1;

						p[3] = p[2];
						p[4] = p[1];
						p[5] = p[3] + 1;
					}
				}

				indexBuffer->Unlock();
			}
		}

		void LatticeMeshResource::updateVertices()
		{
			VERTEX_XYZ_UV0 *dst;
			vertexBuffer->Lock(0, 0, (void **)&dst, 0);

			memcpy(dst, &vertices[0], vertices.size() * sizeof(VERTEX_XYZ_UV0));

			vertexBuffer->Unlock();
		}

		void LatticeMeshResource::draw(ID3DXEffectPtr effect)
		{
			HR(dev->SetVertexDeclaration(vertexDecl));
			HR(dev->SetStreamSource(0, vertexBuffer, 0, sizeof(VERTEX_XYZ_UV0)));
			HR(dev->SetIndices(indexBuffer));

			drawTriangleList(dev, effect, width * height, (width - 1) * (height - 1) * 2);
		}

		ClearScene::ClearScene() :
			dev(NULL),
			rendererEvents(NULL),
			flags(D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER),
			color(D3DCOLOR_XRGB(0, 0, 0)),
			Z(1.0f),
			stencil(0.0f)
		{
			ZZLAB_TRACE_THIS();
		}

		ClearScene::~ClearScene()
		{
			ZZLAB_TRACE_THIS();
		}

		void ClearScene::init()
		{
			mDelegate.connect(bind(&ClearScene::frameBegin, this));
			rendererEvents->waitForFrameBegin(mDelegate());
		}

		void ClearScene::frameBegin()
		{
			//ZZLAB_TRACE_THIS();

			HR(dev->Clear(0, NULL, flags, color, Z, stencil));
			rendererEvents->waitForFrameBegin(mDelegate());
		}

		eb4Renderer::eb4Renderer() : rendererEvents(NULL), effect(NULL), mainTex(NULL), horTex(NULL), verTex(NULL)
		{
			ZZLAB_TRACE_THIS();
		}

		eb4Renderer::~eb4Renderer()
		{
			ZZLAB_TRACE_THIS();
		}

		void eb4Renderer::init()
		{
			gfx::Camera camera;
			camera.proj = gfx::orthoLH(1.0f, 1.0f, 1.0f, 100.0f);
			camera.updateT(
				gfx::lookAtLH(
				Eigen::Vector3f(0, 0, -10),
				Eigen::Vector3f(0.0f, 0.0f, 0.0f),
				Eigen::Vector3f::UnitY()));
			Eigen::Affine3f t0 = Eigen::Affine3f::Identity() *
				Eigen::Translation3f(-0.5f, -0.5f, 0.0f);
			Eigen::Projective3f MATRIX_MVP = camera.matrix() * t0;
			mMVP = MATRIX_MVP.matrix();

			mDrawDelegate.connect(bind(&eb4Renderer::draw, this));

			rendererEvents->waitForDraw(mDrawDelegate());
		}

		void eb4Renderer::draw()
		{
			//ZZLAB_TRACE_THIS();

			effect->effect->SetMatrix("MATRIX_MVP", (const D3DXMATRIX *)mMVP.data());
			if (mainTex)
				effect->effect->SetTexture("MainTex", mainTex->textures[0]);
			if (horTex)
				effect->effect->SetTexture("HorBlend", horTex->textures[0]);
			if (verTex)
				effect->effect->SetTexture("VerBlend", verTex->textures[0]);

			lattice->draw(effect->effect);

			rendererEvents->waitForDraw(mDrawDelegate());
		}

	} // namespace d3d9
} // namespace zzlab