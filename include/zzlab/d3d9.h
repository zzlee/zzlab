#ifndef __ZZLAB_D3D9_H__
#define __ZZLAB_D3D9_H__

#ifdef ZZD3D9_EXPORTS
#define ZZD3D9_API __declspec(dllexport)
#else
#define ZZD3D9_API __declspec(dllimport)
#endif

#include "zzlab.h"
#include "zzlab/utils.h"
#include "zzlab/gfx.h"
#include "zzlab/av.h"
#include "zzlab/wx.h"

#include <d3d9.h>
#include <d3dx9.h>
#include <comdef.h>

#include <boost/tuple/tuple.hpp>
#include <boost/asio/coroutine.hpp>
#include <opencv2/opencv.hpp>

#include <wx/toplevel.h>

namespace zzlab
{
	namespace d3d9
	{
		ZZD3D9_API void install(void);

		_COM_SMARTPTR_TYPEDEF(IDirect3D9, __uuidof(IDirect3D9));
		_COM_SMARTPTR_TYPEDEF(IDirect3D9Ex, __uuidof(IDirect3D9Ex));
		_COM_SMARTPTR_TYPEDEF(IDirect3DDevice9, __uuidof(IDirect3DDevice9));
		_COM_SMARTPTR_TYPEDEF(IDirect3DDevice9Ex, __uuidof(IDirect3DDevice9Ex));
		_COM_SMARTPTR_TYPEDEF(IDirect3DVertexBuffer9, __uuidof(IDirect3DVertexBuffer9));
		_COM_SMARTPTR_TYPEDEF(IDirect3DVertexDeclaration9, __uuidof(IDirect3DVertexDeclaration9));
		_COM_SMARTPTR_TYPEDEF(IDirect3DTexture9, __uuidof(IDirect3DTexture9));
		_COM_SMARTPTR_TYPEDEF(IDirect3DIndexBuffer9, __uuidof(IDirect3DIndexBuffer9));

		_COM_SMARTPTR_TYPEDEF(ID3DXFont, IID_ID3DXFont);
		_COM_SMARTPTR_TYPEDEF(ID3DXBuffer, IID_ID3DXBuffer);
		_COM_SMARTPTR_TYPEDEF(ID3DXEffect, IID_ID3DXEffect);
		_COM_SMARTPTR_TYPEDEF(ID3DXEffectPool, IID_ID3DXEffectPool);
		_COM_SMARTPTR_TYPEDEF(ID3DXMesh, IID_ID3DXMesh);

		typedef boost::tuple<IDirect3DTexture9Ptr, IDirect3DTexture9Ptr> DynamicTexture;
		typedef boost::tuple<DynamicTexture, DynamicTexture, DynamicTexture> YUVTexture;
		typedef boost::tuple<cv::Mat1b, cv::Mat1b, cv::Mat1b> MatYUV;
		typedef boost::tuple<uint8_t, uint8_t, uint8_t> ScalarYUV;

		struct AdapterInfo
		{
			UINT adapter;
			MONITORINFO monitor;
		};

		ZZD3D9_API IDirect3D9ExPtr createEx();

		ZZD3D9_API UINT adapterFromRegion(const RECT &rect);
		ZZD3D9_API UINT adapterFromWindow(HWND hWnd);
		ZZD3D9_API const RECT &regionFromAdapter(UINT adapter);
		ZZD3D9_API void getMonitorRegions(std::vector<AdapterInfo> &ret);

		ZZD3D9_API IDirect3DDevice9ExPtr createDeviceEx(
			LPDIRECT3D9EX d3d,
			UINT Adapter,
			D3DDEVTYPE DeviceType,
			HWND hFocusWindow,
			DWORD BehaviorFlags,
			D3DPRESENT_PARAMETERS *pPresentationParameters,
			D3DDISPLAYMODEEX *pFullscreenDisplayMode
			);

		ZZD3D9_API IDirect3DVertexBuffer9Ptr createVertexBuffer(
			LPDIRECT3DDEVICE9 pDevice,
			UINT Length,
			DWORD Usage = 0,
			DWORD FVF = 0,
			D3DPOOL Pool = D3DPOOL_DEFAULT,
			HANDLE *pSharedHandle = NULL
			);

		ZZD3D9_API IDirect3DVertexDeclaration9Ptr createVertexDeclaration(
			LPDIRECT3DDEVICE9 pDevice,
			const D3DVERTEXELEMENT9 *pVertexElements
			);

		ZZD3D9_API IDirect3DTexture9Ptr createTexture(
			LPDIRECT3DDEVICE9 pDevice,
			UINT Width,
			UINT Height,
			UINT Levels = 0,
			DWORD Usage = 0,
			D3DFORMAT Format = D3DFMT_X8R8G8B8,
			D3DPOOL Pool = D3DPOOL_DEFAULT,
			HANDLE *pSharedHandle = NULL
			);

