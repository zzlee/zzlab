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
		_COM_SMARTPTR_TYPEDEF(IDirect3DSurface9, __uuidof(IDirect3DSurface9));

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

		class ZZD3D9_API RenderDevice : public gfx::ResourceManager, public gfx::RendererEvents
		{
		public:
			D3DPRESENT_PARAMETERS d3dpp;
			D3DDEVTYPE deviceType;
			DWORD behaviorFlags;
			D3DSCANLINEORDERING scanLineOrdering;

			struct clear_t {
				DWORD flags;
				D3DCOLOR color;
				float depth;
				float stencil;
			} clear;

			explicit RenderDevice();
			~RenderDevice();

			void load(XmlNode *node, HWND hWnd);
			void init(LPDIRECT3D9EX d3d, int adapter = -1, HWND hFocus = NULL);

			// NOTICE: access after init is called
			IDirect3DDevice9ExPtr dev;

			void cancel();
			void reset();

			void restoreBackBuffer()
			{
				HR(dev->SetRenderTarget(0, mBackBuffer));
			}

			void setFullscreen(bool fullScreen)
			{
				d3dpp.Windowed = fullScreen ? FALSE : TRUE;
				reset();
			}

			void toggleFullscreen()
			{
				d3dpp.Windowed = !d3dpp.Windowed;
				reset();
			}

		protected:
			boost::asio::deadline_timer mTimer;

			IDirect3DSurface9Ptr mBackBuffer;

			utils::Delegate<void(boost::system::error_code)> mMainDelegate;
			void main(boost::system::error_code error = boost::system::error_code());
		};
		
		ZZD3D9_API void loadAssets(IDirect3DDevice9ExPtr dev,
			gfx::ResourceManager* resources,
			boost::filesystem::wpath path);

		inline void loadAssets(d3d9::RenderDevice* renderDevice, boost::filesystem::wpath path)
		{
			loadAssets(renderDevice->dev, renderDevice, path);
		}

		class ZZD3D9_API Resource : public gfx::Resource
		{
		public:
			IDirect3DDevice9ExPtr dev;

			explicit Resource();
			virtual ~Resource();
		};

		class ZZD3D9_API TextureResource : public Resource
		{
		public:
			IDirect3DTexture9Ptr textures[8];

			explicit TextureResource();
			virtual ~TextureResource();
		};

		class ZZD3D9_API FileTextureResource : public TextureResource
		{
		public:
			boost::filesystem::wpath path;

			explicit FileTextureResource();
			virtual ~FileTextureResource();

			virtual void init();
		};

		class ZZD3D9_API DynamicTextureResource : public TextureResource
		{
		public:
			int width;
			int height;
			D3DFORMAT format;

			DynamicTexture texture;

			explicit DynamicTextureResource();
			virtual ~DynamicTextureResource();

			void update(const cv::Mat& mat)
			{
				updateDynamicTexture(dev, texture, mat);
			}

			void set(const cv::Scalar& v)
			{
				updateDynamicTexture(dev, texture, v);
			}

			virtual void init();
		};

		class ZZD3D9_API DynamicYUVTextureResource : public TextureResource
		{
		public:
			int width;
			int height;
			int uvWidth;
			int uvHeight;

			YUVTexture texture;

			explicit DynamicYUVTextureResource();
			virtual ~DynamicYUVTextureResource();

			void update(AVFrame* frame);

			void set(const ScalarYUV& yuv)
			{
				updateYUVTexture(dev, texture, yuv);
			}

			virtual void init();
		};

		class ZZD3D9_API RenderTextureResource : public TextureResource
		{
		public:
			int width;
			int height;
			D3DFORMAT format;

			IDirect3DSurface9Ptr surface;

			explicit RenderTextureResource();
			virtual ~RenderTextureResource();

			virtual void init();
		};

		class ZZD3D9_API RenderTexture : public RenderTextureResource, public gfx::RendererEvents
		{
		public:
			RenderDevice* renderDevice;

			RenderDevice::clear_t clear;

			explicit RenderTexture();
			virtual ~RenderTexture();

			void init();

		protected:
			utils::Delegate0 mFrameBeginDelegate;

			void onFrameBegin();
		};

		class ZZD3D9_API EffectResource : public Resource
		{
		public:
			boost::filesystem::wpath path;

			ID3DXEffectPtr effect;

			explicit EffectResource();
			virtual ~EffectResource();

			virtual void init();
		};

		class ZZD3D9_API MeshResource : public Resource
		{
		public:
			IDirect3DVertexBuffer9Ptr vertexBuffer;
			IDirect3DVertexDeclaration9Ptr vertexDecl;
			IDirect3DIndexBuffer9Ptr indexBuffer;

			explicit MeshResource();
			virtual ~MeshResource();

			virtual void draw(ID3DXEffectPtr effect) = 0;
		};

		struct ZZD3D9_API VERTEX_XYZ_UV0
		{
			D3DXVECTOR3 POSITION;
			D3DXVECTOR2 UV0;

			static IDirect3DVertexDeclaration9Ptr decl(LPDIRECT3DDEVICE9 dev);
		};

		class ZZD3D9_API QuadMeshResource : public MeshResource
		{
		public:
			explicit QuadMeshResource();
			virtual ~QuadMeshResource();

			virtual void init();
			virtual void draw(ID3DXEffectPtr effect);
		};

		class ZZD3D9_API LatticeMeshResource : public MeshResource
		{
		public:
			int width;
			int height;

			std::vector<VERTEX_XYZ_UV0> vertices;

			explicit LatticeMeshResource();
			virtual ~LatticeMeshResource();

			void allocVertices()
			{
				vertices.resize(width * height);
			}

			void updateVertices();

			virtual void init();
			virtual void draw(ID3DXEffectPtr effect);
		};

		class ZZD3D9_API eb4Renderer
		{
		public:
			gfx::RendererEvents* rendererEvents;
			d3d9::EffectResource* effect;
			d3d9::TextureResource* mainTex;
			d3d9::TextureResource* horTex;
			d3d9::TextureResource* verTex;
			d3d9::MeshResource* lattice;

			explicit eb4Renderer();
			virtual ~eb4Renderer();

			void init();

		protected:
			utils::Delegate0 mDrawDelegate;

			Eigen::Matrix4f mMVP;

			void onDraw();
		};

		class ZZD3D9_API RenderWindow : public wxTopLevelWindow, public RenderDevice
		{
		public:
			IDirect3D9ExPtr d3d9ex;
			std::wstring d3ddevRef;

			RenderWindow(wxWindow *parent, wxWindowID id, const wxString &title, const wxPoint &pos = wxDefaultPosition, const wxSize &size = wxDefaultSize, long style = wxDEFAULT_FRAME_STYLE);
			~RenderWindow();

			void init();
			bool Destroy();
		};

		ZZD3D9_API RenderWindow* loadWindow(IDirect3D9ExPtr d3d9ex, XmlNode* node, wxWindow* parent = NULL);

	} // namespace d3d9
} // namespace zzlab

#endif // __ZZLAB_D3D9_H__