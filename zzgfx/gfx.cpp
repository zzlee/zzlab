// gfx.cpp : 定義 DLL 應用程式的匯出函式。
//

#include "stdafx.h"

#include "zzlab/gfx.h"

#include <boost/bind.hpp>
#include <boost/thread.hpp>

ZZLAB_USE_LOG4CPLUS(zzgfx);

using namespace boost;

namespace zzlab
{
	namespace gfx
	{
		ZZGFX_API boost::asio::io_service* _RenderService = NULL;

		static bool initialize()
		{
			ZZLAB_TRACE_FUNCTION();

			_RenderService = &_MainService;

			return true;
		}

		static void uninitialize()
		{
			ZZLAB_TRACE_FUNCTION();
		}

		void install(void)
		{
			Plugin plugin = {
				L"zzgfx", initialize, uninitialize
			};

			addPlugin(plugin);
		}

		static void dummy() {};

		static bool renderServiceRunning = false;
		static boost::thread* renderThread = nullptr;
		static IdleForever* idle = nullptr;

		static void _ioservice()
		{
			ZZLAB_INFO("Render thread started.");
			_RenderService->run();
			ZZLAB_INFO("Render thread stopped.");
		}

		ZZGFX_API void startRenderService()
		{
			if (renderServiceRunning)
			{
				ZZLAB_WARN("Render service is RUNNING.");
			}
			else
			{
				ZZLAB_TRACE_FUNCTION();

				_RenderService = new asio::io_service();

				idle = new IdleForever(*_RenderService);
				idle->init();

				renderThread = new boost::thread(bind(_ioservice));

				renderServiceRunning = true;

				ZZLAB_INFO("Render service started.");
			}
		}

		ZZGFX_API void stopRenderService()
		{
			ZZLAB_TRACE_FUNCTION();

			if (renderServiceRunning)
			{
				ZZLAB_STEP();
				_RenderService->stop();
				ZZLAB_STEP();
				renderThread->join();
				ZZLAB_STEP();

				renderServiceRunning = false;

				ZZLAB_STEP();
				delete idle;
				idle = nullptr;

				ZZLAB_STEP();
				delete renderThread;
				renderThread = nullptr;

				ZZLAB_STEP();
				delete _RenderService;
				_RenderService = &_MainService;

				ZZLAB_INFO("Render service stopped.");
			}
			else
			{
				ZZLAB_WARN("Render service is NOT running.");
			}
		}

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

		void ResourceManager::set(const std::wstring &name, Resource* obj)
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

		Resource* ResourceManager::remove(const std::wstring &name)
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

		XmlResource::XmlResource() : file(nullptr)
		{
			ZZLAB_TRACE_THIS();
		}

		XmlResource::~XmlResource()
		{
			ZZLAB_TRACE_THIS();

			if (file)
				delete file;
		}

		void XmlResource::init()
		{
			file = new XmlFile(path.string().c_str());
			doc.parse<0>(file->data());
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