#include "stdafx.h"
#include "Test1.h"

#include <boost/asio.hpp>
#include <boost/locale.hpp>
#include <opencv2\opencv.hpp>

using namespace boost;
using namespace zzlab;

ZZLAB_USE_LOG4CPLUS(Test1);

Test1::Test1() : mTimer0(_MainService), mTimer1(_MainService), mTimer2(_MainService)
{
	ZZLAB_TRACE_THIS();

	mVideoQueue = NULL;
	mAudioQueue = NULL;
	mMediaPlayer = NULL;
	mAudioQueueRenderer = NULL;
	//mNullRenderer = NULL;
}

Test1::~Test1()
{
	ZZLAB_TRACE_THIS();

	if (mMediaPlayer)
		delete mMediaPlayer;

	if (mAudioQueueRenderer)
		delete mAudioQueueRenderer;

	//if (mNullRenderer)
	//	delete mNullRenderer;
}

void Test1::init()
{
	ZZLAB_TRACE_THIS();

	cv::namedWindow("higui");

	mAudioDevice.inputParameters.device = paNoDevice;
	mAudioDevice.outputParameters.channelCount = 2;
	mAudioDevice.outputParameters.sampleFormat = paFloat32;
	mAudioDevice.sampleRate = 44100;

	for (PaDeviceIndex i = 0; i < Pa_GetDeviceCount(); ++i)
	{
		ZZLAB_TRACE_VALUE(i);

		const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
		ZZLAB_TRACE_VALUE(info->name);
		ZZLAB_TRACE_VALUE(info->maxInputChannels);
		ZZLAB_TRACE_VALUE(info->maxOutputChannels);

		//if (info->maxOutputChannels > 5)
		//{
		//	mAudioDevice.outputParameters.device = i;
		//	mAudioDevice.outputParameters.channelCount = 6;
		//}
	}

	mAudioDevice.init();
	mAudioDevice.start();
	
	cvKey();
}

void Test1::cvKey(boost::system::error_code err)
{
	if (err)
		return;

	int k = cv::waitKey(1);
	switch (k)
	{
	case 27:
		// TODO: more cleanups
		utils::stopAllServices();
		break;

	case 's':
	case 'S':
		if (mMediaPlayer == NULL)
		{
			play();
		}
		break;

	case 'a':
	case 'A':
		if (mMediaPlayer)
		{
			stop();
		}
		break;
	}

	mTimer0.expires_from_now(posix_time::milliseconds(10));
	mTimer0.async_wait(bind(&Test1::cvKey, this, asio::placeholders::error));
}

void Test1::play()
{
	mMediaPlayer = new av::FileMediaPlayer();
	mMediaPlayer->source = "D:\\dev\\clips\\3.mp4";

	mMediaPlayer->videoDecoderThreads = 1;
	mMediaPlayer->videoQueueSize = 64;
	mMediaPlayer->videoDropTolerance = utils::Timer::timeUnit * 5;

	mMediaPlayer->outChannelLayout = av_get_default_channel_layout(mAudioDevice.outputParameters.channelCount);
	mMediaPlayer->outSampleRate = (int)mAudioDevice.sampleRate;
	mMediaPlayer->outFormat = AV_SAMPLE_FMT_FLT;
	mMediaPlayer->audioQueueSize = 64;
	mMediaPlayer->audioDropTolerance = utils::Timer::timeUnit * 5;

	mMediaPlayer->init();

	char str[1024];
	av_get_channel_layout_string(str, sizeof(str), 
		mAudioDevice.outputParameters.channelCount, mMediaPlayer->outChannelLayout);
	ZZLAB_INFO(str);

	for (size_t i = 0; i < mMediaPlayer->getStreams(); ++i)
	{
		AVStream* stream = mMediaPlayer->getStream(i);
		if (stream->codec->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			ZZLAB_TRACE("We got video");
			mVideoQueue = mMediaPlayer->getFrameQueue(i);
		}
		else if (stream->codec->codec_type == AVMEDIA_TYPE_AUDIO)
		{
			ZZLAB_TRACE("We got audio");
			mAudioQueue = mMediaPlayer->getFrameQueue(i);
		}
	}

	// setup play time
	mPlayTime = mMMTimer.getTime() + 0 * utils::Timer::timeUnit;

	// render video
	if (mVideoQueue)
	{
		mStopping = false;
		mPlaying = true;
		renderVideo();
	}

	// render audio
	if (mAudioQueue)
	{
		mAudioQueueRenderer = new av::AudioQueueRenderer();
		mAudioQueueRenderer->timeSource = &mMMTimer;
		mAudioQueueRenderer->device = &mAudioDevice;
		mAudioQueueRenderer->source = mAudioQueue;
		mAudioQueueRenderer->play(mPlayTime);

		//mNullRenderer = new av::NullRenderer();
		//mNullRenderer->timeSource = &mMMTimer;
		//mNullRenderer->source = mAudioQueue;
		//mNullRenderer->play(mPlayTime);
	}

	mMediaPlayer->play(_MainService.wrap(bind(&Test1::endOfStream, this)));
}

