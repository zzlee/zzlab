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
#include <boost/atomic.hpp>
#include <portaudio.h>

using namespace boost;
using namespace zzlab;

ZZLAB_USE_LOG4CPLUS(test2);

class Test1
{
public:
	Test1() : mVideoFrame(NULL), mTimer(_MainService)
	{
		ZZLAB_TRACE_THIS();
	}

	~Test1()
	{
		ZZLAB_TRACE_THIS();

		mVideoCap.stop();

		AVFrame* frame = mVideoFrame.exchange(nullptr);
		av_frame_free(&frame);
	}

	void init()
	{
		ZZLAB_TRACE_THIS();

		av::monikers_t mks;
		av::enumerateDevices(CLSID_VideoInputDeviceCategory, mks);
		av::dumpMonikerFriendlyNames(mks);

		mVideoCap.onFrame = bind(&Test1::onFrame, this, _1, _2);

		mVideoCap.init(mks[0]);

		mVideoCap.setNullSyncSource();
		mVideoCap.setFormat(640, 480, MEDIASUBTYPE_YUY2);
		mVideoCap.render();
		mVideoCap.dumpConnectedMediaType();

		mScaler.srcW = mScaler.dstW = 640;
		mScaler.srcH = mScaler.dstH = 480;
		mScaler.srcFormat = AV_PIX_FMT_YUYV422;
		mScaler.dstFormat = AV_PIX_FMT_YUV422P;
		mScaler.init();

		mVideoCap.start();

		render();
	}

protected:
	boost::asio::deadline_timer mTimer;
	av::VideoCap mVideoCap;

	av::Scaler mScaler;
	boost::atomic<AVFrame*> mVideoFrame;

	void render(boost::system::error_code err = system::error_code())
	{
		if (err)
			return;

		int ch = cv::waitKey(1);
		if (ch != -1)
		{
			utils::stopAllServices();
			return;
		}

		AVFrame* frame = mVideoFrame.exchange(NULL);
		if (frame)
		{
			cv::Mat1b y(480, 640, frame->data[0], frame->linesize[0]);
			cv::imshow("y", y);

			cv::Mat1b u(480, 640 / 2, frame->data[1], frame->linesize[1]);
			cv::imshow("u", u);

			cv::Mat1b v(480, 640 / 2, frame->data[2], frame->linesize[2]);
			cv::imshow("v", v);

			av_frame_free(&frame);
		}

		mTimer.expires_from_now(posix_time::milliseconds(4));
		mTimer.async_wait(bind(&Test1::render, this, asio::placeholders::error));
	}

	void onFrame(double t, IMediaSample* ms)
	{
		uint8_t* data;
		HR(ms->GetPointer(&data));

		AVFrame* frame = av_frame_alloc();
		frame->width = 640;
		frame->height = 480;
		frame->format = AV_PIX_FMT_YUV422P;
		av_frame_get_buffer(frame, 1);

		int srcStride = 640 * 2;
		mScaler.scale(&data, &srcStride, 480, frame->data, frame->linesize);

		AVFrame* org = mVideoFrame.exchange(frame);
		av_frame_free(&org);
	}
};

int _tmain(int argc, _TCHAR* argv[])
{
	// set logger
	log4cplus::Logger::getRoot().setLogLevel(log4cplus::TRACE_LOG_LEVEL);

	// install plugins
	utils::install();
	av::install();

	// intialize zzlab (including plugins)
	initialize();

	{
		// install ctrl-c signal handler for console mode
		asio::signal_set signals(_MainService, SIGINT, SIGTERM);
		signals.async_wait(boost::bind(utils::stopAllServices));

		Test1 t;
		t.init();

		// start all services
		utils::startAllServices();
	}

	// unintialize zzlab (including plugins)
	uninitialize();

	return 0;
}