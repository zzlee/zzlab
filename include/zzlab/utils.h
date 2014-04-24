#ifndef __ZZLAB_UTILS_H__
#define __ZZLAB_UTILS_H__

#ifdef ZZUTILS_EXPORTS
#define ZZUTILS_API __declspec(dllexport)
#else
#define ZZUTILS_API __declspec(dllimport)
#endif

#include "zzlab.h"

extern "C"
{
#include <libavutil/rational.h>
}

#include <queue>

#include <boost/thread/mutex.hpp>
#include <boost/asio/deadline_timer.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/weak_ptr.hpp>

#include <tbb/tbb.h>

namespace zzlab
{
	namespace utils
	{
		ZZUTILS_API void install(void);

		class ZZUTILS_API Timer
		{
		public:
			static const int64_t timeUnit;
			static const double fTimeUnit;
			static const AVRational rTimeUnitQ;

			explicit Timer();
			virtual ~Timer();

			virtual int64_t getTime() = 0;

			double getTimeInSeconds()
			{
				return getTime() / fTimeUnit;
			}

			int64_t getTimeInMicroseconds()
			{
				return getTime() * 10;
			}
		};

		class ZZUTILS_API HiPerfTimer : public Timer
		{
		public:
			explicit HiPerfTimer();
			~HiPerfTimer();

			void reset();
			int64_t getTime();

		protected:
			int64_t mBegin;
			int64_t mNow;

			int64_t mTime;
		};

		class ZZUTILS_API ManualTimer : public Timer
		{
		public:
			int64_t now;

			explicit ManualTimer();
			~ManualTimer();

			int64_t getTime();
		};

		// NOTICE: MUST use this class in _MainService
		class ZZUTILS_API RealTimeManager
		{
		public:
			explicit RealTimeManager(Timer& timeSource);
			~RealTimeManager();

			template<class T>
			void expires_at(int64_t t, T cb)
			{
				if (mTaskQueue.empty())
				{
					mTimer.expires_from_now(posix_time::milliseconds(2));
					mTimer.async_wait(bind(&RealTimeManager::schedule, this, asio::placeholders::error));
				}

				mTaskQueue.push(new Task(t, cb));
			}

		protected:
			boost::asio::deadline_timer mTimer;
			Timer& mTimeSource;

			struct Task
			{
				int64_t t;
				boost::function<void()> cb;

				Task(int64_t t, boost::function<void()> cb) : t(t), cb(cb)
				{}
			};

			struct greater_t : std::binary_function <Task*, Task*, bool>
			{
				bool operator() (Task* x, Task* y) const
				{
					return x->t > y->t;
				}
			};

			typedef std::priority_queue<Task*, std::deque<Task*>, greater_t> QueueImpl;
			QueueImpl mTaskQueue;

			void schedule(boost::system::error_code err = boost::system::error_code());
		};

		template<class T>
		class Delegate : protected boost::shared_ptr<boost::function<T> >
		{
		public:
			typedef boost::function<T> function_t;

			explicit Delegate()
			{
			}

			template<class T1>
			void connect(const T1& arg1)
			{
				reset(new function_t(boost::bind(arg1)));
			}

			template<class T1, class T2>
			void connect(const T1& arg1, const T2& arg2)
			{
				reset(new function_t(boost::bind(arg1, arg2)));
			}

			template<class T1, class T2, class T3>
			void connect(const T1& arg1, const T2& arg2, const T3& arg3)
			{
				reset(new function_t(boost::bind(arg1, arg2, arg3)));
			}

			template<class T1, class T2, class T3, class T4>
			void connect(const T1& arg1, const T2& arg2, const T3& arg3, const T4& arg4)
			{
				reset(new function_t(boost::bind(arg1, arg2, arg3, arg4)));
			}

			template<class T1, class T2, class T3, class T4, class T5>
			void connect(const T1& arg1, const T2& arg2, const T3& arg3, const T4& arg4, const T5& arg5)
			{
				reset(new function_t(boost::bind(arg1, arg2, arg3, arg4, arg5)));
			}

			template<class T1, class T2, class T3, class T4, class T5, class T6>
			void connect(const T1& arg1, const T2& arg2, const T3& arg3, const T4& arg4, const T5& arg5, const T6& arg6)
			{
				reset(new function_t(boost::bind(arg1, arg2, arg3, arg4, arg5, arg6)));
			}

			void cancel()
			{
				reset();
			}

			function_t operator()()
			{
				return function_t(WeakRef(*this));
			}

		protected:
			struct WeakRef : public boost::weak_ptr<function_t>
			{
				explicit WeakRef(const boost::shared_ptr<function_t>& ptr) : boost::weak_ptr<function_t>(ptr)
				{

				}

				void operator()()
				{
					if (boost::shared_ptr<function_t> t = lock())
					{
						(*t)();
					}
				}

