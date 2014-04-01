// gfx.cpp : 定義 DLL 應用程式的匯出函式。
//

#include "stdafx.h"

#include "zzlab/gfx.h"

#include <boost/bind.hpp>

ZZLAB_USE_LOG4CPLUS(zzgfx);

namespace zzlab
{
	namespace gfx
	{
		static void dummy() {};

		RendererEvents::RendererEvents()
		{
			ZZLAB_TRACE_THIS();

			for (size_t i = 0; i < MAX_LAYERS; ++i)
			{
				mDraw[i] = new utils::AsyncEvents0();
				mDrawing[i] = false;
			}
		}

		RendererEvents::~RendererEvents()
		{
			ZZLAB_TRACE_THIS();

			for (size_t i = 0; i < MAX_LAYERS; ++i)
			{
				delete mDraw[i];
			}
		}

		void RendererEvents::draw()
		{
			for (size_t i = 0; i < MAX_LAYERS; ++i)
			{
				mDrawing[i] = true;
				mDraw[i]->invoke();
				mDrawing[i] = false;
			}
		}

		DeviceResourceEvents::DeviceResourceEvents()
		{
			ZZLAB_TRACE_THIS();
		}

		DeviceResourceEvents::~DeviceResourceEvents()
		{
			ZZLAB_TRACE_THIS();
		}

		Resource::Resource()
		{
			ZZLAB_TRACE_THIS();
		}

		Resource::~Resource()
		{
			ZZLAB_TRACE_THIS();
		}

		ResourceManager::ResourceManager()
		{
			ZZLAB_TRACE_THIS();
		}

		ResourceManager::~ResourceManager()
		{
			ZZLAB_TRACE_THIS();

			clear();
		}

		void ResourceManager::set(const std::string &name, Resource* obj)
		{
			pool_t::iterator i = mPool.find(name);
			if (i != mPool.end())
			{
				delete (*i).second;
				(*i).second = obj;
			}
			else
				mPool[name] = obj;
		}

		Resource* ResourceManager::remove(const std::string &name)
		{
			pool_t::iterator i = mPool.find(name);
			if (i != mPool.end())
			{
				Resource* ret = (*i).second;
				mPool.erase(i);

				return ret;
			}

			return NULL;
		}

		void ResourceManager::clear()
		{
			for (pool_t::const_iterator i = mPool.begin(); i != mPool.end(); ++i)
				delete (*i).second;

			mPool.clear();
		}

		Eigen::Matrix4f perspectiveRH(float w, float h, float zn, float zf)
		{
			Eigen::Matrix4f ret;

			float z = zn - zf;

			ret <<
				2 * zn / w, 0, 0, 0,
				0, 2 * zn / h, 0, 0,
				0, 0, zf / z, zn *zf / z,
				0, 0, -1, 0;

			return ret;
		}

		Eigen::Matrix4f perspectiveLH(float w, float h, float zn, float zf)
		{
			Eigen::Matrix4f ret;

			float z = zf - zn;

			ret <<
				2 * zn / w, 0, 0, 0,
				0, 2 * zn / h, 0, 0,
				0, 0, zf / z, -zn *zf / z,
				0, 0, 1, 0;

			return ret;
		}

		Eigen::Matrix4f perspectiveFovRH(float fovy, float Aspect, float zn, float zf)
		{
			Eigen::Matrix4f ret;

			float yScale = 1 / tan(fovy / 2);
			float xScale = yScale / Aspect;
			float z = zn - zf;

			ret <<
				xScale, 0, 0, 0,
				0, yScale, 0, 0,
				0, 0, zf / z, zn *zf / z,
				0, 0, -1, 0;

			return ret;
		}

		Eigen::Matrix4f perspectiveFovLH(float fovy, float Aspect, float zn, float zf)
		{
			Eigen::Matrix4f ret;

			float yScale = 1 / tan(fovy / 2);
			float xScale = yScale / Aspect;
			float z = zf - zn;

			ret <<
				xScale, 0, 0, 0,
				0, yScale, 0, 0,
				0, 0, zf / z, -zn *zf / z,
				0, 0, 1, 0;

			return ret;
		}

		Eigen::Matrix4f perspectiveOffCenterRH(float l, float r, float b, float t, float zn, float zf)
		{
			Eigen::Matrix4f ret;

			float z = zn - zf;

			ret <<
				2 * zn / (r - l), 0, 0, 0,
				0, 2 * zn / (t - b), 0, 0,
				(l + r) / (r - l), (t + b) / (t - b), zf / z, zn *zf / z,
				0, 0, -1, 0;

			return ret;
		}

