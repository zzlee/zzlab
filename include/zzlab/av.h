#ifndef __ZZLAB_AV_H__
#define __ZZLAB_AV_H__

#ifdef ZZAV_EXPORTS
#define ZZAV_API __declspec(dllexport)
#else
#define ZZAV_API __declspec(dllimport)
#endif

#include "zzlab.h"
#include "zzlab/utils.h"
#include "zzlab/gfx.h"

#include <queue>

#include <boost/function.hpp>
#include <boost/filesystem.hpp>
#include <boost/thread.hpp>
#include <boost/atomic.hpp>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

#include <opencv2/opencv.hpp>
#include <portaudio.h>

#include <comdef.h>
#include <dshow.h>
#include <dvdmedia.h>
#include <wmsdkidl.h>

#import "qedit.dll"

namespace zzlab
{
	namespace av
	{
		using DexterLib::ISampleGrabberPtr;
		using DexterLib::ISampleGrabber;
		using DexterLib::ISampleGrabberCB;

		_COM_SMARTPTR_TYPEDEF(IMoniker, __uuidof(IMoniker));
		_COM_SMARTPTR_TYPEDEF(IEnumMoniker, __uuidof(IEnumMoniker));
		_COM_SMARTPTR_TYPEDEF(IPropertyBag, __uuidof(IPropertyBag));
		_COM_SMARTPTR_TYPEDEF(IRunningObjectTable, __uuidof(IRunningObjectTable));

		_COM_SMARTPTR_TYPEDEF(ICreateDevEnum, __uuidof(ICreateDevEnum));
		_COM_SMARTPTR_TYPEDEF(IAMStreamConfig, __uuidof(IAMStreamConfig));
		_COM_SMARTPTR_TYPEDEF(ICaptureGraphBuilder2, __uuidof(ICaptureGraphBuilder2));
		_COM_SMARTPTR_TYPEDEF(IBaseFilter, __uuidof(IBaseFilter));

		_COM_SMARTPTR_TYPEDEF(IGraphBuilder, __uuidof(IGraphBuilder));
		_COM_SMARTPTR_TYPEDEF(ICaptureGraphBuilder2, __uuidof(ICaptureGraphBuilder2));
		_COM_SMARTPTR_TYPEDEF(IMediaFilter, __uuidof(IMediaFilter));
		_COM_SMARTPTR_TYPEDEF(IMediaControl, __uuidof(IMediaControl));

		typedef std::vector<IMonikerPtr> monikers_t;
		typedef std::pair<AM_MEDIA_TYPE*, VIDEO_STREAM_CONFIG_CAPS> stream_cap;
		typedef std::vector<stream_cap> stream_caps_t;

		ZZAV_API void install(void);

		class ZZAV_API FileDemuxer
		{
		public:
			explicit FileDemuxer();
			~FileDemuxer();

			AVFormatContext* init(boost::filesystem::wpath path);

			void dumpInfo()
			{
				av_dump_format(mFormatCtx, 0, mFormatCtx->filename, 0);
			}

			bool read(AVPacket* pkt)
			{
				return (av_read_frame(mFormatCtx, pkt) >= 0);
			}

			AVFormatContext* getContext() const
			{
				return mFormatCtx;
			}

		protected:
			AVFormatContext *mFormatCtx;
		};

		class ZZAV_API Decoder
		{
		public:
			explicit Decoder();
			~Decoder();

			AVCodecContext* init(AVCodecContext* codexCtx, AVDictionary* opts = NULL);
			bool decode(AVPacket* pkt, AVFrame* frame);

			AVCodecContext* getContext() const
			{
				return mCodecCtx;
			}

		protected:
			AVCodecContext *mCodecCtx;

			boost::function<int(AVCodecContext*, AVFrame*, int*, const AVPacket*)> mDecode;
		};

		template<class T>
		struct AudioMix
		{
			unsigned int channels;
			size_t blockSize;
			explicit AudioMix(unsigned int channels) :
				channels(channels),
				blockSize(channels *sizeof(T))
			{
			}

			size_t operator()(uint8_t *src, uint8_t *dst, unsigned long samples)
			{
				typedef cv::Mat_<T> mat_t;

				mat_t(samples, channels, (T *)dst, blockSize) +=
					mat_t(samples, channels, (T *)src, blockSize);

				return samples * blockSize;
			}
		};

		template<class T>
		struct AudioClear
		{
			size_t blockSize;
			explicit AudioClear(unsigned int channels) :
				blockSize(channels *sizeof(T))
			{
			}