void Test1::stop()
{
	mStopCount = 0;

	if (mVideoQueue)
	{
		++mStopCount;
		mStopping = true;
		mTimer1.cancel();
	}

	if (mAudioQueue)
	{
		++mStopCount;
		mAudioQueueRenderer->stop(_MainService.wrap(bind(&Test1::afterStop, this, "AUDIOQUEUERENDERER")));
		//mNullRenderer->stop(_MainService.wrap(bind(&Test1::afterStop, this, "NULLRENDERER")));
	}

	++mStopCount;
	mMediaPlayer->stop(_MainService.wrap(bind(&Test1::afterStop, this, "MEDIAPLAYER")));
}

void Test1::renderVideo(boost::system::error_code err)
{
	if (err || mStopping)
	{
		mPlaying = false;
		afterStop("Test1");
		return;
	}

	int64_t now = mMMTimer.getTime() - mPlayTime;

	AVFrame* frame = NULL;
	size_t dropped = mVideoQueue->dequeue(now, frame);

	if (frame)
	{
		if (dropped)
			ZZLAB_TRACE_VALUE(dropped);

		cv::Mat1b mat(frame->height, frame->width, frame->data[0], frame->linesize[0]);
		cv::imshow("frame", mat);
		av_frame_free(&frame);
	}

	mTimer1.expires_from_now(posix_time::milliseconds(4));
	mTimer1.async_wait(bind(&Test1::renderVideo, this, asio::placeholders::error));
}

void Test1::afterStop(std::string tag)
{
	ZZLAB_TRACE_THIS1("tag=" << tag.c_str());

	if(--mStopCount == 0)
	{
		delete mAudioQueueRenderer;
		mAudioQueueRenderer = NULL;

		//delete mNullRenderer;
		//mNullRenderer = NULL;

		delete mMediaPlayer;
		mMediaPlayer = NULL;

		ZZLAB_TRACE("STOP DONE");
	}
}

void Test1::endOfStream()
{
	ZZLAB_INFO("END OF STREAM");

	waitForFinish();
}

void Test1::waitForFinish(boost::system::error_code err)
{
	if (err)
		return;

	if (mVideoQueue->empty() && mAudioQueue->empty())
	{
		mStopCount = 0;
		if (mVideoQueue)
		{
			++mStopCount;
			mStopping = true;
			mTimer1.cancel();
		}

		if (mAudioQueue)
		{
			++mStopCount;
			mAudioQueueRenderer->stop(_MainService.wrap(bind(&Test1::afterStop, this, "AUDIOQUEUERENDERER")));
			//mNullRenderer->stop(_MainService.wrap(bind(&Test1::afterStop, this, "NULLRENDERER")));
		}

		mTimer2.expires_from_now(posix_time::seconds(2));
		mTimer2.async_wait(bind(&Test1::restart, this, asio::placeholders::error));
	}
	else
	{
		mTimer2.expires_from_now(posix_time::milliseconds(100));
		mTimer2.async_wait(bind(&Test1::waitForFinish, this, asio::placeholders::error));
	}

}

void Test1::restart(boost::system::error_code err)
{
	if (err)
		return;

	play();
}