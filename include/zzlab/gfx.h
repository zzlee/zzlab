#ifndef __ZZLAB_GFX_H__
#define __ZZLAB_GFX_H__

#ifdef ZZGFX_EXPORTS
#define ZZGFX_API __declspec(dllexport)
#else
#define ZZGFX_API __declspec(dllimport)
#endif

#include "zzlab.h"
#include "zzlab/utils.h"

#include <vector>

#include <boost/unordered_map.hpp>
#include <boost/shared_ptr.hpp>

#include <Eigen/Core>
#include <Eigen/Geometry>

#include <opencv2/opencv.hpp>

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

		template<class T, class Traits, class PContext = Parallel> class Subdivision2D
		{
		public:
			typedef T value_type;

			struct Vertex2D
			{
				value_type x, y;
				uint8_t flags; // traits

				Vertex2D() : x(0), y(0), flags(Traits::none) {}
				Vertex2D(value_type x, value_type y) : x(x), y(y), flags(Traits::none) {}
				Vertex2D(const Vertex2D &v) : x(v.x), y(v.y), flags(v.flags) {}
			};

			typedef std::vector<Vertex2D> Vertex2Ds;
			typedef boost::shared_ptr<Vertex2Ds> Vertex2DsPtr;

			static void LinearInterpolate(const cv::Size &oldSize, const Vertex2Ds &oldVertices, cv::Size &newSize, Vertex2Ds &newVervices)
			{
				newSize.width = oldSize.width + oldSize.width - 1;
				newSize.height = oldSize.height + oldSize.height - 1;
				newVervices.resize(newSize.width * newSize.height);

				PContext::For(tbb::blocked_range2d<int, int>(0, oldSize.height, 0, oldSize.width), LinearInterpolate1(oldSize, oldVertices, newSize, newVervices));
				PContext::For(tbb::blocked_range2d<int, int>(0, oldSize.height, 0, oldSize.width - 1), LinearInterpolate2(newSize, newVervices));
				PContext::For(tbb::blocked_range2d<int, int>(0, oldSize.height - 1, 0, oldSize.width), LinearInterpolate3(newSize, newVervices));
				PContext::For(tbb::blocked_range2d<int, int>(0, oldSize.height - 1, 0, oldSize.width - 1), LinearInterpolate4(newSize, newVervices));
			}

			static void Average(const cv::Size &size, const Vertex2Ds &oldVertices, Vertex2Ds &newVertices)
			{
				newVertices = oldVertices;

				PContext::For(tbb::blocked_range<int>(0, size.width - 2), Average1(size, oldVertices, newVertices));
				PContext::For(tbb::blocked_range<int>(0, size.height - 2), Average2(size, oldVertices, newVertices));
				PContext::For(tbb::blocked_range2d<int, int>(0, size.height - 2, 0, size.width - 2), Average3(size, oldVertices, newVertices));
			}

			static Vertex2DsPtr doSubdivision(const cv::Size &srcSize, const Vertex2Ds &src, int level, cv::Size &dstSize)
			{
				dstSize = srcSize;
				Vertex2DsPtr oldVertices(new Vertex2Ds(src));
				cv::Size newSize;
				Vertex2DsPtr newVertices(new Vertex2Ds());
				Vertex2DsPtr newVertices1(new Vertex2Ds());
				for (int i = 0; i < level; ++i)
				{
					LinearInterpolate(dstSize, *oldVertices, newSize, *newVertices1);
					Average(newSize, *newVertices1, *newVertices);

					std::swap(dstSize, newSize);
					std::swap(oldVertices, newVertices);
				}

				return oldVertices;
			}

		protected:
			struct LinearInterpolate1
			{
				const cv::Size &oldSize;
				const Vertex2Ds &oldVertices;
				const cv::Size &newSize;
				Vertex2Ds &newVertices;

				LinearInterpolate1(const cv::Size &oldSize, const Vertex2Ds &oldVertices, const cv::Size &newSize, Vertex2Ds &newVertices)
					: oldSize(oldSize), oldVertices(oldVertices), newSize(newSize), newVertices(newVertices)
				{
				}

				void operator()(tbb::blocked_range2d<int, int> &range) const
				{
					const Vertex2D *ppt0_ = &oldVertices[range.rows().begin() * oldSize.width + range.cols().begin()];
					Vertex2D *ppt1_ = &newVertices[range.rows().begin() * newSize.width * 2 + range.cols().begin() * 2];
					for (int y = range.rows().begin(); y != range.rows().end(); ++y, ppt0_ += oldSize.width, ppt1_ += newSize.width * 2)
					{
						const Vertex2D *ppt0 = ppt0_;
						Vertex2D *ppt1 = ppt1_;
						for (int x = range.cols().begin(); x != range.cols().end(); ++x, ++ppt0, ppt1 += 2)
						{
							*ppt1 = *ppt0;
						}
					}
				}
			};

			struct LinearInterpolate2
			{
				const cv::Size &size;
				Vertex2Ds &vertices;

				LinearInterpolate2(const cv::Size &size, Vertex2Ds &vertices)
					: size(size), vertices(vertices)
				{
				}

				void operator()(tbb::blocked_range2d<int, int> &range) const
				{
					Vertex2D *ppt_ = &vertices[range.rows().begin() * size.width * 2 + range.cols().begin() * 2];
					for (int y = range.rows().begin(); y != range.rows().end(); ++y, ppt_ += size.width * 2)
					{
						Vertex2D *ppt = ppt_;
						for (int x = range.cols().begin(); x != range.cols().end(); ++x, ppt += 2)
						{
							const Vertex2D &pt0 = *ppt;
							const Vertex2D &pt1 = *(ppt + 2);
							Vertex2D &pt = *(ppt + 1);

							pt.x = (pt0.x + pt1.x) / 2;
							pt.y = (pt0.y + pt1.y) / 2;
						}
					}
				}
			};

			struct LinearInterpolate3
			{
				const cv::Size &size;
				Vertex2Ds &vertices;

				LinearInterpolate3(const cv::Size &size, Vertex2Ds &vertices)
					: size(size), vertices(vertices)
				{
				}

				void operator()(tbb::blocked_range2d<int, int> &range) const
				{
					Vertex2D *ppt_ = &vertices[range.rows().begin() * size.width * 2 + range.cols().begin() * 2];
					for (int y = range.rows().begin(); y != range.rows().end(); ++y, ppt_ += size.width * 2)
					{
						Vertex2D *ppt = ppt_;
						for (int x = range.cols().begin(); x != range.cols().end(); ++x, ppt += 2)
						{
							const Vertex2D &pt0 = *ppt;
							const Vertex2D &pt1 = *(ppt + size.width * 2);
							Vertex2D &pt = *(ppt + size.width);

							pt.x = (pt0.x + pt1.x) / 2;
							pt.y = (pt0.y + pt1.y) / 2;
						}
					}
				}
			};

			struct LinearInterpolate4
			{
				const cv::Size &size;
				Vertex2Ds &vertices;

				LinearInterpolate4(const cv::Size &size, Vertex2Ds &vertices)
					: size(size), vertices(vertices)
				{
				}

				void operator()(tbb::blocked_range2d<int, int> &range) const
				{
					Vertex2D *ppt_ = &vertices[range.rows().begin() * size.width * 2 + range.cols().begin() * 2];
					for (int y = range.rows().begin(); y != range.rows().end(); ++y, ppt_ += size.width * 2)
					{
						Vertex2D *ppt = ppt_;
						for (int x = range.cols().begin(); x != range.cols().end(); ++x, ppt += 2)
						{
							const Vertex2D &pt0 = *(ppt + 1);
							const Vertex2D &pt1 = *(ppt + size.width);
							const Vertex2D &pt2 = *(ppt + size.width + 2);
							const Vertex2D &pt3 = *(ppt + size.width * 2 + 1);
							Vertex2D &pt = *(ppt + size.width + 1);

							pt.x = (pt0.x + pt1.x + pt2.x + pt3.x) / 4;
							pt.y = (pt0.y + pt1.y + pt2.y + pt3.y) / 4;
						}
					}
				}
			};

			struct Average1
			{
				const cv::Size &size;
				const Vertex2Ds &oldVertices;
				Vertex2Ds &newVertices;

				Average1(const cv::Size &size, const Vertex2Ds &oldVertices, Vertex2Ds &newVertices)
					: size(size), oldVertices(oldVertices), newVertices(newVertices)
				{
				}

				void operator()(tbb::blocked_range<int> &range) const
				{
					int lastLine = (size.height - 1) * size.width;
					Vertex2D *ppt_ = &newVertices[range.begin()];
					const Vertex2D *ppt1_ = &oldVertices[range.begin()];
					for (int x = range.begin(); x < range.end(); ++x, ++ppt_, ++ppt1_)
					{
						{
							Vertex2D &pt = *(ppt_ + 1);
							if (pt.flags & Traits::cease)
								pt = *(ppt1_ + 1);
							else
							{
								const Vertex2D &pt0 = *(ppt1_ + size.width + 1);
								const Vertex2D &pt1 = *ppt1_;
								const Vertex2D &pt2 = *(ppt1_ + 2);
								const Vertex2D &pt3 = pt0;

								pt.x = (pt1.x + pt2.x) / 2;
								pt.y = (pt1.y + pt2.y) / 2;
							}
						}
						{
							Vertex2D &pt = *(ppt_ + lastLine + 1);
							if (pt.flags & Traits::cease)
								pt = *(ppt1_ + lastLine + 1);
							else
							{
								const Vertex2D &pt0 = *(ppt1_ + lastLine - size.width + 1);
								const Vertex2D &pt1 = *(ppt1_ + lastLine);
								const Vertex2D &pt2 = *(ppt1_ + lastLine + 2);
								const Vertex2D &pt3 = pt0;

								pt.x = (pt1.x + pt2.x) / 2;
								pt.y = (pt1.y + pt2.y) / 2;
							}
						}
					}
				}
			};

			struct Average2
			{
				const cv::Size &size;
				const Vertex2Ds &oldVertices;
				Vertex2Ds &newVertices;

				Average2(const cv::Size &size, const Vertex2Ds &oldVertices, Vertex2Ds &newVertices)
					: size(size), oldVertices(oldVertices), newVertices(newVertices)
				{
				}

				void operator()(tbb::blocked_range<int> &range) const
				{
					int begin = range.begin() * size.width;
					Vertex2D *ppt_ = &newVertices[begin];
					const Vertex2D *ppt1_ = &oldVertices[begin];
					for (int y = range.begin(); y < range.end(); ++y, ppt_ += size.width, ppt1_ += size.width)
					{
						{
							Vertex2D &pt = *(ppt_ + size.width);
							if (pt.flags & Traits::cease)
								pt = *(ppt1_ + size.width);
							else
							{
								const Vertex2D &pt0 = *(ppt1_ + size.width + 1);
								const Vertex2D &pt1 = *ppt1_;
								const Vertex2D &pt2 = *(ppt1_ + size.width * 2);
								const Vertex2D &pt3 = pt0;

								pt.x = (pt1.x + pt2.x) / 2;
								pt.y = (pt1.y + pt2.y) / 2;
							}
						}
						{
							Vertex2D &pt = *(ppt_ + size.width + size.width - 1);
							if (pt.flags & Traits::cease)
								pt = *(ppt1_ + size.width + size.width - 1);
							else
							{
								const Vertex2D &pt0 = *(ppt1_ + size.width + size.width - 1 - 1);
								const Vertex2D &pt1 = *(ppt1_ + size.width - 1);
								const Vertex2D &pt2 = *(ppt1_ + size.width * 2 + size.width - 1);
								const Vertex2D &pt3 = pt0;

								pt.x = (pt1.x + pt2.x) / 2;
								pt.y = (pt1.y + pt2.y) / 2;
							}
						}
					}
				}
			};

			struct Average3
			{
				const cv::Size &size;
				const Vertex2Ds &oldVertices;
				Vertex2Ds &newVertices;

				Average3(const cv::Size &size, const Vertex2Ds &oldVertices, Vertex2Ds &newVertices)
					: size(size), oldVertices(oldVertices), newVertices(newVertices)
				{
				}

				void operator()(tbb::blocked_range2d<int, int> &range) const
				{
					int begin = range.rows().begin() * size.width + range.cols().begin();
					Vertex2D *ppt_ = &newVertices[begin];
					const Vertex2D *ppt1_ = &oldVertices[begin];
					for (int y = range.rows().begin(); y != range.rows().end(); ++y, ppt_ += size.width, ppt1_ += size.width)
					{
						Vertex2D *ppt = ppt_;
						const Vertex2D *ppt1 = ppt1_;
						for (int x = range.cols().begin(); x != range.cols().end(); ++x, ++ppt, ++ppt1)
						{
							Vertex2D &pt = *(ppt + size.width + 1);
							if (pt.flags & Traits::cease)
								pt = *(ppt1 + size.width + 1);
							else
							{
								const Vertex2D &pt0 = *(ppt1 + 1);
								const Vertex2D &pt1 = *(ppt1 + size.width);
								const Vertex2D &pt2 = *(ppt1 + size.width + 2);
								const Vertex2D &pt3 = *(ppt1 + size.width * 2 + 1);

								pt.x = (pt0.x + pt1.x + pt2.x + pt3.x) / 4;
								pt.y = (pt0.y + pt1.y + pt2.y + pt3.y) / 4;
							}
						}
					}
				}
			};
		};

		template<class Iter> void genEdgeMatte(const float &blending, const float &gamma, const float &center, Iter begin, Iter end)
		{
			float gammaInv = 1 / gamma;
			int len = end - begin;
			float len1 = len - 1;
			Iter p = begin;
			for (int x = 0; x < len / 2; x++, p++)
			{
				float v = float(x) / len1;
				*p = powf(center * powf(2 * v, blending), gammaInv);
			}
			for (int x = len / 2; x < len; x++, p++)
			{
				float v = float(x) / len1;
				*p = powf(1 - (1 - center) * powf(2 * (1 - v), blending), gammaInv);
			}
		}

	} // namespace gfx
} // namespace zzlab

#endif // __ZZLAB_GFX_H__