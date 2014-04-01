#ifndef __ZZLAB_GFX_H__
#define __ZZLAB_GFX_H__

#ifdef ZZGFX_EXPORTS
#define ZZGFX_API __declspec(dllexport)
#else
#define ZZGFX_API __declspec(dllimport)
#endif

#include "zzlab.h"
#include "zzlab/utils.h"

#include <boost/unordered_map.hpp>

#include <Eigen/Core>
#include <Eigen/Geometry>

namespace zzlab
{
	namespace gfx
	{
		class ZZGFX_API RendererEvents
		{
		public:
			enum LayerTag
			{
				LAYER_0,
				LAYER_BACKGROUND = 0,

				LAYER_1,
				LAYER_2,
				LAYER_3,
				LAYER_4,

				LAYER_5,
				LAYER_MAIN = 5,

				LAYER_6,
				LAYER_7,
				LAYER_8,

				LAYER_9,
				LAYER_OVERLAY = 9,

				MAX_LAYERS
			};

			explicit RendererEvents();
			virtual ~RendererEvents();

			template<class T>
			void waitForFrameBegin(T cb)
			{
				mFrameBegin.enqueue(cb);
			}

			template<class T>
			void waitForDraw(T cb, size_t layer = LAYER_MAIN)
			{
				mDraw[layer]->enqueue(cb);
			}

			template<class T>
			void waitForFrameEnd(T cb)
			{
				mFrameEnd.enqueue(cb);
			}

			void frameBegin()
			{
				mFrameBegin.invoke();
			}

			void draw();

			bool isDrawing(size_t layer) const
			{
				return mDrawing[layer];
			}

			void frameEnd()
			{
				mFrameEnd.invoke();
			}

		protected:
			utils::AsyncEvents0 mFrameBegin;
			utils::AsyncEvents0* mDraw[MAX_LAYERS];
			bool mDrawing[MAX_LAYERS];
			utils::AsyncEvents0 mFrameEnd;
		};
		
		class ZZGFX_API DeviceResourceEvents
		{
		public:
			explicit DeviceResourceEvents();
			virtual ~DeviceResourceEvents();

			template<class T>
			void waitForDeviceLost(T cb)
			{
				mDeviceLost.enqueue(cb);
			}

			template<class T>
			void waitForDeviceReset(T cb)
			{
				mDeviceReset.enqueue(cb);
			}

			void deviceLost()
			{
				mDeviceLost.invoke();
			}

			void deviceReset()
			{
				mDeviceReset.invoke();
			}

		protected:
			utils::AsyncEvents0 mDeviceLost;
			utils::AsyncEvents0 mDeviceReset;
		};

		class ZZGFX_API Resource
		{
		public:
			explicit Resource();
			virtual ~Resource();
		};

		class ZZGFX_API ResourceManager
		{
		public:
			explicit ResourceManager();
			~ResourceManager();

			Resource* get(const std::string &name)
			{
				return mPool[name];
			}

			void set(const std::string &name, Resource* obj);

			template<class T>
			T* get(const std::string &name)
			{
				return dynamic_cast<T*>(get(name));
			}
						
			Resource* remove(const std::string &name);

			template<class T>
			T* remove(const std::string &name)
			{
				return dynamic_cast<T*>(remove(name));
			}

			void clear();

		protected:
			typedef boost::unordered_map<std::string, Resource*> pool_t;

			pool_t mPool;
		};

		ZZGFX_API Eigen::Matrix4f perspectiveRH(float w, float h, float zn, float zf);
		ZZGFX_API Eigen::Matrix4f perspectiveLH(float w, float h, float zn, float zf);
		ZZGFX_API Eigen::Matrix4f perspectiveFovRH(float fovy, float Aspect, float zn, float zf);
		ZZGFX_API Eigen::Matrix4f perspectiveFovLH(float fovy, float Aspect, float zn, float zf);
		ZZGFX_API Eigen::Matrix4f perspectiveOffCenterRH(float l, float r, float b, float t, float zn, float zf);
		ZZGFX_API Eigen::Matrix4f perspectiveOffCenterLH(float l, float r, float b, float t, float zn, float zf);
		ZZGFX_API Eigen::Matrix4f orthoRH(float w, float h, float zn, float zf);
		ZZGFX_API Eigen::Matrix4f orthoLH(float w, float h, float zn, float zf);
		ZZGFX_API Eigen::Matrix4f orthoOffCenterRH(float l, float r, float b, float t, float zn, float zf);
		ZZGFX_API Eigen::Matrix4f orthoOffCenterLH(float l, float r, float b, float t, float zn, float zf);
		ZZGFX_API Eigen::Affine3f lookAtRH(const Eigen::Vector3f &eye, const Eigen::Vector3f &at, const Eigen::Vector3f &up);
		ZZGFX_API Eigen::Affine3f lookAtLH(const Eigen::Vector3f &eye, const Eigen::Vector3f &at, const Eigen::Vector3f &up);
		ZZGFX_API Eigen::Vector3f eulerAngles(const Eigen::Quaternionf &q1);

		class ZZGFX_API Camera
		{
		public:
			EIGEN_MAKE_ALIGNED_OPERATOR_NEW

			Eigen::Matrix4f proj;

			explicit Camera();
			virtual ~Camera();

			const Eigen::Matrix4f &matrix()
			{
				return mMatrix;
			}

			void update(const Eigen::Affine3f &v)
			{
				mMatrix = proj * v.matrix();
			}

			void updateT(const Eigen::Affine3f &v)
			{
				mMatrix = proj * (
					Eigen::Affine3f::Identity() * 
					Eigen::AngleAxisf(v.rotation().transpose()) * 
					Eigen::Translation3f(-v.translation())).matrix();
			}

		protected:
			Eigen::Matrix4f mMatrix;
		};

	} // namespace gfx
} // namespace zzlab

#endif // __ZZLAB_GFX_H__