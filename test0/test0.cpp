// test0.cpp : 定義主控台應用程式的進入點。
//

#include "stdafx.h"
#include "zzlab.h"
#include "zzlab/av.h"
#include "zzlab/utils.h"

#include <iostream>
#include <queue>
#include <conio.h>

#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/ref.hpp>
#include <boost/thread.hpp>

#include <opencv2\opencv.hpp>

using namespace boost;
using namespace zzlab;

ZZLAB_USE_LOG4CPLUS(Main);

class Test0
{
public:
	av::FileReader reader;
	//av::Demuxer demuxer;
	//av::VideoDecoder* videoDecoder;
	//av::FrameQueue videoFrameQ;

	utils::HiPerfTimer mTimer;
	int64_t mPlayTime;

	asio::deadline_timer mUITimer;

	Test0() : /*videoDecoder(NULL),*/ mUITimer(_MainService)
	{
		ZZLAB_TRACE_THIS();
	}

	~Test0()
	{
		ZZLAB_TRACE_THIS();

		//if (videoDecoder)
		//	delete videoDecoder;
	}

	void init()
	{
		//AVFormatContext* ctx = reader.load(_AssetsPath / L"0824YiLan-2.mp4");
		AVFormatContext* ctx = nullptr;
		if (ctx)
		{
			reader.dumpInfo();
			reader.afterRead = bind(&av::Test0::afterRead, this, _1, _2);
			reader.onEOF = bind(&Test0::onEOF, this);

			//demuxer.init(ctx);
			//ZZLAB_TRACE_VALUE(ctx->nb_streams);
			//for (unsigned int i = 0; i < ctx->nb_streams; ++i)
			//{
			//	AVStream *stream = ctx->streams[i];
			//	AVCodecContext *codecCtx = stream->codec;

			//	ZZLAB_TRACE_VALUE(av_get_media_type_string(codecCtx->codec_type));

			//	if (codecCtx->codec_type == AVMEDIA_TYPE_VIDEO)
			//	{
			//		if (videoDecoder == NULL)
			//		{
			//			videoDecoder = new zzlab::av::VideoDecoder();
			//			if (!videoDecoder->init(codecCtx))
			//			{
			//				delete videoDecoder;
			//				videoDecoder = NULL;
			//				continue;
			//			}
			//			//videoDecoder->afterDecode = bind(&av::FrameQueue::enqueue, ref(videoFrameQ), _1, _2);
			//			videoFrameQ.timeBase = stream->time_base;

			//			//demuxer.afterDemux[i] = bind(&av::VideoDecoder::decode, videoDecoder, _1, _2);
			//		}
			//	}
			//}

			// 3 seconds delayed
			mPlayTime = mTimer.getTime() + utils::Timer::timeUnit * 3;

			// start read packet
			reader.startRead();
		}

		cv::namedWindow("Main");
		_MainService.post(bind(&Test0::step, this, system::error_code()));
	}

	void afterRead(AVPacket*, av::Notifier notifier)
	{

	}

	void step(const boost::system::error_code &e)
	{
		if (e)
			return;

		if (cv::waitKey(1) >= 0)
		{
			utils::stopAllServices();
			return;
		}

		// get frame
		//int64_t now = mTimer.getTime() - mPlayTime;
		//ZZLAB_STEP();
		//AVFrame* frame = videoFrameQ.dequeue(now);
		//if (frame)
		//{
		//	//ZZLAB_TRACE_VALUE(frame);
		//	//cv::Mat1b gray(frame->height, frame->width, frame->data[0], frame->linesize[0]);
		//	//cv::Mat3b img;
		//	//cv::cvtColor(gray, img, CV_GRAY2BGR);
		//	//cv::imshow("Image", img);
		//	//av_frame_free(&frame);
		//	{
		//		delete[] frame->data[0];
		//		delete frame;
		//	}
		//}

		mUITimer.expires_from_now(boost::posix_time::milliseconds(10));
		mUITimer.async_wait(boost::bind(&Test0::step, this, boost::asio::placeholders::error));
	}

	void onEOF()
	{
		ZZLAB_TRACE_THIS();
	}
};

//class Test1
//{
//public:
//	Test1() : mCapture(NULL), mTimer(_MainService)
//	{
//	}
//
//	~Test1()
//	{
//		if (mCapture)
//			delete mCapture;
//	}
//
//	void init()
//	{
//		mCapture = new cv::VideoCapture(0);
//
//		cv::namedWindow("Main");
//		
//		_MainService.post(bind(&Test1::step, this, system::error_code()));
//	}
//
//protected:
//	asio::deadline_timer mTimer;
//	cv::VideoCapture* mCapture;
//
//	void step(const boost::system::error_code &e)
//	{
//		if (e)
//			return;
//
//		cv::Mat img;
//		(*mCapture) >> img;
//		cv::imshow("img", img);
//
//		if (cv::waitKey(1) >= 0)
//		{
//			stopAll();
//			return;
//		}
//
//		mTimer.expires_from_now(boost::posix_time::milliseconds(10));
//		mTimer.async_wait(boost::bind(&Test1::step, this, boost::asio::placeholders::error));
//	}
//};

int _tmain(int argc, _TCHAR* argv[])
{
	// set log level
	log4cplus::Logger::getRoot().setLogLevel(log4cplus::TRACE_LOG_LEVEL);

	// install plugins
	av::install();
	utils::install();

	// intialize zzlab (including plugins)
	initialize();

	// install ctrl-c signal handler for console mode
	asio::signal_set signals(_MainService, SIGINT, SIGTERM);
	signals.async_wait(utils::stopAllServices());

	{
		Test0 test;
		test.init();

		// start services
		startWorkerService();
		startMainService();
	}

	// unintialize zzlab (including plugins)
	uninitialize();

	return 0;
}