			void operator()(uint8_t *dst, unsigned long samples)
			{
				memset(dst, 0, samples * blockSize);
			}
		};

		typedef boost::function<void(uint8_t *, unsigned long)> clear_audio_t;
		typedef boost::function<size_t(uint8_t *, uint8_t *, unsigned long)> mix_audio_t;

		ZZAV_API AVSampleFormat cvtSampleFormat(PaSampleFormat fmt);
		ZZAV_API PaSampleFormat cvtSampleFormat(AVSampleFormat fmt);

		class ZZAV_API AudioDevice
		{
		public:
			PaStreamParameters inputParameters;
			PaStreamParameters outputParameters;
			double sampleRate;
			unsigned long framesPerBuffer;
			PaStreamFlags streamFlags;

			explicit AudioDevice();
			virtual ~AudioDevice();

			static void dumpInfo();

			template<class T>
			void waitForBuffer(T cb)
			{
				mEvents.enqueue(cb);
			}

			bool load(XmlNode* node);
			void init();

			PaStream *getStream() const
			{
				return mStream;
			}

			PaTime getStreamTime() const
			{
				return Pa_GetStreamTime(mStream);
			}

			void start();
			void stop();

			// audio mixer functor
			mix_audio_t mixer;

		protected:
			zzlab::utils::AsyncEvents < boost::function < void(
				const uint8_t *, uint8_t *, unsigned long, const PaStreamCallbackTimeInfo*) > > mEvents;

			PaStream *mStream;
			clear_audio_t mClear;

			static int onDataArrived(const uint8_t *input, 
				uint8_t *output, unsigned long frameCount,
				const PaStreamCallbackTimeInfo *timeInfo,
				PaStreamCallbackFlags statusFlags, AudioDevice *pThis);
			int onDataArrived0(const uint8_t *input,
				uint8_t *output, unsigned long frameCount,
				const PaStreamCallbackTimeInfo *timeInfo,
				PaStreamCallbackFlags statusFlags);
		};

		class ZZAV_API FrameQueue
		{
		public:
			size_t maxSize;
			int64_t dropTolerance;

			explicit FrameQueue();
			virtual ~FrameQueue();

			size_t size()
			{
				boost::mutex::scoped_lock l(mQueueMutex);

				return mQueue.size();
			}

			bool empty()
			{
				boost::mutex::scoped_lock l(mQueueMutex);

				return mQueue.empty();
			}

			bool enqueue(AVFrame* val);
			size_t dequeue(int64_t now, AVFrame*& val);

		protected:
			struct pts_greater_t : std::binary_function <AVFrame*, AVFrame*, bool>
			{
				bool operator() (AVFrame* x, AVFrame* y) const
				{
					return x->pts > y->pts;
				}
			};

			typedef std::priority_queue<AVFrame*, std::deque<AVFrame*>, pts_greater_t> QueueImpl;

			boost::mutex mQueueMutex;
			QueueImpl mQueue;
		};

		class ZZAV_API AudioResampler
		{
		public:
			uint64_t inChannelLayout;
			int inSampleRate;
			AVSampleFormat inFormat;

			uint64_t outChannelLayout;
			int outSampleRate;
			AVSampleFormat outFormat;

			explicit AudioResampler();
			virtual ~AudioResampler();

			void init();

			AVFrame* convert(AVFrame* src);

		protected:
			SwrContext *mSwrCtx;
		};

		class ZZAV_API FileMediaPlayer
		{
		public:
			boost::filesystem::wpath source;
			bool dumpInfo;
			bool loopPlay;

			// for video
			size_t videoDecoderThreads;
			size_t videoQueueSize;
			int64_t videoDropTolerance;

			// for audio
			size_t audioQueueSize;
			int64_t audioDropTolerance;
			uint64_t outChannelLayout;
			int outSampleRate;
			AVSampleFormat outFormat;

			explicit FileMediaPlayer();
			~FileMediaPlayer();
						
			void load(XmlNode* node, AudioDevice* audioDevice);
			void init();
			void play(boost::function<void()> endOfStream);
			void stop(boost::function<void()> afterStop);

			bool isPlaying() const
			{
				return mPlaying.load(boost::memory_order_acquire) != 0;
			}

			bool isEndOfStream()
			{
				return mPlaying.load(boost::memory_order_acquire) == -1;
			}

			size_t getStreams() const
			{
				return mDemuxer.getContext()->nb_streams;
			}

