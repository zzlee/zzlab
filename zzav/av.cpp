// zzav.cpp : 定義 DLL 應用程式的匯出函式。
//

#include "stdafx.h"
#include "zzlab.h"
#include "zzlab/av.h"
#include "zzlab/utils.h"

#include <boost/bind.hpp>
#include <boost/locale.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/asio/placeholders.hpp>

extern "C"
{
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
}

ZZLAB_USE_LOG4CPLUS(zzav);

using namespace boost;

namespace zzlab
{
	namespace av
	{
		static void dummy() {};

		static bool initialize()
		{
			ZZLAB_TRACE_FUNCTION();

			ZZLAB_INFO("Register FFmpeg library");
			av_register_all();

			ZZLAB_INFO("Initialize PortAudio");
			PaError err = Pa_Initialize();
			if (err != paNoError)
				ZZLAB_ERROR(L"Failed to initialize PortAudio, " << Pa_GetErrorText(err));

			return true;
		}

		static void uninitialize()
		{
			ZZLAB_TRACE_FUNCTION();

			ZZLAB_INFO("Terminate PortAudio");
			Pa_Terminate();
		}

		void install(void)
		{
			Plugin plugin = {
				L"zzav", initialize, uninitialize
			};

			addPlugin(plugin);
		}

		FileDemuxer::FileDemuxer() : mFormatCtx(NULL)
		{
			ZZLAB_TRACE_THIS();
		}

		FileDemuxer::~FileDemuxer()
		{
			ZZLAB_TRACE_THIS();

			if (mFormatCtx)
				avformat_close_input(&mFormatCtx);
		}

		AVFormatContext* FileDemuxer::init(boost::filesystem::wpath path)
		{
			if (mFormatCtx != NULL)
			{
				ZZLAB_ERROR(__FUNCTION__ << L": already initialized");
				return NULL;
			}

			AVFormatContext* ctx = NULL;
			if (avformat_open_input(&ctx, path.string().c_str(), NULL, NULL) < 0)
			{
				ZZLAB_ERROR(L"Failed to open " << path.wstring());

				return NULL;
			}

			if (avformat_find_stream_info(ctx, NULL) < 0)
			{
				avformat_close_input(&ctx);

				ZZLAB_ERROR(L"Failed to find stream info from " << path.wstring());

				return NULL;
			}

			mFormatCtx = ctx;

			return mFormatCtx;
		}

		Decoder::Decoder() : mCodecCtx(NULL)
		{
			ZZLAB_TRACE_THIS();
		}

		Decoder::~Decoder()
		{
			ZZLAB_TRACE_THIS();

			if (mCodecCtx)
				avcodec_close(mCodecCtx);
		}

		AVCodecContext* Decoder::init(AVCodecContext* codecCtx, AVDictionary* opts)
		{
			if (mCodecCtx != NULL)
			{
				ZZLAB_ERROR(__FUNCTION__ << L": already initialized");
				return NULL;
			}

			AVMediaType type = codecCtx->codec_type;
			if (!(type == AVMEDIA_TYPE_VIDEO || type == AVMEDIA_TYPE_AUDIO))
			{
				ZZLAB_ERROR(__FUNCTION__ << L": unsupported codec type, " << type);
				return NULL;
			}

			AVCodec *codec = avcodec_find_decoder(codecCtx->codec_id);
			if (codec == NULL)
			{
				ZZLAB_ERROR(L"Failed to find decoder, codec_id=" << codecCtx->codec_id);
				return NULL;
			}

			av_dict_set(&opts, "refcounted_frames", "1", 0);
			if (avcodec_open2(codecCtx, codec, &opts) < 0)
			{
				ZZLAB_ERROR(L"Failed to open decoder, codec_id=" << codecCtx->codec_id);
				return NULL;
			}

			switch (type)
			{
			case AVMEDIA_TYPE_VIDEO:
				mDecode = avcodec_decode_video2;
				break;

			case AVMEDIA_TYPE_AUDIO:
				mDecode = avcodec_decode_audio4;
				break;
			}

			mCodecCtx = codecCtx;

			return mCodecCtx;
		}

		bool Decoder::decode(AVPacket* pkt, AVFrame* frame)
		{
			int gotFrame;
			if (mDecode(mCodecCtx, frame, &gotFrame, pkt) < 0)
			{
				ZZLAB_ERROR(L"Failed to decode packet");

				return false;
			}

			return gotFrame != 0;
		}

		FrameQueue::FrameQueue() :
			maxSize(16),
			dropTolerance(utils::Timer::timeUnit)
		{
			ZZLAB_TRACE_THIS();
		}

		FrameQueue::~FrameQueue()
		{
			ZZLAB_TRACE_THIS();

			ZZLAB_TRACE_VALUE(mQueue.size());
			while (!mQueue.empty())
			{
				AVFrame* frame = mQueue.top();
				av_frame_free(&frame);

				mQueue.pop();
			}
		}

		bool FrameQueue::enqueue(AVFrame* val)
		{
			mutex::scoped_lock l(mQueueMutex);

			if (mQueue.size() >= maxSize)
				return false;

			mQueue.push(val);

			return true;
		}

		size_t FrameQueue::dequeue(int64_t now, AVFrame*& val)
		{
			mutex::scoped_lock l(mQueueMutex);

			if (mQueue.empty() || mQueue.top()->pts > now)
			{
				val = NULL;
				return 0;
			}

			val = mQueue.top();

			// relax drop threshold
			now -= dropTolerance;

			// drop frame(s) if needed
			size_t dropped = 0;
			while (true)
			{
				mQueue.pop();

				if (mQueue.empty() || mQueue.top()->pts > now)
					break;

				av_frame_free(&val);
				++dropped;
				val = mQueue.top();
			}

			return dropped;
		}

		AudioResampler::AudioResampler() :
			mSwrCtx(NULL),
			inChannelLayout(AV_CH_LAYOUT_STEREO),
			inSampleRate(44100),
			inFormat(AV_SAMPLE_FMT_FLT),
			outChannelLayout(AV_CH_LAYOUT_STEREO),
			outSampleRate(44100),
			outFormat(AV_SAMPLE_FMT_FLT)
		{
			ZZLAB_TRACE_THIS();
		}