				template<class T1>
				void operator()(T1 arg1)
				{
					if (boost::shared_ptr<function_t> t = lock())
					{
						(*t)(arg1);
					}
				}

				template<class T1, class T2>
				void operator()(T1 arg1, T2 arg2)
				{
					if (boost::shared_ptr<function_t> t = lock())
					{
						(*t)(arg1, arg2);
					}
				}

				template<class T1, class T2, class T3>
				void operator()(T1 arg1, T2 arg2, T3 arg3)
				{
					if (boost::shared_ptr<function_t> t = lock())
					{
						(*t)(arg1, arg2, arg3);
					}
				}

				template<class T1, class T2, class T3, class T4>
				void operator()(T1 arg1, T2 arg2, T3 arg3, T4 arg4)
				{
					if (boost::shared_ptr<function_t> t = lock())
					{
						(*t)(arg1, arg2, arg3, arg4);
					}
				}

				template<class T1, class T2, class T3, class T4, class T5>
				void operator()(T1 arg1, T2 arg2, T3 arg3, T4 arg4, T5 arg5)
				{
					if (boost::shared_ptr<function_t> t = lock())
					{
						(*t)(arg1, arg2, arg3, arg4, arg5);
					}
				}

				template<class T1, class T2, class T3, class T4, class T5, class T6>
				void operator()(T1 arg1, T2 arg2, T3 arg3, T4 arg4, T5 arg5, T6 arg6)
				{
					if (boost::shared_ptr<function_t> t = lock())
					{
						(*t)(arg1, arg2, arg3, arg4, arg5, arg6);
					}
				}
			};
		};
		typedef Delegate<void()> Delegate0;

		template<class T>
		class AsyncEvents
		{
		public:
			typedef T callback_t;

			explicit AsyncEvents()
			{
				mCallback[0] = new std::queue<callback_t>();
				mCallback[1] = new std::queue<callback_t>();
			}

			~AsyncEvents()
			{
				delete mCallback[0];
				delete mCallback[1];
			}

			void enqueue(T cb)
			{
				boost::mutex::scoped_lock lock(mMutex);

				mCallback[1]->push(cb);
			}

			void invoke()
			{
				swapEvents();

				while (!mCallback[0]->empty())
				{
					mCallback[0]->front()();
					mCallback[0]->pop();
				}
			}

			template<class T1>
			void invoke(T1 arg1)
			{
				swapEvents();

				while (!mCallback[0]->empty())
				{
					mCallback[0]->front()(arg1);
					mCallback[0]->pop();
				}
			}

			template<class T1, class T2>
			void invoke(T1 arg1, T2 arg2)
			{
				swapEvents();

				while (!mCallback[0]->empty())
				{
					mCallback[0]->front()(arg1, arg2);
					mCallback[0]->pop();
				}
			}

			template<class T1, class T2, class T3>
			void invoke(T1 arg1, T2 arg2, T3 arg3)
			{
				swapEvents();

				while (!mCallback[0]->empty())
				{
					mCallback[0]->front()(arg1, arg2, arg3);
					mCallback[0]->pop();
				}
			}

			template<class T1, class T2, class T3, class T4>
			void invoke(T1 arg1, T2 arg2, T3 arg3, T4 arg4)
			{
				swapEvents();

				while (!mCallback[0]->empty())
				{
					mCallback[0]->front()(arg1, arg2, arg3, arg4);
					mCallback[0]->pop();
				}
			}

			template<class T1, class T2, class T3, class T4, class T5>
			void invoke(T1 arg1, T2 arg2, T3 arg3, T4 arg4, T5 arg5)
			{
				swapEvents();

				while (!mCallback[0]->empty())
				{
					mCallback[0]->front()(arg1, arg2, arg3, arg4, arg5);
					mCallback[0]->pop();
				}
			}

			template<class T1, class T2, class T3, class T4, class T5, class T6>
			void invoke(T1 arg1, T2 arg2, T3 arg3, T4 arg4, T5 arg5, T6 arg6)
			{
				swapEvents();

				while (!mCallback[0]->empty())
				{
					mCallback[0]->front()(arg1, arg2, arg3, arg4, arg5, arg6);
					mCallback[0]->pop();
				}
			}

		protected:
			boost::mutex mMutex;
			std::queue<callback_t> *mCallback[2];

			void swapEvents()
			{
				boost::mutex::scoped_lock lock(mMutex);

				std::swap(mCallback[0], mCallback[1]);
			}
		};

		typedef AsyncEvents<boost::function<void()> > AsyncEvents0;

		struct Parallel
		{
			template<class Range, class Body> static void For(Range range, Body body)
			{
				tbb::parallel_for(range, body);
			}
		};

		struct Sequential
		{
			template<class Range, class Body> static void For(Range range, Body body)
			{
				body(range);
			}
		};
	}
}

#endif // __ZZLAB_UTILS_H__