		Eigen::Matrix4f perspectiveOffCenterLH(float l, float r, float b, float t, float zn, float zf)
		{
			Eigen::Matrix4f ret;

			float z = zf - zn;

			ret <<
				2 * zn / (r - l), 0, 0, 0,
				0, 2 * zn / (t - b), 0, 0,
				(l + r) / (l - r), (t + b) / (b - t), zf / z, -zn *zf / z,
				0, 0, 1, 0;

			return ret;
		}

		Eigen::Matrix4f orthoRH(float w, float h, float zn, float zf)
		{
			Eigen::Matrix4f ret;

			float z = zn - zf;

			ret <<
				2 / w, 0, 0, 0,
				0, 2 / h, 0, 0,
				0, 0, 1 / z, zn / z,
				0, 0, 0, 1;

			return ret;
		}

		Eigen::Matrix4f orthoLH(float w, float h, float zn, float zf)
		{
			Eigen::Matrix4f ret;

			float z = zf - zn;

			ret <<
				2 / w, 0, 0, 0,
				0, 2 / h, 0, 0,
				0, 0, 1 / z, -zn / z,
				0, 0, 0, 1;

			return ret;
		}

		Eigen::Matrix4f orthoOffCenterRH(float l, float r, float b, float t, float zn, float zf)
		{
			Eigen::Matrix4f ret;

			float z = zn - zf;

			ret <<
				2 / (r - l), 0, 0, (l + r) / (l - r),
				0, 2 / (t - b), 0, (t + b) / (b - t),
				0, 0, 1 / z, zn / z,
				0, 0, 0, 1;

			return ret;
		}

		Eigen::Matrix4f orthoOffCenterLH(float l, float r, float b, float t, float zn, float zf)
		{
			Eigen::Matrix4f ret;

			float z = zf - zn;

			ret <<
				2 / (r - l), 0, 0, (l + r) / (l - r),
				0, 2 / (t - b), 0, (t + b) / (b - t),
				0, 0, 1 / z, -zn / z,
				0, 0, 0, 1;

			return ret;
		}


		Eigen::Affine3f lookAtRH(const Eigen::Vector3f &eye, const Eigen::Vector3f &at, const Eigen::Vector3f &up)
		{
			Eigen::Matrix3f mat;
			mat.col(2) = (eye - at).normalized();
			mat.col(0) = up.cross(mat.col(2)).normalized();
			mat.col(1) = mat.col(2).cross(mat.col(0));

			return Eigen::Affine3f::Identity() * Eigen::Translation3f(eye) * Eigen::AngleAxisf(mat);
		}

		Eigen::Affine3f lookAtLH(const Eigen::Vector3f &eye, const Eigen::Vector3f &at, const Eigen::Vector3f &up)
		{
			Eigen::Matrix3f mat;
			mat.col(2) = (at - eye).normalized();
			mat.col(0) = up.cross(mat.col(2)).normalized();
			mat.col(1) = mat.col(2).cross(mat.col(0));

			return Eigen::Affine3f::Identity() * Eigen::Translation3f(eye) * Eigen::AngleAxisf(mat);
		}

		Eigen::Vector3f eulerAngles(const Eigen::Quaternionf &q1)
		{
			float test = q1.x() * q1.y() + q1.z() * q1.w();
			if (test > 0.499f)   // singularity at north pole
				return Eigen::Vector3f(0, 2 * atan2(q1.x(), q1.w()), float(M_PI / 2));
			else if (test < -0.499f)   // singularity at south pole
				return Eigen::Vector3f(0, -2 * atan2f(q1.x(), q1.w()), -float(M_PI / 2));

			float sqx = q1.x() * q1.x();
			float sqy = q1.y() * q1.y();
			float sqz = q1.z() * q1.z();
			return Eigen::Vector3f(
				atan2f(2 * q1.x() * q1.w() - 2 * q1.y() * q1.z(), 1 - 2 * sqx - 2 * sqz),
				atan2f(2 * q1.y() * q1.w() - 2 * q1.x() * q1.z(), 1 - 2 * sqy - 2 * sqz),
				asinf(2 * test));
		}

		Camera::Camera()
		{
			ZZLAB_TRACE_THIS();
		}

		Camera::~Camera()
		{
			ZZLAB_TRACE_THIS();
		}

	} // namespace av
} // namespace zzlab