		AudioResampler::~AudioResampler()
		{
			ZZLAB_TRACE_THIS();

			swr_free(&mSwrCtx);
		}

		void AudioResampler::init()
		{
			ZZLAB_TRACE_THIS();

			mSwrCtx = swr_alloc();

			av_opt_set_int(mSwrCtx, "in_channel_layout", inChannelLayout, 0);
			av_opt_set_int(mSwrCtx, "in_sample_rate", inSampleRate, 0);
			av_opt_set_sample_fmt(mSwrCtx, "in_sample_fmt", inFormat, 0);

			av_opt_set_int(mSwrCtx, "out_channel_layout", outChannelLayout, 0);
			av_opt_set_int(mSwrCtx, "out_sample_rate", outSampleRate, 0);
			av_opt_set_sample_fmt(mSwrCtx, "out_sample_fmt", outFormat, 0);

			if (swr_init(mSwrCtx) < 0)
			{
				ZZLAB_ERROR("Failed to initialize SWR");
				swr_free(&mSwrCtx);
				return;
			}
		}

		AVFrame* AudioResampler::convert(AVFrame* src)
		{
			AVFrame* frame = av_frame_alloc();
			frame->nb_samples = (int)av_rescale_rnd(swr_get_delay(mSwrCtx, inSampleRate) +
				src->nb_samples, outSampleRate, inSampleRate, AV_ROUND_UP);
			frame->format = outFormat;
			frame->channel_layout = outChannelLayout;
			int ret = av_frame_get_buffer(frame, 1);
			if (ret < 0)
			{
				ZZLAB_ERROR("av_frame_get_buffer failed, ret=" << ret);
				av_frame_free(&frame);
				return NULL;
			}

			ret = swr_convert(mSwrCtx, frame->data, frame->nb_samples,
				(const uint8_t **)src->data, src->nb_samples);
			if (ret < 0)
			{
				ZZLAB_ERROR("swr_convert failed, ret=" << ret);
				av_frame_free(&frame);
				return NULL;
			}
			frame->nb_samples = ret;

			return frame;
		}

		FileMediaPlayer::FileMediaPlayer() :
			loopPlay(false),
			dumpInfo(true),
			videoDecoderThreads(0),
			videoQueueSize(16),
			audioQueueSize(16),
			outChannelLayout(AV_CH_LAYOUT_STEREO),
			outSampleRate(44100),
			outFormat(AV_SAMPLE_FMT_FLT),
			mFrameToEnqueue(NULL),
			mTimer(_WorkerService),
			mPlaying(0),
			mStopping(false)
		{
			ZZLAB_TRACE_THIS();

			av_init_packet(&mPacket);
			mFrame = av_frame_alloc();
		}

		FileMediaPlayer::~FileMediaPlayer()
		{
			ZZLAB_TRACE_THIS();

			if (isPlaying())
				ZZLAB_ERROR("MUST call stop first!!");

			av_free_packet(&mPacket);
			av_frame_free(&mFrame);
			av_frame_free(&mFrameToEnqueue);

			for (std::vector<StreamHandler*>::const_iterator i = mStreamHandlers.begin(); i != mStreamHandlers.end(); ++i)
			{
				if (*i)
					delete *i;
			}
		}

		void FileMediaPlayer::load(XmlNode* node, AudioDevice* audioDevice)
		{
			XmlAttribute* attr = node->first_attribute(L"video-decoder-threads");
			if (attr)
				videoDecoderThreads = _wtoi(attr->value());

			attr = node->first_attribute(L"video-queue-size");
			if (attr)
				videoQueueSize = _wtoi(attr->value());

			attr = node->first_attribute(L"video-drop-tolerance");
			if (attr)
				videoDropTolerance = _wtoi(attr->value()) * utils::Timer::timeUnit;

			outChannelLayout = av_get_default_channel_layout(audioDevice->outputParameters.channelCount);
			outSampleRate = (int)audioDevice->sampleRate;
			outFormat = cvtSampleFormat(audioDevice->outputParameters.sampleFormat);

			attr = node->first_attribute(L"audio-queue-size");
			if (attr)
				audioQueueSize = _wtoi(attr->value());

			attr = node->first_attribute(L"audio-drop-tolerance");
			if (attr)
				audioDropTolerance = _wtoi(attr->value()) * utils::Timer::timeUnit;
		}

		void FileMediaPlayer::init()
		{
			ZZLAB_TRACE_THIS();

			AVFormatContext* ctx = mDemuxer.init(source);
			if (!ctx)
			{
				ZZLAB_ERROR("mDemuxer.init failed");
				return;
			}

			if (dumpInfo)
				mDemuxer.dumpInfo();

			mStreamHandlers.resize(ctx->nb_streams);
			std::fill(mStreamHandlers.begin(), mStreamHandlers.end(), (StreamHandler*)NULL);

			for (size_t i = 0; i < ctx->nb_streams; ++i)
			{
				AVStream* stream = ctx->streams[i];
				if (stream->codec->codec_type == AVMEDIA_TYPE_VIDEO)
				{
					StreamHandler* handler = new StreamHandler;

					AVDictionary* opts = NULL;
					if (videoDecoderThreads > 0)
					{
						std::string t = lexical_cast<std::string>(videoDecoderThreads);
						av_dict_set(&opts, "threads", t.c_str(), 0);
					}

					if (handler->d.init(stream->codec, opts))
					{
						handler->base = stream->time_base;
						handler->q.maxSize = videoQueueSize;
						handler->q.dropTolerance = videoDropTolerance;

						mStreamHandlers[i] = handler;
					}
					else
						delete handler;
				}
				else if (stream->codec->codec_type == AVMEDIA_TYPE_AUDIO)
				{
					AudioStreamHandler* handler = new AudioStreamHandler;

					if (handler->d.init(stream->codec))
					{
						handler->base = stream->time_base;
						handler->q.maxSize = audioQueueSize;
						handler->q.dropTolerance = audioDropTolerance;

						handler->r.inChannelLayout = stream->codec->channel_layout;
						handler->r.inSampleRate = stream->codec->sample_rate;
						handler->r.inFormat = stream->codec->sample_fmt;
						handler->r.outChannelLayout = outChannelLayout;
						handler->r.outSampleRate = outSampleRate;
						handler->r.outFormat = outFormat;
						handler->r.init();

						mStreamHandlers[i] = handler;
					}
					else
						delete handler;
				}
				else
				{
					ZZLAB_WARN("Unsupported codec type " << stream->codec->codec_type << " at " << i << " channel");
				}
			}
		}