		ZZD3D9_API IDirect3DIndexBuffer9Ptr createIndexBuffer(
			LPDIRECT3DDEVICE9 pDevice,
			UINT Length,
			DWORD Usage = 0,
			D3DFORMAT Format = D3DFMT_INDEX16,
			D3DPOOL Pool = D3DPOOL_DEFAULT,
			HANDLE *pSharedHandle = NULL
			);

		ZZD3D9_API ID3DXEffectPtr createEffectFromFile(
			LPDIRECT3DDEVICE9 pDevice,
			LPCWSTR pSrcFile,
			const D3DXMACRO *pDefines = NULL,
			LPD3DXINCLUDE pInclude = NULL,
			DWORD Flags = 0,
			LPD3DXEFFECTPOOL pPool = NULL
			);

		ZZD3D9_API ID3DXFontPtr createFontIndirect(
			LPDIRECT3DDEVICE9 pDevice,
			const D3DXFONT_DESC *pDesc
			);

		ZZD3D9_API IDirect3DTexture9Ptr createTextureFromFile(
			LPDIRECT3DDEVICE9 pDevice,
			LPCWSTR pSrcFile
			);

		ZZD3D9_API boost::tuple<ID3DXMeshPtr, ID3DXBufferPtr, ID3DXBufferPtr, DWORD>
			loadMeshFromX(
			LPDIRECT3DDEVICE9 pD3DDevice,
			LPCWSTR pSrcFile,
			DWORD Options = D3DXMESH_MANAGED
			);

		ZZD3D9_API void updateTexture(
			LPDIRECT3DTEXTURE9 pTexture,
			cv::Mat src,
			UINT Level = 0
			);

		ZZD3D9_API void updateTexture(
			LPDIRECT3DTEXTURE9 pTexture,
			cv::Scalar src,
			UINT Level = 0
			);

		ZZD3D9_API DynamicTexture createDynamicTexture(
			LPDIRECT3DDEVICE9 pDevice,
			UINT Width,
			UINT Height,
			D3DFORMAT Format
			);
		ZZD3D9_API void updateDynamicTexture(
			LPDIRECT3DDEVICE9 pDevice,
			const DynamicTexture &tex,
			cv::Mat src
			);
		ZZD3D9_API void updateDynamicTexture(
			LPDIRECT3DDEVICE9 pDevice,
			const DynamicTexture &tex,
			cv::Scalar src
			);

		inline YUVTexture createYUVTexture(
			LPDIRECT3DDEVICE9 pDevice,
			UINT Width,
			UINT Height,
			UINT uvWidth,
			UINT uvHeight
			)
		{
			return make_tuple(
				createDynamicTexture(pDevice, Width, Height, D3DFMT_L8),
				createDynamicTexture(pDevice, uvWidth, uvHeight, D3DFMT_L8),
				createDynamicTexture(pDevice, uvWidth, uvHeight, D3DFMT_L8));
		}

		inline YUVTexture createYV12Texture(
			LPDIRECT3DDEVICE9 pDevice,
			UINT Width,
			UINT Height
			)
		{
			return createYUVTexture(pDevice, Width, Height, Width / 2, Height / 2);
		}

		inline void updateYUVTexture(
			LPDIRECT3DDEVICE9 pDevice,
			const YUVTexture &tex,
			MatYUV yuv
			)
		{
			updateDynamicTexture(pDevice, tex.get<0>(), yuv.get<0>());
			updateDynamicTexture(pDevice, tex.get<1>(), yuv.get<1>());
			updateDynamicTexture(pDevice, tex.get<2>(), yuv.get<2>());
		}

		inline void updateYUVTexture(
			LPDIRECT3DDEVICE9 pDevice,
			const YUVTexture &tex,
			ScalarYUV yuv
			)
		{
			updateDynamicTexture(pDevice, tex.get<0>(), yuv.get<0>());
			updateDynamicTexture(pDevice, tex.get<1>(), yuv.get<1>());
			updateDynamicTexture(pDevice, tex.get<2>(), yuv.get<2>());
		}

		class ZZD3D9_API BeginEffect
		{
		public:
			explicit BeginEffect(ID3DXEffect *effect, DWORD flags = 0);
			~BeginEffect();

			void next()
			{
				++mCurrentPass;
			}

			bool more() const
			{
				return mCurrentPass < mPasses;
			}

			friend class Pass;
			class ZZD3D9_API Pass
			{
			public:
				explicit Pass(BeginEffect &b);
				~Pass();