			AVStream* getStream(size_t idx) const
			{
				return mDemuxer.getContext()->streams[idx];
			}

			zzlab::av::FrameQueue* getFrameQueue(size_t idx) const
			{
				StreamHandler* h = mStreamHandlers[idx];
				if (h)
					return &(h->q);
				else
					return NULL;
			}

		protected:
			boost::asio::deadline_timer mTimer;
			zzlab::av::FileDemuxer mDemuxer;
			AVPacket mPacket;
			AVFrame* mFrame;
			boost::atomic<int> mPlaying;

			boost::function<void()> mEndOfStream;
			boost::function<void()> mAfterStop;
			boost::atomic<bool> mStopping;

			struct StreamHandler
			{
				AVRational base;
				zzlab::av::Decoder d;
				zzlab::av::FrameQueue q;
				int64_t pts;
				int64_t offset;

				StreamHandler();
				virtual ~StreamHandler();

				AVFrame* handle(AVPacket* pkt, AVFrame* frame);
				virtual AVFrame* process(AVFrame* frame);
			};

			struct AudioStreamHandler : public StreamHandler
			{
				zzlab::av::AudioResampler r;

				AudioStreamHandler();
				virtual ~AudioStreamHandler();

				virtual AVFrame* process(AVFrame* frame);
			};

			std::vector<StreamHandler*> mStreamHandlers;

			AVFrame* mFrameToEnqueue;
			StreamHandler* mHandler;

			void readPacket(boost::system::error_code err = boost::system::error_code());

			size_t mCurrentIndex;
			void handleDelayedFrames(boost::system::error_code err = boost::system::error_code());
			void handleAfterEndOfStream(boost::system::error_code err = boost::system::error_code());
		};

		class ZZAV_API NullRenderer
		{
		public:
			zzlab::utils::Timer* timeSource;
			zzlab::av::FrameQueue* source;
			float renderRate;

			explicit NullRenderer();
			~NullRenderer();

			void play(int64_t playTime);
			void stop(boost::function<void()> afterStop);

			bool isPlaying() const
			{
				return mPlaying.load(boost::memory_order_acquire);
			}

		protected:
			boost::asio::deadline_timer mTimer;
			size_t mDuration; // in milliseconds
			int64_t mPlayTime;
			boost::atomic<bool> mPlaying;

			boost::atomic<bool> mStopping;
			boost::function<void()> mAfterStop;

			void render(boost::system::error_code err = boost::system::error_code());
		};

		class ZZAV_API AudioQueueRenderer
		{
		public:
			utils::Timer* timeSource;
			zzlab::av::FrameQueue* source;
			zzlab::av::AudioDevice* device;

			explicit AudioQueueRenderer();
			~AudioQueueRenderer();

			void play(int64_t t);
			void stop(boost::function<void()> afterStop);

			bool isPlaying() const
			{
				return mPlaying.load(boost::memory_order_acquire);
			}

		protected:
			int64_t mPlayTime;
			double mTimeOffset;
			AVFrame* mAudioFrame;
			int mAudioSamples;
			uint8_t* mAudioData;
			void renderAudio(const uint8_t *input, uint8_t *output, 
				unsigned long frameCount, const PaStreamCallbackTimeInfo* timeInfo);
			void stopRenderAudio();

			boost::atomic<bool> mPlaying;
			boost::function<void()> mAfterStop;

			// NOTICE: accessed only in the thread of audio device!!
			bool mStopRendering;
		};

		class ZZAV_API VideoQueueRenderer
		{
		public:
			utils::Timer* timeSource;
			av::FrameQueue* source;
			gfx::RendererEvents* rendererEvents;

			// events
			boost::function<void(AVFrame*)> renderFrame;

			explicit VideoQueueRenderer();
			virtual ~VideoQueueRenderer();

			void play(int64_t playTime);
			void stop(boost::function<void()> afterStop);

			bool isPlaying() const
			{
				return mPlaying;
			}

		protected:
			int64_t mPlayTime;
			bool mPlaying;
			bool mStopping;
			boost::function<void()> mAfterStop;

			void renderVideo();
		};

		class ZZAV_API SimpleMediaPlayer : public FileMediaPlayer
		{
		public:
			utils::Timer* timeSource;
			AudioDevice* audioDevice;

			explicit SimpleMediaPlayer();
			virtual ~SimpleMediaPlayer();

			void load(XmlNode* node)
			{
				FileMediaPlayer::load(node, audioDevice);
			}

			void init();
			void play(int64_t playTime, boost::function<void()> endOfStream);
			void stop(boost::function<void()> afterStop);