		void FileMediaPlayer::play(boost::function<void()> endOfStream)
		{
			if (mPlaying.exchange(true, memory_order_release) == 1)
			{
				ZZLAB_WARN("FileMediaPlayer is PLAYING");
				return;
			}

			mStopping.store(false, memory_order_release);
			mEndOfStream = endOfStream;
			readPacket();
		}

		void FileMediaPlayer::stop(boost::function<void()> afterStop)
		{
			if (!isPlaying())
			{
				ZZLAB_WARN("FileMediaPlayer is NOT playing");
				return;
			}

			mStopping.store(true, memory_order_release);
			mAfterStop = afterStop;
			mTimer.cancel();
		}

		FileMediaPlayer::StreamHandler::StreamHandler() : pts(0), offset(0)
		{
			ZZLAB_TRACE_THIS();
		}

		FileMediaPlayer::StreamHandler::~StreamHandler()
		{
			ZZLAB_TRACE_THIS();
		}

		AVFrame* FileMediaPlayer::StreamHandler::handle(AVPacket* pkt, AVFrame* frame)
		{
			if (!d.decode(pkt, frame))
				return NULL;

			AVFrame* out = process(frame);
			pts = av_rescale_q(
				av_frame_get_best_effort_timestamp(frame),
				base, utils::Timer::rTimeUnitQ);
			out->pts = pts + offset;

			return out;
		}

		AVFrame* FileMediaPlayer::StreamHandler::process(AVFrame* frame)
		{
			return av_frame_clone(frame);
		}

		FileMediaPlayer::AudioStreamHandler::AudioStreamHandler()
		{
			ZZLAB_TRACE_THIS();
		}

		FileMediaPlayer::AudioStreamHandler::~AudioStreamHandler()
		{
			ZZLAB_TRACE_THIS();
		}

		AVFrame* FileMediaPlayer::AudioStreamHandler::process(AVFrame* frame)
		{
			return r.convert(frame);
		}

		void FileMediaPlayer::readPacket(boost::system::error_code err)
		{
			if (err || mStopping.load(boost::memory_order_acquire))
			{
				mPlaying.store(0, memory_order_release);
				mAfterStop();

				return;
			}

			if (mFrameToEnqueue)
			{
				if (!mHandler->q.enqueue(mFrameToEnqueue))
				{
					mTimer.expires_from_now(posix_time::milliseconds(10));
					mTimer.async_wait(bind(&FileMediaPlayer::readPacket, this, asio::placeholders::error));
					return;
				}

				mFrameToEnqueue = NULL;
			}

			while (true)
			{
				if (!mDemuxer.read(&mPacket))
				{
					//ZZLAB_TRACE("end of stream?!");
					mCurrentIndex = 0;
					handleDelayedFrames();
					break;
				}

				mHandler = mStreamHandlers[mPacket.stream_index];
				if (mHandler)
				{
					mFrameToEnqueue = mHandler->handle(&mPacket, mFrame);
					av_free_packet(&mPacket);

					if (!mFrameToEnqueue)
						continue;

					if (mHandler->q.enqueue(mFrameToEnqueue))
						mFrameToEnqueue = NULL;
				}

				mTimer.expires_from_now(posix_time::milliseconds(4));
				mTimer.async_wait(bind(&FileMediaPlayer::readPacket, this, asio::placeholders::error));

				break;
			}
		}

		void FileMediaPlayer::handleDelayedFrames(boost::system::error_code err)
		{
			if (err || mStopping.load(boost::memory_order_acquire))
			{
				mPlaying.store(0, memory_order_release);
				mAfterStop();

				return;
			}

			if (mFrameToEnqueue)
			{
				if (!mHandler->q.enqueue(mFrameToEnqueue))
				{
					mTimer.expires_from_now(posix_time::milliseconds(10));
					mTimer.async_wait(bind(&FileMediaPlayer::handleDelayedFrames, this, asio::placeholders::error));
					return;
				}

				mFrameToEnqueue = NULL;
			}

			while (true)
			{
				mHandler = mStreamHandlers[mCurrentIndex];
				if (mHandler)
				{
					mFrameToEnqueue = mHandler->handle(&mPacket, mFrame);
					if (!mFrameToEnqueue)
					{
						// next stream
						++mCurrentIndex;
					}
					else if (mHandler->q.enqueue(mFrameToEnqueue))
					{
						mFrameToEnqueue = NULL;

						// next stream
						++mCurrentIndex;
					}
				}
				else
				{
					// next stream
					++mCurrentIndex;
				}

				// end of stream
				if (mCurrentIndex >= mStreamHandlers.size())
				{
					if (loopPlay)
					{
						for (size_t i = 0; i < mStreamHandlers.size(); ++i)
						{
							StreamHandler* handler = mStreamHandlers[i];
							if (handler)
							{
								handler->offset += handler->pts;

								//ZZLAB_TRACE(i << ": " << handler->pts << ',' << handler->offset);

								// seek to begining
								av_seek_frame(mDemuxer.getContext(), (int)i, 0, AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_FRAME);

								// flush codec context
								avcodec_flush_buffers(handler->d.getContext());
							}
						}

						// read more packets
						readPacket();
					}
					else
					{
						mPlaying.store(-1, memory_order_release);
						mTimer.expires_from_now(posix_time::seconds(86400)); // wait forever
						mTimer.async_wait(bind(&FileMediaPlayer::handleAfterEndOfStream, this, asio::placeholders::error));

						mEndOfStream();
					}
				}
				else
				{
					mTimer.expires_from_now(posix_time::milliseconds(4));
					mTimer.async_wait(bind(&FileMediaPlayer::handleDelayedFrames, this, asio::placeholders::error));
				}

				break;
			}
		}

