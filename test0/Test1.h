#pragma once

#include "zzlab.h"
#include "zzlab/av.h"
#include "zzlab/utils.h"

#include <boost/asio/deadline_timer.hpp>
#include <boost/thread/mutex.hpp>

class Test1
{
public:
	Test1();
	~Test1();

	void init();

protected:
	boost::asio::deadline_timer mTimer0;

	void cvKey(boost::system::error_code err = boost::system::error_code());

	zzlab::utils::HiPerfTimer mMMTimer;
	int64_t mPlayTime;

	zzlab::av::AudioDevice mAudioDevice;

	zzlab::av::FileMediaPlayer* mMediaPlayer;

	zzlab::av::FrameQueue* mVideoQueue;
	boost::asio::deadline_timer mTimer1;
	void renderVideo(boost::system::error_code err = boost::system::error_code());
	
	zzlab::av::FrameQueue* mAudioQueue;
	zzlab::av::AudioQueueRenderer* mAudioQueueRenderer;
	//zzlab::av::NullRenderer* mNullRenderer;
	
	bool mPlaying;
	bool mStopping;

	void play();
	void stop();

	int mStopCount;
	void afterStop(std::string tag);
	void endOfStream();

	boost::asio::deadline_timer mTimer2;
	void waitForFinish(boost::system::error_code err = boost::system::error_code());
	void restart(boost::system::error_code err = boost::system::error_code());
};