			AVCodecContext* getVideoCodecContext() const
			{
				return mVideoCodecCtx;
			}

			VideoQueueRenderer* getVideoRenderer() const
			{
				return mVideoRenderer;
			}

		protected:
			AudioQueueRenderer* mAudioRenderer;
			VideoQueueRenderer* mVideoRenderer;
			AVCodecContext* mVideoCodecCtx;
			boost::function<void()> mAfterStop;

			size_t mStopCount;
			void handleStop();
		};

		template<class T1, class T2>
		void enumerateAll(T1 e, std::vector<T2>& ret)
		{
			e->Reset();
			ULONG cFetched;
			T2 item;
			while (true)
			{
				HRESULT hr = e->Next(1, &item, &cFetched);
				if (hr != S_OK) break;

				ret.push_back(item);
			}
		}

		ZZAV_API void enumerateDevices(const CLSID& clsid, monikers_t& monikers);
		ZZAV_API _bstr_t getFriendlyName(const IMonikerPtr& moniker);
		ZZAV_API void dumpMonikerFriendlyNames(const monikers_t& mks);
		ZZAV_API AM_MEDIA_TYPE* allocMediaType();
		ZZAV_API void freeMediaType(AM_MEDIA_TYPE& mt);
		ZZAV_API void deleteMediaType(AM_MEDIA_TYPE *pmt);
		ZZAV_API void deleteStreamCaps(stream_caps_t& caps);
		ZZAV_API AM_MEDIA_TYPE* getMediaType(const stream_caps_t& caps, int width, int height, const GUID& subtype);
		ZZAV_API void dumpMediaType(AM_MEDIA_TYPE* mt);

		class ZZAV_API Scaler
		{
		public:
			int srcW;
			int srcH;
			PixelFormat srcFormat;

			int dstW;
			int dstH;
			PixelFormat dstFormat;

			int flags;

			explicit Scaler();
			virtual ~Scaler();

			void init()
			{
				mCtx = sws_getContext(srcW, srcH, srcFormat, dstW, dstH, dstFormat, flags, NULL, NULL, NULL);
			}

			void scale(const uint8_t *const srcSlice[], const int srcStride[], int srcSliceY, int srcSliceH, uint8_t *const dst[], const int dstStride[])
			{
				sws_scale(mCtx, srcSlice, srcStride, srcSliceY, srcSliceH, dst, dstStride);
			}

			void scale(const uint8_t *const srcSlice[], const int srcStride[], int srcSliceH, uint8_t *const dst[], const int dstStride[])
			{
				scale(srcSlice, srcStride, 0, srcSliceH, dst, dstStride);
			}

			void scale(const uint8_t *const srcSlice[], const int srcStride[], uint8_t *const dst[], const int dstStride[])
			{
				scale(srcSlice, srcStride, 0, srcH, dst, dstStride);
			}

		protected:
			SwsContext* mCtx;
		};

		class ZZAV_API VideoCap : protected ISampleGrabberCB
		{
		public:
			// event
			boost::function<void(double, IMediaSample*)> onFrame;

			explicit VideoCap();
			virtual ~VideoCap();

			void init(IMonikerPtr moniker);
			void getStreamCaps(stream_caps_t& ret);
			void setFormat(int width, int height, const GUID& subtype);
			void setFormat(const stream_caps_t& caps, int width, int height, const GUID& subtype);
			void setFormat(AM_MEDIA_TYPE* mt);
			void setNullSyncSource();
			void render();
			AM_MEDIA_TYPE* getConnectedMediaType();
			void dumpConnectedMediaType();
			void start();
			void stop();

		protected:
			IBindCtxPtr mBindCtx;
			ICaptureGraphBuilder2Ptr mCaptureGraph;
			IBaseFilterPtr mSourceFilter;
			IBaseFilterPtr mSampleGrabberFilter;
			IBaseFilterPtr mRendererFilter;
			IGraphBuilderPtr mGraph;

			HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void ** ppvObject);
			ULONG STDMETHODCALLTYPE AddRef(void);
			ULONG STDMETHODCALLTYPE Release(void);

			HRESULT STDMETHODCALLTYPE raw_SampleCB(double SampleTime, DexterLib::IMediaSample *pSample);
			HRESULT STDMETHODCALLTYPE raw_BufferCB(double SampleTime, BYTE *pBuffer, long BufferLen);
		};

	} // namespace av
} // namespace zzlab

#endif // __ZZLAB_AV_H__