		void FileMediaPlayer::handleAfterEndOfStream(boost::system::error_code err)
		{
			if (err || mStopping.load(boost::memory_order_acquire))
			{
				mPlaying.store(0, memory_order_release);
				mAfterStop();

				return;
			}

			mTimer.expires_from_now(posix_time::seconds(86400)); // wait forever
			mTimer.async_wait(bind(&FileMediaPlayer::handleAfterEndOfStream, this, asio::placeholders::error));
		}

		AVSampleFormat cvtSampleFormat(PaSampleFormat fmt)
		{
			switch (fmt)
			{
			case paInt16:
				return AV_SAMPLE_FMT_S16;

			case paInt32:
				return AV_SAMPLE_FMT_S32;

			case paFloat32:
				return AV_SAMPLE_FMT_FLT;

			default:
				return AV_SAMPLE_FMT_NONE;
			}
		}

		PaSampleFormat cvtSampleFormat(AVSampleFormat fmt)
		{
			switch (fmt)
			{
			case AV_SAMPLE_FMT_S16:
				return paInt16;

			case AV_SAMPLE_FMT_S32:
				return paInt32;

			case AV_SAMPLE_FMT_FLT:
				return paFloat32;

			default:
				return paCustomFormat;
			}
		}

		AudioDevice::AudioDevice() :
			sampleRate(44100),
			framesPerBuffer(paFramesPerBufferUnspecified),
			streamFlags(paNoFlag),
			mStream(NULL)
		{
			ZZLAB_TRACE_THIS();

			inputParameters.device = Pa_GetDefaultInputDevice();
			if (inputParameters.device != paNoDevice)
			{
				inputParameters.channelCount = 2;
				inputParameters.sampleFormat = paFloat32;
				inputParameters.suggestedLatency = Pa_GetDeviceInfo(inputParameters.device)->defaultLowOutputLatency;
				inputParameters.hostApiSpecificStreamInfo = NULL;
			}

			outputParameters.device = Pa_GetDefaultOutputDevice();
			if (outputParameters.device != paNoDevice)
			{
				outputParameters.channelCount = 2;
				outputParameters.sampleFormat = paFloat32;
				outputParameters.suggestedLatency = Pa_GetDeviceInfo(outputParameters.device)->defaultLowOutputLatency;
				outputParameters.hostApiSpecificStreamInfo = NULL;
			}
		}

		AudioDevice::~AudioDevice()
		{
			ZZLAB_TRACE_THIS();

			if (!mStream)
				return;

			PaError err = Pa_CloseStream(mStream);
			if (err != paNoError)
				ZZLAB_ERROR("Failed to close audio stream, " << Pa_GetErrorText(err));
		}

		void AudioDevice::dumpInfo()
		{
			for (PaDeviceIndex i = 0; i < Pa_GetDeviceCount(); ++i)
			{
				ZZLAB_INFO("---- Device index: " << i);
				const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
				if (info)
				{
					ZZLAB_INFO("\"" << info->name << "\"");
					ZZLAB_INFO("    structVersion: " << info->structVersion);
					ZZLAB_INFO("    hostApi: " << info->hostApi);
					ZZLAB_INFO("    maxInputChannels: " << info->maxInputChannels);
					ZZLAB_INFO("    maxOutputChannels: " << info->maxOutputChannels);
					ZZLAB_INFO("    defaultLowInputLatency: " << info->defaultLowInputLatency);
					ZZLAB_INFO("    defaultLowOutputLatency: " << info->defaultLowOutputLatency);
					ZZLAB_INFO("    defaultHighInputLatency: " << info->defaultHighInputLatency);
					ZZLAB_INFO("    defaultHighOutputLatency: " << info->defaultHighOutputLatency);
					ZZLAB_INFO("    defaultSampleRate: " << info->defaultSampleRate);
				}
				else
				{
					ZZLAB_ERROR("NO INFO for this device!!");
				}
			}
		}

		bool AudioDevice::load(XmlNode* node)
		{
			if (node == NULL)
				return false;

			XmlAttribute* attr = node->first_attribute(L"sapmle-rate");
			if (attr)
			{
				sampleRate = (float)_wtof(attr->value());
			}

			XmlNode* param = node->first_node(L"input");
			if (param)
			{
				attr = param->first_attribute(L"device-index");
				if (attr)
					inputParameters.device = _wtoi(attr->value());

				attr = param->first_attribute(L"channel-count");
				if (attr)
					inputParameters.channelCount = _wtoi(attr->value());
			}
			else
			{
				inputParameters.device = paNoDevice;
			}

			param = node->first_node(L"output");
			if (param)
			{
				attr = param->first_attribute(L"device-index");
				if (attr)
					outputParameters.device = _wtoi(attr->value());

				attr = param->first_attribute(L"channel-count");
				if (attr)
					outputParameters.channelCount = _wtoi(attr->value());
			}
			else
			{
				outputParameters.device = paNoDevice;
			}

			return true;
		}

		void AudioDevice::init()
		{
			ZZLAB_TRACE_THIS();

			switch (outputParameters.sampleFormat)
			{
			case paInt16:
				mClear = AudioClear<int16_t>(outputParameters.channelCount);
				mixer = AudioMix<int16_t>(outputParameters.channelCount);
				break;

			case paInt32:
				mClear = AudioClear<int32_t>(outputParameters.channelCount);
				mixer = AudioMix<int32_t>(outputParameters.channelCount);
				break;

			case paFloat32:
				mClear = AudioClear<float>(outputParameters.channelCount);
				mixer = AudioMix<float>(outputParameters.channelCount);
				break;

			default:
				//mClear = bind(dummy, (uint8_t *)NULL, 0);
				break;
			}

			ZZLAB_INFO("Opening Audio I/O stream @ " << sampleRate << " Hz...");
			if (inputParameters.device != paNoDevice)
				ZZLAB_INFO("---- Audio input: index=" << inputParameters.device << ", channels=" << inputParameters.channelCount);
			if (outputParameters.device != paNoDevice)
				ZZLAB_INFO("---- Audio output: index=" << outputParameters.device << ", channels=" << outputParameters.channelCount);

			PaError err = Pa_OpenStream(&mStream,
				inputParameters.device == paNoDevice ? NULL : &inputParameters,
				outputParameters.device == paNoDevice ? NULL : &outputParameters,
				sampleRate,
				framesPerBuffer, streamFlags,
				(PaStreamCallback *)&onDataArrived, this);
			if (err != paNoError)
			{
				ZZLAB_ERROR("Failed to open audio stream, " << Pa_GetErrorText(err));
				return;
			}
		}