			protected:
				BeginEffect &_impl;
			};

		protected:
			ID3DXEffect *mEffect;
			UINT mPasses;
			UINT mCurrentPass;
		};

		class ZZD3D9_API Device
		{
		public:
			D3DPRESENT_PARAMETERS d3dpp;
			D3DDISPLAYMODEEX d3ddm;
			bool doNotWait;
			float rate;
			utils::Timer* timeSource; // used in do-not-wait mode

			explicit Device();
			virtual ~Device();

			void load(XmlNode *node, LPDIRECT3D9EX d3d, int adapter, HWND hWnd, HWND hFocus = NULL);
			void init();

			// NOITICE: access after init is called
			IDirect3DDevice9ExPtr dev;
			gfx::RendererEvents rendererEvents;
			gfx::DeviceResourceEvents deviceResourceEvents;
			gfx::ResourceManager* resources;

		protected:
			boost::asio::deadline_timer mTimer;

			DWORD mFlags;
			size_t mDuration;
			int64_t mFrameStart;

			boost::asio::coroutine __coro_main;
			void main(boost::system::error_code error = boost::system::error_code());
		};

		class ZZD3D9_API TextureResource : public gfx::Resource
		{
		public:
			IDirect3DDevice9ExPtr dev;
			gfx::DeviceResourceEvents* deviceResourceEvents;
			boost::filesystem::wpath path;

			IDirect3DTexture9Ptr texture;

			TextureResource();
			~TextureResource();

			void init();

		protected:
			utils::SharedEvent0 mEvent0;
			boost::asio::coroutine __coro_main;
			void main();
			void initResources();
		};

		class ZZD3D9_API DynamicTextureResource : public gfx::Resource
		{
		public:
			IDirect3DDevice9ExPtr dev;
			gfx::DeviceResourceEvents* deviceResourceEvents;
			int width;
			int height;
			D3DFORMAT format;

			DynamicTexture texture;

			DynamicTextureResource();
			~DynamicTextureResource();

			void init();

		protected:
			utils::SharedEvent0 mEvent0;
			boost::asio::coroutine __coro_main;
			void main();
			void initResources();
		};

		class ZZD3D9_API DynamicYUVTextureResource : public gfx::Resource
		{
		public:
			IDirect3DDevice9ExPtr dev;
			gfx::DeviceResourceEvents* deviceResourceEvents;
			int width;
			int height;

			int uvWidth;
			int uvHeight;

			YUVTexture texture;

			DynamicYUVTextureResource();
			~DynamicYUVTextureResource();

			void init();
			void update(AVFrame* frame);

			void set(const ScalarYUV& yuv)
			{
				updateYUVTexture(dev, texture, yuv);
			}

		protected:
			utils::SharedEvent0 mEvent0;
			boost::asio::coroutine __coro_main;
			void main();
			void initResources();
		};

		class ZZD3D9_API EffectResource : public gfx::Resource
		{
		public:
			IDirect3DDevice9ExPtr dev;
			gfx::DeviceResourceEvents* deviceResourceEvents;
			boost::filesystem::wpath path;

			ID3DXEffectPtr effect;

			EffectResource();
			~EffectResource();

			void init();

		protected:
			utils::SharedEvent0 mEvent0;
			boost::asio::coroutine __coro_main;
			void main();
		};

		class ZZD3D9_API MeshResource : public gfx::Resource
		{
		public:
			IDirect3DDevice9ExPtr dev;
			gfx::DeviceResourceEvents* deviceResourceEvents;

			IDirect3DVertexBuffer9Ptr vertexBuffer;
			IDirect3DVertexDeclaration9Ptr vertexDecl;
			IDirect3DIndexBuffer9Ptr indexBuffer;

			MeshResource();
			~MeshResource();

			void init();

		protected:
			utils::SharedEvent0 mEvent0;
			boost::asio::coroutine __coro_main;
			void main();
			virtual void initResources() = 0;
		};

		class ZZD3D9_API QuadMeshResource : public MeshResource
		{
		public:
			QuadMeshResource();
			~QuadMeshResource();

			void draw(ID3DXEffectPtr effect);

		protected:
			virtual void initResources();
		};

		class ZZD3D9_API ClearScene
		{
		public:
			IDirect3DDevice9ExPtr dev;
			gfx::RendererEvents* rendererEvents;

			DWORD flags;
			D3DCOLOR color;
			float Z;
			DWORD stencil;

			explicit ClearScene();
			virtual ~ClearScene();

			void init();

		protected:
			utils::SharedEvent0 mDrawDelegate;

			void draw();
		};

	} // namespace d3d9
} // namespace zzlab

#endif // __ZZLAB_D3D9_H__