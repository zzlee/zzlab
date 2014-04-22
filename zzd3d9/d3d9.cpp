// d3d9.cpp : 定義 DLL 應用程式的匯出函式。
//

#include "stdafx.h"

#include "zzlab/d3d9.h"
#include <limits>

#include <boost/system/error_code.hpp>
#include <boost/asio/placeholders.hpp>
#include <boost/bind.hpp>
#include <boost/make_shared.hpp>
#include <boost/locale.hpp>
#include <boost/algorithm/string.hpp>

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

		RenderDevice::RenderDevice() :
			mTimer(*gfx::_RenderService)
		{
			ZZLAB_TRACE_THIS();

			ZeroMemory(&d3dpp, sizeof(d3dpp));
			d3dpp.BackBufferFormat = D3DFMT_X8R8G8B8;
			d3dpp.BackBufferCount = 1;
			d3dpp.MultiSampleType = D3DMULTISAMPLE_NONE;
			d3dpp.MultiSampleQuality = 0;
			d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
			d3dpp.Windowed = TRUE;
			d3dpp.EnableAutoDepthStencil = TRUE;
			d3dpp.AutoDepthStencilFormat = D3DFMT_D24S8;
			d3dpp.Flags = 0;
			d3dpp.FullScreen_RefreshRateInHz = 0;
			d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;

			deviceType = D3DDEVTYPE_HAL;
			scanLineOrdering = D3DSCANLINEORDERING_PROGRESSIVE;
		}

		RenderDevice::~RenderDevice()
		{
			ZZLAB_TRACE_THIS();
		}

		void RenderDevice::load(XmlNode *node, HWND hWnd)
		{
			ZZLAB_INFO("Loading " << node->name() << ", 0x" << std::hex << hWnd << "...");

			RECT rc;
			GetWindowRect(hWnd, &rc);
			int width = rc.right - rc.left;
			int height = rc.bottom - rc.top;

			XmlAttribute *attr = node->first_attribute(L"back-buffer-width");
			d3dpp.BackBufferWidth = attr ? _wtoi(attr->value()) : width;

			attr = node->first_attribute(L"back-buffer-height");
			d3dpp.BackBufferHeight = attr ? _wtoi(attr->value()) : height;

			attr = node->first_attribute(L"back-buffer-format");
			if (attr)
			{
#define _MATCH(x) \
	if (_wcsicmp(attr->value(), _T( # x )) == 0) \
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

			attr = node->first_attribute(L"back-buffer-count");
			d3dpp.BackBufferCount = attr ? _wtoi(attr->value()) : 1;

			attr = node->first_attribute(L"multi-sample-type");
			if (attr)
			{
#define _MATCH(x) \
	if (_wcsicmp(attr->value(), _T( # x )) == 0) { d3dpp.MultiSampleType = x; } else

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

			attr = node->first_attribute(L"multi-sample-quality");
			d3dpp.MultiSampleQuality = attr ? _wtoi(attr->value()) : 0;

			attr = node->first_attribute(L"swap-effect");
			if (attr)
			{
#define _MATCH(x) \
	if (_wcsicmp(attr->value(), _T( # x )) == 0) \
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
			attr = node->first_attribute(L"windowed");
			d3dpp.Windowed = attr ? (_wcsicmp(attr->value(), L"true") == 0 ? TRUE : FALSE) : FALSE;
			attr = node->first_attribute(L"enable-auto-depth-stencil");
			d3dpp.EnableAutoDepthStencil = attr ? (_wcsicmp(attr->value(), L"true") == 0 ? TRUE : FALSE) : FALSE;

			attr = node->first_attribute(L"auto-depth-stencil-format");
			if (attr)
			{
#define _MATCH(x) \
	if (_wcsicmp(attr->value(), _T( # x )) == 0) \
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
					d3dpp.AutoDepthStencilFormat = D3DFMT_D24S8;

#undef _MATCH
			}
			else
				d3dpp.AutoDepthStencilFormat = D3DFMT_D24S8;

			d3dpp.Flags = 0;
			attr = node->first_attribute(L"flags");
			if (attr)
			{
				std::wstring val = attr->value();

				typedef split_iterator<std::wstring::iterator> wstring_split_iterator;
				for (wstring_split_iterator It = make_split_iterator(val, first_finder(L"|", is_iequal()));
					It != wstring_split_iterator(); ++It)
				{
					std::wstring s = copy_range<std::wstring>(*It);
					trim(s);

#define _MATCH(x) \
	if (_wcsicmp(s.c_str(),_T( # x)) == 0) \
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

			attr = node->first_attribute(L"refresh-rate");
			d3dpp.FullScreen_RefreshRateInHz = attr ? _wtoi(attr->value()) : 0;

			attr = node->first_attribute(L"presentation-interval");
			if (attr)
			{
#define _MATCH(x) \
	if (_wcsicmp(attr->value(), _T( # x )) == 0) \
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

			attr = node->first_attribute(L"behavior-flags");
			if (attr)
			{
#define _MATCH(x) \
	if (_wcsicmp(attr->value(), _T( # x )) == 0) \
	behaviorFlags = x; \
			else

				_MATCH(D3DCREATE_SOFTWARE_VERTEXPROCESSING)
					_MATCH(D3DCREATE_HARDWARE_VERTEXPROCESSING)
					behaviorFlags = D3DCREATE_SOFTWARE_VERTEXPROCESSING;

#undef _MATCH
			}
			else
				behaviorFlags = D3DCREATE_SOFTWARE_VERTEXPROCESSING;

			attr = node->first_attribute(L"scan-line-ordering");
			if (attr)
			{
#define _MATCH(x) \
	if (_wcsicmp(attr->value(), _T( # x )) == 0) \
	scanLineOrdering = x; \
			else

				_MATCH(D3DSCANLINEORDERING_PROGRESSIVE)
					_MATCH(D3DSCANLINEORDERING_INTERLACED)
					scanLineOrdering = D3DSCANLINEORDERING_PROGRESSIVE;

#undef _MATCH
			}
			else
				scanLineOrdering = D3DSCANLINEORDERING_PROGRESSIVE;

			attr = node->first_attribute(L"device-type");
			if (attr)
			{
#define _MATCH(x) \
	if (_wcsicmp(attr->value(), _T( # x )) == 0) \
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
		}

		void RenderDevice::init(LPDIRECT3D9EX d3d, int adapter, HWND hFocus)
		{
			ZZLAB_TRACE_THIS();

			static HWND sFocus = NULL;

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

			ZZLAB_INFO("Use adapter: " << adapter);

			DWORD behaviorFlags_ = behaviorFlags;
			if (!d3dpp.Windowed)
				behaviorFlags_ |= D3DCREATE_NOWINDOWCHANGES;

			D3DPRESENT_PARAMETERS tmp = d3dpp;
			if (tmp.Windowed)
				tmp.FullScreen_RefreshRateInHz = 0;

			D3DDISPLAYMODEEX d3ddm;
			d3ddm.Size = sizeof(D3DDISPLAYMODEEX);
			d3ddm.Width = d3dpp.BackBufferWidth;
			d3ddm.Height = d3dpp.BackBufferHeight;
			d3ddm.RefreshRate = d3dpp.FullScreen_RefreshRateInHz;
			d3ddm.Format = d3dpp.BackBufferFormat;
			d3ddm.ScanLineOrdering = scanLineOrdering;

			dev = createDeviceEx(d3d, adapter, deviceType, hFocus, behaviorFlags_, &tmp, tmp.Windowed ? NULL : &d3ddm);

			IDirect3DSurface9* s;
			HR(dev->GetRenderTarget(0, &s));
			mBackBuffer = IDirect3DSurface9Ptr(s, false);

			mMainDelegate.connect(&RenderDevice::main, this, asio::placeholders::error);
			gfx::_RenderService->post(boost::bind(mMainDelegate(), system::error_code()));
		}

		void RenderDevice::reset()
		{
			D3DPRESENT_PARAMETERS tmp = d3dpp;

			if (d3dpp.Windowed)
			{
				tmp.FullScreen_RefreshRateInHz = 0;
				HR(dev->ResetEx(&tmp, NULL));
			}
			else
			{
				D3DDISPLAYMODEEX d3ddm;
				d3ddm.Size = sizeof(D3DDISPLAYMODEEX);
				d3ddm.Width = d3dpp.BackBufferWidth;
				d3ddm.Height = d3dpp.BackBufferHeight;
				d3ddm.RefreshRate = d3dpp.FullScreen_RefreshRateInHz;
				d3ddm.Format = d3dpp.BackBufferFormat;
				d3ddm.ScanLineOrdering = scanLineOrdering;

				HR(dev->ResetEx(&tmp, &d3ddm));
			}
		}

		void RenderDevice::main(boost::system::error_code error)
		{
			if (error)
				return;

			frameBegin();

			HR(dev->BeginScene());
			draw();
			HR(dev->EndScene());

			HR(dev->PresentEx(NULL, NULL, NULL, NULL, 0));

			frameEnd();

			mTimer.expires_from_now(posix_time::microseconds(4));
			mTimer.async_wait(mMainDelegate());
		}

		void loadAssets(IDirect3DDevice9ExPtr dev,
			gfx::ResourceManager* resources,
			boost::filesystem::wpath path)
		{
			ZZLAB_INFO("Loading assets " << path.wstring() << " ...");

			XmlFile file(path.string().c_str());

			XmlDocument settings;
			settings.parse<0>(file.data());

			XmlNode* assets = settings.first_node(L"Assets");

			for (XmlNode* node = assets->first_node(L"Texture"); node; node = node->next_sibling(L"Texture"))
			{
				d3d9::FileTextureResource* res = new d3d9::FileTextureResource();
				res->dev = dev;
				res->path = _AssetsPath / node->first_attribute(L"path")->value();
				res->init();
				resources->set(node->first_attribute(L"name")->value(), res);
			}

			for (XmlNode* node = assets->first_node(L"Effect"); node; node = node->next_sibling(L"Effect"))
			{
				d3d9::EffectResource* res = new d3d9::EffectResource();
				res->dev = dev;
				res->path = _AssetsPath / node->first_attribute(L"path")->value();
				res->init();
				resources->set(node->first_attribute(L"name")->value(), res);
			}

			for (XmlNode* node = assets->first_node(L"Quad"); node; node = node->next_sibling(L"Quad"))
			{
				d3d9::QuadMeshResource* res = new d3d9::QuadMeshResource();
				res->dev = dev;
				res->init();
				resources->set(node->first_attribute(L"name")->value(), res);
			}
		}

		Resource::Resource()
		{
			ZZLAB_TRACE_THIS();
		}

		Resource::~Resource()
		{
			ZZLAB_TRACE_THIS();
		}

		TextureResource::TextureResource()
		{
			ZZLAB_TRACE_THIS();
		}

		TextureResource::~TextureResource()
		{
			ZZLAB_TRACE_THIS();
		}

		FileTextureResource::FileTextureResource()
		{
			ZZLAB_TRACE_THIS();
		}

		FileTextureResource::~FileTextureResource()
		{
			ZZLAB_TRACE_THIS();
		}

		void FileTextureResource::init()
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

		void DynamicTextureResource::init()
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

		void DynamicYUVTextureResource::init()
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

		void RenderTextureResource::init()
		{
			textures[0] = createTexture(dev, width, height, 1, D3DUSAGE_RENDERTARGET, format);

			IDirect3DSurface9* s;
			textures[0]->GetSurfaceLevel(0, &s);
			surface = IDirect3DSurface9Ptr(s, false);
		}

		RenderTexture::RenderTexture() : renderDevice(NULL)
		{
			ZZLAB_TRACE_THIS();
		}

		RenderTexture::~RenderTexture()
		{
			ZZLAB_TRACE_THIS();
		}

		void RenderTexture::init()
		{
			dev = renderDevice->dev;
			RenderTextureResource::init();

			mFrameBeginDelegate.connect(bind(&RenderTexture::onFrameBegin, this));

			renderDevice->waitForFrameBegin(mFrameBeginDelegate());
		}

		void RenderTexture::onFrameBegin()
		{
			HR(dev->SetRenderTarget(0, surface));
			frameBegin();

			HR(dev->BeginScene());
			draw();
			HR(dev->EndScene());

			frameEnd();
			renderDevice->restoreBackBuffer();

			renderDevice->waitForFrameBegin(mFrameBeginDelegate());
		}

		EffectResource::EffectResource()
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
		}

		MeshResource::MeshResource()
		{
			ZZLAB_TRACE_THIS();
		}

		MeshResource::~MeshResource()
		{
			ZZLAB_TRACE_THIS();
		}

		QuadMeshResource::QuadMeshResource()
		{
			ZZLAB_TRACE_THIS();
		}

		QuadMeshResource::~QuadMeshResource()
		{
			ZZLAB_TRACE_THIS();
		}

		void QuadMeshResource::init()
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

		void LatticeMeshResource::init()
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
			mDelegate.connect(bind(&ClearScene::onFrameBegin, this));
			rendererEvents->waitForFrameBegin(mDelegate());
		}

		void ClearScene::onFrameBegin()
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

			mDrawDelegate.connect(bind(&eb4Renderer::onDraw, this));

			rendererEvents->waitForDraw(mDrawDelegate());
		}

		void eb4Renderer::onDraw()
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