		void AudioDevice::start()
		{
			ZZLAB_TRACE_THIS();

			PaError err = Pa_StartStream(mStream);
			if (err != paNoError)
			{
				ZZLAB_ERROR("Failed to start audio stream, " << Pa_GetErrorText(err));
				return;
			}
		}

		void AudioDevice::stop()
		{
			ZZLAB_TRACE_THIS();

			PaError err = Pa_StopStream(mStream);
			if (err != paNoError)
			{
				ZZLAB_ERROR("Failed to stop audio stream, " << Pa_GetErrorText(err));
				return;
			}
		}

		int AudioDevice::onDataArrived(const uint8_t *input,
			uint8_t *output, unsigned long frameCount,
			const PaStreamCallbackTimeInfo *timeInfo,
			PaStreamCallbackFlags statusFlags, AudioDevice *pThis)
		{
			return pThis->onDataArrived0(input, output, frameCount, timeInfo, statusFlags);
		}

		int AudioDevice::onDataArrived0(const uint8_t *input,
			uint8_t *output, unsigned long frameCount,
			const PaStreamCallbackTimeInfo *timeInfo,
			PaStreamCallbackFlags statusFlags)
		{
			mClear(output, frameCount);
			mEvents.invoke(input, output, frameCount, timeInfo);

			return paContinue;
		}

		NullRenderer::NullRenderer() :
			timeSource(NULL),
			source(NULL),
			renderRate(240.0f),
			mPlayTime(0),
			mTimer(_MainService),
			mPlaying(false),
			mStopping(false)
		{
			ZZLAB_TRACE_THIS();
		}

		NullRenderer::~NullRenderer()
		{
			ZZLAB_TRACE_THIS();

			if (isPlaying())
				ZZLAB_ERROR("MUST call stop first!!");
		}

		void NullRenderer::play(int64_t playTime)
		{
			if (mPlaying.exchange(true, memory_order_release) == true)
			{
				ZZLAB_WARN("Null renderer is PLAYING");
				return;
			}

			mPlayTime = playTime;
			mStopping.store(false, memory_order_release);
			mDuration = size_t(1000 / renderRate);
			render();
		}

		void NullRenderer::stop(boost::function<void()> afterStop)
		{
			if (!isPlaying())
			{
				ZZLAB_WARN("Null renderer is NOT playing");
				return;
			}

			mStopping.store(true, memory_order_release);
			mAfterStop = afterStop;
			mTimer.cancel();
		}

		void NullRenderer::render(boost::system::error_code err)
		{
			if (err || mStopping.load(boost::memory_order_acquire))
			{
				mPlaying.store(false, memory_order_release);
				mAfterStop();

				return;
			}

			int64_t now = timeSource->getTime() - mPlayTime;

			AVFrame* frame;
			size_t dropped = source->dequeue(now, frame);
			if (frame)
			{
				//if (dropped)
				//	ZZLAB_WARN(__FUNCTION__ << ": " << dropped << " frame(s) dropped");

				av_frame_free(&frame);
			}

			mTimer.expires_from_now(posix_time::milliseconds(mDuration));
			mTimer.async_wait(bind(&NullRenderer::render, this, asio::placeholders::error));
		}

		AudioQueueRenderer::AudioQueueRenderer() :
			timeSource(NULL),
			source(NULL),
			device(NULL),
			mTimeOffset(0),
			mPlayTime(0),
			mAudioFrame(NULL),
			mPlaying(false),
			mStopRendering(true)
		{
			ZZLAB_TRACE_THIS();
		}

		AudioQueueRenderer::~AudioQueueRenderer()
		{
			ZZLAB_TRACE_THIS();

			if (isPlaying())
				ZZLAB_ERROR("MUST call stop first!!");

			av_frame_free(&mAudioFrame);
		}

		void AudioQueueRenderer::play(int64_t playTime)
		{
			if (mPlaying.exchange(true, memory_order_release) == true)
			{
				ZZLAB_WARN("Audio queue is PLAYING");
				return;
			}

			int64_t now = timeSource->getTime();
			PaTime streamTime = device->getStreamTime();

			double masterTime = now / utils::Timer::fTimeUnit;
			mTimeOffset = masterTime - streamTime;

			mPlayTime = playTime;
			mStopRendering = false;
			device->waitForBuffer(bind(&AudioQueueRenderer::renderAudio, this, _1, _2, _3, _4));
		}

		void AudioQueueRenderer::stop(boost::function<void()> afterStop)
		{
			if (!isPlaying())
			{
				ZZLAB_WARN("Audio queue is NOT playing");
				return;
			}

			mAfterStop = afterStop;

			// add a task to audio thread
			device->waitForBuffer(bind(&AudioQueueRenderer::stopRenderAudio, this));
		}

		void AudioQueueRenderer::renderAudio(const uint8_t *input, uint8_t *output,
			unsigned long frameCount, const PaStreamCallbackTimeInfo* timeInfo)
		{
			//ZZLAB_TRACE_THIS();

			if (mStopRendering)
			{
				mPlaying.store(false, memory_order_release);
				mAfterStop();

				return;
			}

			int64_t now = int64_t((timeInfo->currentTime + mTimeOffset) * utils::Timer::timeUnit)
				- mPlayTime;
			while (frameCount > 0)
			{
				if (mAudioFrame)
				{
					// mix audio
					unsigned long samplesConsumed = (std::min)((unsigned long)frameCount,
						(unsigned long)mAudioSamples);
					size_t bytesConsumed = device->mixer(mAudioData, output, samplesConsumed);

					output += bytesConsumed;
					frameCount -= samplesConsumed;

					mAudioData += bytesConsumed;
					if ((mAudioSamples -= samplesConsumed) == 0)
						av_frame_free(&mAudioFrame);
				}
				else
				{
					size_t dropped = source->dequeue(now, mAudioFrame);
					if (!mAudioFrame)
						break;

					//if (dropped)
					//	ZZLAB_WARN(__FUNCTION__ << ": " << dropped << " frame(s) dropped");

					mAudioData = mAudioFrame->data[0];
					mAudioSamples = mAudioFrame->nb_samples;
				}
			}

			device->waitForBuffer(bind(&AudioQueueRenderer::renderAudio, this, _1, _2, _3, _4));
		}

