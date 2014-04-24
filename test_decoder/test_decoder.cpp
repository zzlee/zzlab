// test_decoder.cpp : 定義主控台應用程式的進入點。
//

#include "stdafx.h"

#include <fstream>

#include <opencv2/opencv.hpp>

#include "zzlab.h"
#include "zzlab/utils.h"
#include "zzlab/av.h"

using namespace boost;
using namespace zzlab;

ZZLAB_USE_LOG4CPLUS(test_decoder);

int _tmain(int argc, _TCHAR* argv[])
{
	// set logger
	log4cplus::Logger::getRoot().setLogLevel(log4cplus::TRACE_LOG_LEVEL);

	// install plugins
	zzlab::utils::install();
	zzlab::av::install();

	// intialize zzlab (including plugins)
	zzlab::initialize();

	AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_H264);
	AVCodecContext* c = avcodec_alloc_context3(codec);

	if (avcodec_open2(c, codec, NULL) < 0) {
		ZZLAB_ERROR("Could not open codec");
		return 1;
	}

	{
		av::Decoder decoder;
		decoder.init(c);

		std::ifstream idx("D:\\UDisk\\YUAN\\20140305.bill\\video_buf_h264.txt");
		std::ifstream bin("D:\\UDisk\\YUAN\\20140305.bill\\video_buf_h264.bin", std::ios_base::binary);

		AVFrame* frame;
		AVPacket pkt;

		av_init_packet(&pkt);
		frame = av_frame_alloc();

		cv::Mat3b m(240, 320);
		std::vector<uint8_t> buffer;
		while (true)
		{
			cv::imshow("m", m);

			int kf, size;
			idx >> kf >> size;

			if (size > buffer.size())
				buffer.resize(size);

			bin.read((char*)&buffer[0], size);
			pkt.data = &buffer[0];
			pkt.size = size;

			if (decoder.decode(&pkt, frame))
			{
				cv::Mat1b mat(frame->height, frame->width, frame->data[0], frame->linesize[0]);
				cv::imshow("mat", mat);
			}

			int ch = cv::waitKey(1);
			if (ch == 27)
				break;
		}

		av_frame_free(&frame);
	}

	av_free(c);

	zzlab::uninitialize();

	return 0;
}