		void AudioQueueRenderer::stopRenderAudio()
		{
			ZZLAB_TRACE_THIS();

			mStopRendering = true;
		}

		VideoQueueRenderer::VideoQueueRenderer() :
			timeSource(NULL),
			source(NULL),
			rendererEvents(NULL),
			mPlaying(false),
			mStopping(false)
		{
			ZZLAB_TRACE_THIS();

			renderFrame = boost::bind(dummy);
		}

		VideoQueueRenderer::~VideoQueueRenderer()
		{
			ZZLAB_TRACE_THIS();
		}

		void VideoQueueRenderer::play(int64_t playTime)
		{
			if (isPlaying())
			{
				ZZLAB_WARN("Video renderer is PLAYING");
				return;
			}

			mPlayTime = playTime;
			mPlaying = true;
			mStopping = false;

			renderVideo();
		}

		void VideoQueueRenderer::stop(boost::function<void()> afterStop)
		{
			if (!isPlaying())
			{
				ZZLAB_WARN("Video renderer is NOT playing");
				return;
			}

			mAfterStop = afterStop;
			mStopping = true;
		}

		void VideoQueueRenderer::renderVideo()
		{
			if (mStopping)
			{
				mPlaying = false;
				mAfterStop();

				return;
			}

			int64_t now = timeSource->getTime() - mPlayTime;

			AVFrame* frame = NULL;
			size_t dropped = source->dequeue(now, frame);

			if (frame)
			{
				//if (dropped)
				//	ZZLAB_WARN(__FUNCTION__ << ": " << dropped << " frame(s) dropped");

				renderFrame(frame);
				av_frame_free(&frame);
			}

			// issue a next-frame request
			rendererEvents->waitForFrameBegin(bind(&VideoQueueRenderer::renderVideo, this));
		}

		SimpleMediaPlayer::SimpleMediaPlayer() :
			timeSource(NULL),
			audioDevice(NULL),
			mAudioRenderer(NULL),
			mVideoRenderer(NULL),
			mVideoCodecCtx(NULL)
		{
			ZZLAB_TRACE_THIS();
		}

		SimpleMediaPlayer::~SimpleMediaPlayer()
		{
			ZZLAB_TRACE_THIS();

			if (mAudioRenderer)
				delete mAudioRenderer;

			if (mVideoRenderer)
				delete mVideoRenderer;
		}

		void SimpleMediaPlayer::init()
		{
			FileMediaPlayer::init();

			for (size_t i = 0; i < getStreams(); ++i)
			{
				AVStream* stream = getStream(i);
				if (stream->codec->codec_type == AVMEDIA_TYPE_VIDEO)
				{
					if (!mVideoRenderer)
					{
						mVideoCodecCtx = stream->codec;

						mVideoRenderer = new av::VideoQueueRenderer();
						mVideoRenderer->timeSource = timeSource;
						mVideoRenderer->source = getFrameQueue(i);
					}
				}
				else if (stream->codec->codec_type == AVMEDIA_TYPE_AUDIO)
				{
					if (!mAudioRenderer)
					{
						mAudioRenderer = new av::AudioQueueRenderer();
						mAudioRenderer->timeSource = timeSource;
						mAudioRenderer->source = getFrameQueue(i);
						mAudioRenderer->device = audioDevice;
					}
				}
			}
		}

		void SimpleMediaPlayer::play(int64_t playTime, boost::function<void()> endOfStream)
		{
			if (mAudioRenderer)
				mAudioRenderer->play(playTime);

			if (mVideoRenderer)
				mVideoRenderer->play(playTime);

			FileMediaPlayer::play(endOfStream);
		}

		void SimpleMediaPlayer::stop(boost::function<void()> afterStop)
		{
			mAfterStop = afterStop;
			mStopCount = 1;

			if (mAudioRenderer)
			{
				++mStopCount;
				mAudioRenderer->stop(_MainService.wrap(
					boost::bind(&SimpleMediaPlayer::handleStop, this)));
			}

			if (mVideoRenderer)
			{
				++mStopCount;
				mVideoRenderer->stop(_MainService.wrap(
					boost::bind(&SimpleMediaPlayer::handleStop, this)));
			}

			FileMediaPlayer::stop(_MainService.wrap(
				boost::bind(&SimpleMediaPlayer::handleStop, this)));
		}

		void SimpleMediaPlayer::handleStop()
		{
			if (--mStopCount == 0)
				mAfterStop();
		}

		void enumerateDevices(const CLSID& clsid, monikers_t& monikers)
		{
			ICreateDevEnumPtr createDevEnum(CLSID_SystemDeviceEnum);

			IEnumMonikerPtr enumMoniker;
			HR(createDevEnum->CreateClassEnumerator(clsid, &enumMoniker, 0));

			enumerateAll(enumMoniker, monikers);
		}

		_bstr_t getFriendlyName(const IMonikerPtr& moniker)
		{
			IBindCtxPtr bindCtx;
			HR(CreateBindCtx(0, &bindCtx));

			IPropertyBagPtr propertyBag;
			HRESULT hr = moniker->BindToStorage(bindCtx, NULL, IID_IPropertyBag, (void **)&propertyBag);
			if (SUCCEEDED(hr))
			{
				_variant_t var;
				hr = propertyBag->Read(L"FriendlyName", &var, NULL);
				if (SUCCEEDED(hr)) return var;
			}

			return _bstr_t();
		}

		void dumpMonikerFriendlyNames(const monikers_t& mks)
		{
			for (size_t i = 0; i < mks.size(); ++i)
			{
				ZZLAB_INFO("Index: " << i);

				const IMonikerPtr& m = mks[i];

				_bstr_t name = av::getFriendlyName(m);
				if (name.length() > 0)
					ZZLAB_INFO("Name: " << name);
			}
		}

		AM_MEDIA_TYPE* allocMediaType()
		{
			AM_MEDIA_TYPE* ret = (AM_MEDIA_TYPE*)CoTaskMemAlloc(sizeof(AM_MEDIA_TYPE));
			memset(ret, 0, sizeof(AM_MEDIA_TYPE));

			return ret;
		}

		void freeMediaType(AM_MEDIA_TYPE& mt)
		{
			if (mt.cbFormat != 0)
			{
				CoTaskMemFree((PVOID)mt.pbFormat);
				mt.cbFormat = 0;
				mt.pbFormat = NULL;
			}
			if (mt.pUnk != NULL)
			{
				// pUnk should not be used.
				mt.pUnk->Release();
				mt.pUnk = NULL;
			}
		}

		void deleteMediaType(AM_MEDIA_TYPE *pmt)
		{
			if (pmt != NULL)
			{
				freeMediaType(*pmt);
				CoTaskMemFree(pmt);
			}
		}

		void deleteStreamCaps(stream_caps_t& caps)
		{
			for (stream_caps_t::const_iterator i = caps.begin(); i != caps.end(); ++i)
				deleteMediaType((*i).first);

			caps.clear();
		}

		AM_MEDIA_TYPE* getMediaType(const stream_caps_t& caps, int width, int height, const GUID& subtype)
		{
			// select a format
			for (stream_caps_t::const_iterator i = caps.begin(); i != caps.end(); ++i)
			{
				const stream_cap& cap = *i;

				if (width == cap.second.InputSize.cx && height == cap.second.InputSize.cy &&
					subtype == cap.first->subtype)
				{
					return cap.first;
				}
			}

			return NULL;
		}

		void dumpMediaType(AM_MEDIA_TYPE* mt)
		{
			if (mt->majortype == MEDIATYPE_Audio)
				ZZLAB_INFO("MajorType: MEDIATYPE_Audio");
			else if (mt->majortype == MEDIATYPE_Video)
				ZZLAB_INFO("MajorType: MEDIATYPE_Video");
			else
				ZZLAB_INFO("MajorType: Unknown");

			if (mt->subtype == MEDIASUBTYPE_YUY2)
				ZZLAB_INFO("SubType: MEDIASUBTYPE_YUY2");
			else if (mt->subtype == MEDIASUBTYPE_YV12)
				ZZLAB_INFO("SubType: MEDIASUBTYPE_YV12");
			else if (mt->subtype == WMMEDIASUBTYPE_I420)
				ZZLAB_INFO("SubType: WMMEDIASUBTYPE_I420");
			else if (mt->subtype == MEDIASUBTYPE_RGB24)
				ZZLAB_INFO("SubType: MEDIASUBTYPE_RGB24");
			else if (mt->subtype == MEDIASUBTYPE_RGB32)
				ZZLAB_INFO("SubType: MEDIASUBTYPE_RGB32");
			else
				ZZLAB_INFO("SubType: Unknown");

			BITMAPINFOHEADER* bih;
			if (mt->formattype == FORMAT_VideoInfo)
			{
				ZZLAB_INFO("FormatType: FORMAT_VideoInfo");

				VIDEOINFOHEADER* vih = (VIDEOINFOHEADER*)mt->pbFormat;
				ZZLAB_INFO("rcSource: " << vih->rcSource.left << ',' << vih->rcSource.top << ',' << vih->rcSource.right << ',' << vih->rcSource.bottom);
				ZZLAB_INFO("rcTarget: " << vih->rcTarget.left << ',' << vih->rcTarget.top << ',' << vih->rcTarget.right << ',' << vih->rcTarget.bottom);
				ZZLAB_INFO("dwBitRate: " << vih->dwBitRate);
				ZZLAB_INFO("dwBitErrorRate: " << vih->dwBitErrorRate);
				ZZLAB_INFO("AvgTimePerFrame: " << vih->AvgTimePerFrame / utils::Timer::fTimeUnit);

				bih = &vih->bmiHeader;
			}
			else if (mt->formattype == FORMAT_VideoInfo2)
			{
				ZZLAB_INFO("FormatType: FORMAT_VideoInfo2");

				VIDEOINFOHEADER2* vih = (VIDEOINFOHEADER2*)mt->pbFormat;
				ZZLAB_INFO("rcSource: " << vih->rcSource.left << ',' << vih->rcSource.top << ',' << vih->rcSource.right << ',' << vih->rcSource.bottom);
				ZZLAB_INFO("rcTarget: " << vih->rcTarget.left << ',' << vih->rcTarget.top << ',' << vih->rcTarget.right << ',' << vih->rcTarget.bottom);
				ZZLAB_INFO("dwBitRate: " << vih->dwBitRate);
				ZZLAB_INFO("dwBitErrorRate: " << vih->dwBitErrorRate);
				ZZLAB_INFO("AvgTimePerFrame: " << vih->AvgTimePerFrame / utils::Timer::fTimeUnit);

				bih = &vih->bmiHeader;
			}
			else
			{
				ZZLAB_INFO("FormatType: Unknown");

				bih = NULL;
			}

			if (bih)
			{
				ZZLAB_INFO("biSize: " << bih->biSize);
				ZZLAB_INFO("biWidth: " << bih->biWidth);
				ZZLAB_INFO("biHeight: " << bih->biHeight);
				ZZLAB_INFO("biPlanes: " << bih->biPlanes);
				ZZLAB_INFO("biBitCount: " << bih->biBitCount);
				ZZLAB_INFO("biCompression: " << bih->biCompression);
				ZZLAB_INFO("biSizeImage: " << bih->biSizeImage);
				ZZLAB_INFO("biXPelsPerMeter: " << bih->biXPelsPerMeter);
				ZZLAB_INFO("biYPelsPerMeter: " << bih->biYPelsPerMeter);
				ZZLAB_INFO("biClrUsed: " << bih->biClrUsed);
				ZZLAB_INFO("biClrImportant: " << bih->biClrImportant);
			}
		}

		Scaler::Scaler() : srcW(0), srcH(0), srcFormat(AV_PIX_FMT_RGB32),
			dstW(0), dstH(0), dstFormat(AV_PIX_FMT_RGB32), flags(SWS_POINT), mCtx(NULL)
		{
			ZZLAB_TRACE_THIS();
		}

		Scaler::~Scaler()
		{
			ZZLAB_TRACE_THIS();

			if (mCtx)
				sws_freeContext(mCtx);
		}

		VideoCap::VideoCap()
		{
			ZZLAB_TRACE_THIS();
		}

		VideoCap::~VideoCap()
		{
			ZZLAB_TRACE_THIS();
		}

		void VideoCap::init(IMonikerPtr moniker)
		{
			HR(CreateBindCtx(0, &mBindCtx));

			IGraphBuilderPtr graphBuilder(CLSID_FilterGraph);

			IBaseFilterPtr sourceFilter;
			HR(moniker->BindToObject(mBindCtx, NULL, IID_IBaseFilter, (void**)&sourceFilter));
			HR(graphBuilder->AddFilter(sourceFilter, L"Source"));

			IBaseFilterPtr sampleGrabberFilter(__uuidof(DexterLib::SampleGrabber));
			HR(graphBuilder->AddFilter(sampleGrabberFilter, L"SampleGrabber"));

			IBaseFilterPtr nullRenderer(__uuidof(DexterLib::NullRenderer));
			HR(graphBuilder->AddFilter(nullRenderer, L"Renderer"));

			ICaptureGraphBuilder2Ptr captureGraphBuilder(CLSID_CaptureGraphBuilder2);
			captureGraphBuilder->SetFiltergraph(graphBuilder);

			mGraph = graphBuilder;
			mCaptureGraph = captureGraphBuilder;
			mSourceFilter = sourceFilter;
			mSampleGrabberFilter = sampleGrabberFilter;
			mRendererFilter = nullRenderer;
		}

		void VideoCap::getStreamCaps(stream_caps_t& ret)
		{
			IAMStreamConfigPtr config;
			HR(mCaptureGraph->FindInterface(NULL, NULL, mSourceFilter, IID_IAMStreamConfig, (void**)&config));

			int count, size;
			HR(config->GetNumberOfCapabilities(&count, &size));

			if (size != sizeof(VIDEO_STREAM_CONFIG_CAPS))
				HR(E_NOTIMPL);

			ret.resize(count);

			for (int i = 0; i < count; ++i)
			{
				HR(config->GetStreamCaps(i, &ret[i].first, (BYTE*)&ret[i].second));
			}
		}

		void VideoCap::setFormat(int width, int height, const GUID& subtype)
		{
			stream_caps_t caps;
			getStreamCaps(caps);
			setFormat(caps, width, height, subtype);
			deleteStreamCaps(caps);
		}

		void VideoCap::setFormat(const stream_caps_t& caps, int width, int height, const GUID& subtype)
		{
			AM_MEDIA_TYPE* mt = getMediaType(caps, width, height, subtype);
			if (mt)
				setFormat(mt);
		}

		void VideoCap::setFormat(AM_MEDIA_TYPE* mt)
		{
			IAMStreamConfigPtr config;
			HR(mCaptureGraph->FindInterface(NULL, NULL, mSourceFilter, IID_IAMStreamConfig, (void**)&config));
			HR(config->SetFormat(mt));
		}

		void VideoCap::setNullSyncSource()
		{
			IMediaFilterPtr mediaFilter = mGraph;
			HR(mediaFilter->SetSyncSource(NULL));
		}

		void VideoCap::render()
		{
			HR(mCaptureGraph->RenderStream(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, mSourceFilter, mSampleGrabberFilter, mRendererFilter));

			ISampleGrabberPtr sampleGrabber = mSampleGrabberFilter;
			HR(sampleGrabber->SetCallback(this, 0));
		}

		AM_MEDIA_TYPE* VideoCap::getConnectedMediaType()
		{
			ISampleGrabberPtr sampleGrabber = mSampleGrabberFilter;

			AM_MEDIA_TYPE* mt = allocMediaType();
			HR(sampleGrabber->GetConnectedMediaType((DexterLib::_AMMediaType*)mt));

			return mt;
		}

		void VideoCap::dumpConnectedMediaType()
		{
			AM_MEDIA_TYPE* mt = getConnectedMediaType();
			if (mt)
			{
				dumpMediaType(mt);
				deleteMediaType(mt);
			}
		}

		void VideoCap::start()
		{
			IMediaControlPtr control = mGraph;
			HR(control->Run());
		}

		void VideoCap::stop()
		{
			IMediaControlPtr control = mGraph;
			HR(control->Stop());
		}

		HRESULT STDMETHODCALLTYPE VideoCap::QueryInterface(REFIID riid, void ** ppvObject)
		{
			if (!ppvObject) return E_POINTER;

			if (riid == __uuidof(IUnknown))
				*ppvObject = (IUnknown*)this;
			else if (riid == __uuidof(ISampleGrabberCB))
				*ppvObject = (ISampleGrabberCB*)this;

			return E_NOINTERFACE;
		}

		ULONG STDMETHODCALLTYPE VideoCap::AddRef(void)
		{
			return 1;
		}

		ULONG STDMETHODCALLTYPE VideoCap::Release(void)
		{
			return 1;
		}

		HRESULT STDMETHODCALLTYPE VideoCap::raw_SampleCB(double SampleTime, DexterLib::IMediaSample *pSample)
		{
			onFrame(SampleTime, (IMediaSample*)pSample);

			return S_OK;
		}

		HRESULT STDMETHODCALLTYPE VideoCap::raw_BufferCB(double SampleTime, BYTE *pBuffer, long BufferLen)
		{
			return E_UNEXPECTED;
		}

	} // namespace ab
} // namespace zzlab