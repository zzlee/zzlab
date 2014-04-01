// lms1xxx.cpp : 定義主控台應用程式的進入點。
//

#include "stdafx.h"

#include <boost/math/constants/constants.hpp>

using namespace std;
using namespace boost;

asio::io_service _main;

typedef boost::shared_ptr<string> StringPtr;
typedef boost::shared_ptr<vector<uchar> > BufferPtr;
typedef boost::shared_ptr<boost::asio::streambuf> StreamBufPtr;
typedef boost::shared_ptr<vector<StringPtr> > ValuesPtr;

typedef boost::function<void(const boost::system::error_code&, StreamBufPtr)> ResponseHandler;

// basic protocal stack
void sendCommand(asio::ip::tcp::socket &socket_, StringPtr payload, ResponseHandler handler);
void afterSTXSent(const boost::system::error_code& error, asio::ip::tcp::socket &socket_, StringPtr payload, ResponseHandler handler);
void afterPayloadSent(const boost::system::error_code& error, asio::ip::tcp::socket &socket_, StringPtr payload, ResponseHandler handler);
void afterETXSent(const boost::system::error_code& error, asio::ip::tcp::socket &socket_, ResponseHandler handler);

void sendCommand(asio::ip::tcp::socket &socket_, StringPtr payload, ResponseHandler handler)
{
	static const string STX = "\x02";

	boost::asio::async_write(socket_,
		boost::asio::buffer(STX.c_str(), 1),
		boost::bind(&afterSTXSent, boost::asio::placeholders::error, boost::ref(socket_), payload, handler));
}

void afterSTXSent(const boost::system::error_code& error, asio::ip::tcp::socket &socket_, StringPtr payload, ResponseHandler handler)
{
	if (error)
	{
		cout << "Failed to send STX" << endl;
		handler(error, StreamBufPtr());
		return;
	}

	boost::asio::async_write(socket_,
		boost::asio::buffer(payload->c_str(), payload->length()),
		boost::bind(&afterPayloadSent, boost::asio::placeholders::error, boost::ref(socket_), payload, handler));
}

void afterPayloadSent(const boost::system::error_code& error, asio::ip::tcp::socket &socket_, StringPtr payload, ResponseHandler handler)
{
	static const string ETX = "\x03";

	if (error)
	{
		cout << "Failed to send payload" << endl;
		handler(error, StreamBufPtr());
		return;
	}

	boost::asio::async_write(socket_,
		boost::asio::buffer(ETX.c_str(), 1),
		boost::bind(&afterETXSent, boost::asio::placeholders::error, boost::ref(socket_), handler));
}

void afterETXSent(const boost::system::error_code& error, asio::ip::tcp::socket &socket_, ResponseHandler handler)
{
	if (error)
	{
		cout << "Failed to send ETX" << endl;
		handler(error, StreamBufPtr());
		return;
	}

	StreamBufPtr response(new boost::asio::streambuf());

	boost::asio::async_read_until(socket_, *response, "\x03",
		boost::bind(handler,
		boost::asio::placeholders::error, response));
}

StringPtr unpacktize(StreamBufPtr response_)
{
	StringPtr ret(new string);

	std::istream s(&(*response_));
	s.unsetf(std::ios_base::skipws);

	uchar ch;
	while (!s.eof())
	{
		s >> ch;
		if (ch == '\x02')
			break;
	}

	while (!s.eof())
	{
		s >> ch;
		if (ch == '\x03')
			break;

		ret->push_back(ch);
	}

	return ret;
}

ValuesPtr parsePacket(StreamBufPtr response_)
{
	ValuesPtr ret(new vector<StringPtr>);

	std::istream s(&(*response_));
	s.unsetf(std::ios_base::skipws);

	uchar ch;
	while (!s.eof())
	{
		s >> ch;
		if (ch == '\x02')
			break;
	}

	StringPtr value(new string);
	while (!s.eof())
	{
		s >> ch;
		if (ch == '\x03')
			break;
		if (ch == ' ')
		{
			value.reset(new string);
			ret->push_back(value);
			continue;
		}

		value->push_back(ch);
	}

	return ret;
}

inline int hex2int(char v)
{
	if (v >= '0' && v <= '9')
		return (int)v - '0';

	if (v >= 'A' && v <= 'F')
		return ((int)v - 'A') + 10;

	if (v >= 'a' && v <= 'f')
		return ((int)v - 'a') + 10;

	return -1;
}

int hex2int(const std::string& s)
{
	int ret = 0;
	for (size_t i = 0; i < s.size(); ++i)
	{
		ret = ret * 16 + hex2int(s[i]);
	}

	return ret;
}

cv::Point2f fromAngleDistance(float angle, float dist)
{
	float radians = angle * boost::math::constants::pi<float>() / 180.0f;

	return cv::Point2f(dist * cosf(radians), dist * sinf(radians));
}

class TrajectorySmoother : public cv::Point2f {
public:
	TrajectorySmoother() :
		mFilter(6, 2, 0), mMeasurement(2, 1) {
	}

	~TrajectorySmoother() {
	}

	void start(const cv::Point2f& pt) {
		mFilter.statePre.at<float>(0) = pt.x;
		mFilter.statePre.at<float>(1) = pt.y;
		mFilter.statePre.at<float>(2) = 0;
		mFilter.statePre.at<float>(3) = 0;
		mFilter.statePre.at<float>(4) = 0;
		mFilter.statePre.at<float>(5) = 0;

		mFilter.transitionMatrix =
			*(cv::Mat_<float>(6, 6) <<
			1, 0, 1, 0, 0.5f, 0,
			0, 1, 0, 1, 0, 0.5,
			0, 0, 1, 0, 1, 0,
			0, 0, 0, 1, 0, 1,
			0, 0, 0, 0, 1, 0,
			0, 0, 0, 0, 0, 1);
		mFilter.measurementMatrix =
			*(cv::Mat_<float>(2, 6) <<
			1, 0,
			1, 0,
			0.5, 0,
			0, 1,
			0, 1,
			0, 0.5);

		mFilter.statePre.copyTo(mFilter.statePost);

		setIdentity(mFilter.processNoiseCov, cv::Scalar::all(1e-4));
		setIdentity(mFilter.measurementNoiseCov, cv::Scalar::all(1e-1));
		setIdentity(mFilter.errorCovPost, cv::Scalar::all(.1));

		x = pt.x;
		y = pt.y;
	}

	void update(const cv::Point2f& pt) {
		cv::Mat prediction = mFilter.predict();
		cv::Point2f predictPt(prediction.at<float>(0), prediction.at<float>(1));

		mMeasurement(0) = pt.x;
		mMeasurement(1) = pt.y;
		cv::Mat estimated = mFilter.correct(mMeasurement);
		cv::Point2f statePt(estimated.at<float>(0), estimated.at<float>(1));

		x = statePt.x;
		y = statePt.y;
	}

protected:
	cv::KalmanFilter mFilter;
	cv::Mat1f mMeasurement;
};

typedef std::vector<pair<size_t, int> > Candidates;
typedef boost::shared_ptr<std::vector<pair<size_t, int> > > CandidatesPtr;

struct RadarData : public cv::Point2f
{
	RadarData() {};
	RadarData(const cv::Point2f& pt, CandidatesPtr c)
		: cv::Point2f(pt), candidates(c)
	{}

	CandidatesPtr candidates;
};

static float Distance(const cv::Point2f& a, const cv::Point2f& b)
{
	float dx = a.x - b.x;
	float dy = a.y - b.y;

	return sqrtf(dx * dx + dy * dy);
}

static void TargetTracker_onDummyEvent() {
}

template<class T>
class TargetTracker_ {
public:
	typedef unsigned long ID;

	float trackMinDist;
	int trackInitialLiveness;

	//boost::function<void(ID, const T&)> onAdded;
	//boost::function<void(ID, const T&)> onMove;
	//boost::function<void(ID)> onRemove;

	TargetTracker_()
	{
		//onAdded = boost::bind(&TargetTracker_onDummyEvent);
		//onMove = boost::bind(&TargetTracker_onDummyEvent);
		//onRemove = boost::bind(&TargetTracker_onDummyEvent);

		trackMinDist = 200.0f; // minimal distance to track in millimeter
		trackInitialLiveness = 5; // liveness detection
	}

	virtual ~TargetTracker_()
	{
		for (hash_map<ID, TrackInfo*>::const_iterator i = mTargets.begin();
			i != mTargets.end(); ++i) {
			delete (*i).second;
		}
	}

	void track(const vector<T>& pts)
	{
		// step 1: clear
		for (hash_map<ID, TrackInfo*>::const_iterator i = mTargets.begin();
			i != mTargets.end(); ++i) {
			TrackInfo* info = (*i).second;
			--info->liveness;
		}

		// step 2: update / add
		for (vector<T>::const_iterator i = pts.begin(); i != pts.end(); ++i) {
			const T& pt = *i;

			bool found = false;
			for (hash_map<ID, TrackInfo*>::const_iterator j = mTargets.begin();
				j != mTargets.end(); ++j) {
				TrackInfo* info = (*j).second;
				float dist = Distance(info->lastPosition, pt);
				if (dist < trackMinDist) {
					++info->liveness;
					info->trajectory.update(pt);
					//if (info->lastPosition != pt)
					//	onMove((*j).first, info->trajectory);
					info->lastPosition = pt;
					found = true;
					break;
				}
			}

			if (!found) {
				TrackInfo* info = new TrackInfo;
				info->liveness = trackInitialLiveness;
				info->trajectory.start(pt);
				info->lastPosition = pt;

				ID id = nextID();
				mTargets[id] = info;

				//onAdded(id, pt);
			}
		}

		// step 3: remove
		for (hash_map<ID, TrackInfo*>::iterator i = mTargets.begin();
			i != mTargets.end();) {
			TrackInfo*& info = (*i).second;
			if (info->liveness) {
				++i;
				continue;
			}

			//onRemove((*i).first);
			delete info;
			info = NULL;

			mTargets.erase(i++);
		}
	}

	void enumerateAll(
		boost::function<void(ID, const T&, const cv::Point2f&)> cb) const
	{
		for (hash_map<ID, TrackInfo*>::const_iterator i = mTargets.begin();
			i != mTargets.end(); ++i) {
			cb((*i).first, (*i).second->lastPosition, (*i).second->trajectory);
		}
	}

protected:
	static ID gSeed;
	static ID nextID()
	{
		return gSeed++;
	}

	struct TrackInfo {
		int liveness;
		T lastPosition;
		TrajectorySmoother trajectory;
	};
	hash_map<ID, TrackInfo*> mTargets;
};

typedef TargetTracker_<RadarData> TargetTracker;
TargetTracker::ID TargetTracker::gSeed = 0;

bool SortBy(const std::pair<size_t, int>& a, const std::pair<size_t, int>& b)
{
	return a.second < b.second;
}

static cv::Scalar gColors[] = { cv::Scalar(255, 0, 0), cv::Scalar(0, 255, 0), cv::Scalar(0, 0,
	255), cv::Scalar(255, 255, 0), cv::Scalar(255, 0, 255), cv::Scalar(0, 255, 255),
	cv::Scalar(255, 255, 255), cv::Scalar(128, 0, 0), cv::Scalar(0, 128, 0), cv::Scalar(0,
	0, 128), cv::Scalar(128, 128, 0), cv::Scalar(128, 0, 128), cv::Scalar(0,
	128, 128), cv::Scalar(128, 128, 128), };

class Main
{
public:
	Main() : mUITimer(_main), mSocket(_main), mTouchCorners(4, 1), mScreenSize(1, 1), mTouchPad(700, 1300)
	{
		mTouchOffset.x = mTouchPad.cols / 2.0f;
		mTouchOffset.y = /*canvas.rows / 2.0f*/ 0;

		mTouchCorners[0][0] = mTouchCorners[1][0] = mTouchCorners[2][0] =
			mTouchCorners[3][0] = cv::Vec2f(-1, -1);
		mScreenSize[0][0] = cv::Vec2f(mTouchPad.cols, mTouchPad.rows);
	}

	~Main()
	{}

	void init()
	{
		cv::namedWindow("LMS1xxx");

		mDataDirty = false;
		mBackgroundDirty = false;
		mResetCorners = false;
		_main.post(boost::bind(&Main::step, this, system::error_code()));

		boost::asio::ip::tcp::endpoint endpoint(asio::ip::address::from_string("192.168.0.222"), 2111);
		mSocket.async_connect(endpoint,
			boost::bind(&Main::afterConnected, this, boost::asio::placeholders::error));
	}

protected:
	asio::deadline_timer mUITimer;
	asio::ip::tcp::socket mSocket;
	StreamBufPtr mResponse;

	float mStartAngle;
	float mStep;
	vector<uint16_t> mData;
	bool mDataDirty;

	vector<uint16_t> mBackground;
	bool mBackgroundDirty;

	TargetTracker mTracker;
	cv::Point2f mCurrentPoint;

	bool mResetCorners;
	size_t mCurrentCornerIndex;
	cv::Mat2f mTouchCorners;
	cv::Mat mTouchToScreen;
	cv::Mat2f mScreenSize;

	cv::Mat3b mTouchPad;
	cv::Point2f mTouchOffset;

	void afterStopScanData()
	{
		cout << "afterStopScanData" << endl;
	}

	void afterStartScanData()
	{
		cout << "afterStartScanData" << endl;
	}

	void calcTouchToScreenMapping() {
		cout << "Calculate touch plane to screen mapping..." << endl;

		cv::Mat2f dst(4, 1);
		dst << cv::Vec2f(0.25f, 0.25f), cv::Vec2f(0.75f, 0.25f), cv::Vec2f(0.75f, 0.75f), cv::Vec2f(
			0.25f, 0.75f);

		mTouchToScreen = cv::getPerspectiveTransform(mTouchCorners, dst);

		cout << mTouchToScreen << endl;
	}

	void saveConfig() {
		cout << "Saving configuration file..." << endl;

		cv::FileStorage fs("settings.xml", cv::FileStorage::WRITE);
		fs << "corners" << mTouchCorners;
	}

	void loadConfig()
	{
		cout << "Loading configuration file..." << endl;

		cv::FileStorage fs("settings.xml", cv::FileStorage::READ);

		if (fs.isOpened()) {
			fs["corners"] >> mTouchCorners;
			calcTouchToScreenMapping();
		}
	}

	void step(const system::error_code &e)
	{
		int ch = cv::waitKey(1);
		if (ch >= 0)
		{
			switch (ch)
			{
			case 'b':
			case 'B':
				mBackgroundDirty = true;
				break;

			case 'c':
			case 'C':
				mResetCorners = true;
				mCurrentCornerIndex = 0;
				break;

			case ' ':
				if (mResetCorners)
				{
					mTouchCorners[mCurrentCornerIndex][0][0] = mCurrentPoint.x;
					mTouchCorners[mCurrentCornerIndex][0][1] = mCurrentPoint.y;

					cout << "Corner " << mCurrentCornerIndex << ": " << mCurrentPoint << endl;

					if (mCurrentCornerIndex == 3) {
						calcTouchToScreenMapping();
						mResetCorners = false;
					}

					mCurrentCornerIndex = (mCurrentCornerIndex + 1) % 4;
				}
				break;

			case 'l':
			case 'L':
				loadConfig();
				break;

			case 'w':
			case 'W':
				saveConfig();
				break;

			case 's':
			case 'S':
			{
				StringPtr payload(new string("sEN LMDscandata 0"));
				sendCommand(boost::ref(mSocket), payload, boost::bind(&Main::afterStopScanData, this));
			}
				break;

			case 'r':
			case 'R':
			{
				StringPtr payload(new string("sEN LMDscandata 1"));
				sendCommand(boost::ref(mSocket), payload, boost::bind(&Main::afterStartScanData, this));
			}
				break;

			case 'q':
			case 'Q':
			case 27:
				_main.stop();
				return;

			case 2424832:
				//cout << "left" << endl;
				mTouchOffset.x -= 10;
				break;

			case 2555904:
				//cout << "right" << endl;
				mTouchOffset.x += 10;
				break;
			}
		}

		if (mDataDirty)
		{
			if (mBackgroundDirty)
			{
				cout << "Reset background" << endl;

				mBackground = mData;
				mBackgroundDirty = false;
			}

			// plot all
			plotAll();

			mDataDirty = false;
		}

		mUITimer.expires_from_now(posix_time::milliseconds(5));
		mUITimer.async_wait(boost::bind(&Main::step, this, asio::placeholders::error));
	}

	void afterConnected(const boost::system::error_code& error)
	{
		if (error)
		{
			cout << "Failed to connect" << endl;
			return;
		}
		else
		{
			cout << "Connected" << endl;

			//StringPtr payload(new string("sMN SetAccessMode 02 B21ACE26")); // Maintenance
			StringPtr payload(new string("sMN SetAccessMode 03 F4724744")); // Authorized Client
			//StringPtr payload(new string(payload = "sMN SetAccessMode 04 81BE23AA")); // Service

			sendCommand(boost::ref(mSocket), payload, boost::bind(&Main::afterLogin, this, _1, _2));
		}
	}

	void afterLogin(const boost::system::error_code& error, StreamBufPtr response)
	{
		if (error)
		{
			cout << "Failed to LOGIN" << endl;
			return;
		}

		StringPtr data = unpacktize(response);
		cout << *data << endl;

		StringPtr payload(new string("sMN Run"));
		sendCommand(boost::ref(mSocket), payload, boost::bind(&Main::afterRun, this, _1, _2));
	}

	void afterRun(const boost::system::error_code& error, StreamBufPtr response)
	{
		if (error)
		{
			cout << "Failed to RUN" << endl;
			return;
		}

		StringPtr data = unpacktize(response);
		cout << *data << endl;

		StringPtr payload(new string("sEN LMDscandata 1"));
		sendCommand(boost::ref(mSocket), payload, boost::bind(&Main::afterScanData, this, _1, _2));
	}

	void afterScanData(const boost::system::error_code& error, StreamBufPtr response)
	{
		if (error)
		{
			cout << "Failed to SCANDATA" << endl;
			return;
		}

		//StringPtr data = unpacktize(response);
		//cout << *data << endl << endl;

		ValuesPtr values = parsePacket(response);
		if (values->size() > 24)
		{
			int cnt = hex2int(*(*values)[24]);
			if (values->size() - 24 >= cnt)
			{
				mData.resize(cnt);
				for (size_t i = 0; i < cnt; ++i)
				{
					mData[i] = hex2int((*values)[i + 24]->c_str());
				}
			}

			mStartAngle = hex2int(*(*values)[22]) / 10000.0f;
			mStep = hex2int(*(*values)[23]) / 10000.0f;
		}

		mDataDirty = true;

		// receive more
		boost::asio::async_read_until(mSocket, *response, "\x03",
			boost::bind(boost::bind(&Main::afterScanData, this, _1, _2),
			boost::asio::placeholders::error, response));
	}

	void drawPointsOnResetCorner(TargetTracker::ID id, const cv::Point2f& pt, const cv::Point2f& smooth)
	{
		const float scale = 1 / 3.0f;
		const cv::Scalar& clr = gColors[id % (sizeof(gColors) / sizeof(gColors[0]))];

		cv::circle(mTouchPad, smooth * scale + mTouchOffset, 8, clr, CV_FILLED);
		cv::circle(mTouchPad, pt * scale + mTouchOffset, 3, clr, CV_FILLED);

		mCurrentPoint = smooth;
	}

	void drawPoints(TargetTracker::ID id, const RadarData& pt, const cv::Point2f& smooth)
	{
		const float w = mScreenSize[0][0][0];
		const float h = mScreenSize[0][0][1];

		cv::Mat2f dst0;
		cv::perspectiveTransform(*(cv::Mat2f(1, 1) << cv::Vec2f(pt.x, pt.y)), dst0,
			mTouchToScreen);
		cv::Point2f pt0(dst0[0][0][0] * w, dst0[0][0][1] * h);

		cv::Mat2f dst1;
		cv::perspectiveTransform(*(cv::Mat2f(1, 1) << cv::Vec2f(smooth.x, smooth.y)), dst1,
			mTouchToScreen);
		cv::Point2f pt1(dst1[0][0][0] * w, dst1[0][0][1] * h);

		const cv::Scalar& clr = gColors[id % (sizeof(gColors) / sizeof(gColors[0]))];
		cv::circle(mTouchPad, pt1, 8, cv::Scalar(255, 255, 255), CV_FILLED);
		cv::circle(mTouchPad, pt0, 3, cv::Scalar(255, 0, 255), CV_FILLED);

#if 1
		{
			const Candidates& candidates = *pt.candidates;

			for (Candidates::const_iterator i = candidates.begin(); i != candidates.end(); ++i)
			{
				const Candidates::value_type& v = *i;

				float angle = mStartAngle + mStep * v.first;
				float dist = v.second; // in millimeter

				cv::Point2f pt0 = fromAngleDistance(angle, dist);
				cv::Mat2f dst0;
				cv::perspectiveTransform(*(cv::Mat2f(1, 1) << cv::Vec2f(pt0.x, pt0.y)), dst0,
					mTouchToScreen);
				cv::Point2f pt1(dst0[0][0][0] * w, dst0[0][0][1] * h);

				cv::circle(mTouchPad, pt1, 2, cv::Scalar(0, 255, 255));
			}
		}
#endif
	}

	void plotAll()
	{
		static bool blink = false;

		static const int T0 = 100; // 100mm
		static const float T1 = 0.1f; // 0.1m
		static const size_t T2 = 1; // minimal support

		if (mData.empty())
			return;

#if 1
		{
			cv::Mat3b canvas(128, 128);
			canvas.setTo(cv::Vec3b(0, 0, 0));

			if (blink)
				cv::circle(canvas, cv::Point(10, 10), 10, cv::Scalar(0, 0, 255), CV_FILLED);
			blink = !blink;

			cv::imshow("LMS1xxx", canvas);
		}
#endif

		// plot raw data
#if 0
		{
			cv::Mat3b canvas(128, (int)mData.size());
			canvas.setTo(cv::Vec3b(0, 0, 0));

			cv::Point pt0(0, mData[0] * canvas.rows / 20000.0f);
			for (size_t i = 0; i < mData.size(); ++i)
			{
				cv::Point pt1(i, mData[i] * canvas.rows / 20000.0f);
				cv::line(canvas, pt0, pt1, cv::Scalar(0, 255, 0));

				pt0 = pt1;
			}
			cv::imshow("raw", canvas);
		}
#endif

		// plot projected view
#if 0
		{
			cv::Mat3b canvas(512, 768);
			canvas.setTo(cv::Vec3b(0, 0, 0));

			cv::Point2f center(canvas.cols / 2.0f, canvas.rows / 2.0f);
			float scale = 1.0f / 20.0f;

			cv::Point2f pt0 = fromAngleDistance(mStartAngle, mData[0] * scale);
			for (size_t i = 0; i < mData.size(); ++i)
			{
				float angle = mStartAngle + mStep * i;
				float dist = mData[i]; // in millimeter

				cv::Point2f pt1 = fromAngleDistance(angle, dist * scale);
				cv::line(canvas, pt0 + center, pt1 + center, cv::Scalar(0, 255, 0));

				pt0 = pt1;
			}
			cv::imshow("projected", canvas);
		}
#endif

		// plot raw detection
#if 0
		if (mBackground.size() == mData.size())
		{
			cv::Mat3b canvas(512, 768);
			canvas.setTo(cv::Vec3b(0, 0, 0));

			//cv::Point pt0(0, mData[0] * canvas.rows / 20000.0f);
			for (size_t i = 0; i < mData.size(); ++i)
			{
				int dist = mBackground[i] - mData[i];

				if (dist > T0)
				{
					cv::Point pt1(i, mData[i] * canvas.rows / 20000.0f);
					//cv::line(canvas, pt0, pt1, cv::Scalar(0, 255, 0));
					cv::circle(canvas, pt1, 1, cv::Scalar(255, 0, 0));

					//pt0 = pt1;
				}
			}

			cv::imshow("raw detection", canvas);
		}
#endif

		// track target detection
		std::vector<RadarData> points;
		if (mBackground.size() == mData.size())
		{
			CandidatesPtr candidates(new Candidates);
			for (size_t i = 0; i < mData.size(); ++i)
			{
				int dist = mBackground[i] - mData[i];
				if (dist > T0)
				{
					candidates->push_back(make_pair(i, mData[i]));
				}
				else if (!candidates->empty())
				{
					if (candidates->size() > T2)
					{
						sort(candidates->begin(), candidates->end(), SortBy);

						size_t idx = 0; // pick the max distance
						float angle = mStartAngle + mStep * (*candidates)[idx].first;
						float dist = (*candidates)[idx].second; // in millimeter

						points.push_back(RadarData(fromAngleDistance(angle, dist), candidates));
					}

					candidates.reset(new Candidates);
				}
			}
		}

		// plot detection
#if 0
		{
			cv::Mat3b canvas(700, 1300);
			canvas.setTo(cv::Vec3b(0, 0, 0));

			cv::Point2f center(canvas.cols / 2.0f, /*canvas.rows / 2.0f*/ 0);
			const float scale = 5.0f / 20.0f;

			for (size_t i = 0; i < points.size(); ++i)
			{
				const cv::Point2f& pt1 = points[i];
				cv::circle(canvas, pt1 * scale + center, 5, cv::Scalar(0, 0, 255), 1);
			}

			cv::imshow("detection", canvas);
		}
#endif

		// tracking		
		mTracker.track(points);

		// draw touchpad
		{
			mTouchPad.setTo(cv::Vec3b(0, 0, 0));

			if (mResetCorners)
			{
				const cv::Point2f lt(mTouchPad.cols * 0.25f, mTouchPad.rows * 0.25f);
				const cv::Point2f rb(mTouchPad.cols * 0.75f, mTouchPad.rows * 0.75f);
				const float scale = 1 / 3.0f;

				cv::rectangle(mTouchPad, lt, rb, cv::Scalar(255, 255, 255), CV_FILLED);

				// draw projected signals
				{
					cv::Point2f pt0 = fromAngleDistance(mStartAngle, mData[0] * scale);
					for (size_t i = 0; i < mData.size(); ++i)
					{
						float angle = mStartAngle + mStep * i;
						float dist = mData[i]; // in millimeter

						cv::Point2f pt1 = fromAngleDistance(angle, dist * scale);
						cv::line(mTouchPad, pt0 + mTouchOffset, pt1 + mTouchOffset, cv::Scalar(0, 255, 0));

						pt0 = pt1;
					}
				}

				for (size_t i = 0; i < 4; ++i) {
					cv::circle(mTouchPad, cv::Point2f(mTouchCorners[i][0]) * scale + mTouchOffset, 5,
						cv::Scalar(0, 0, 255));
				}
				cv::line(mTouchPad, cv::Point2f(mTouchCorners[0][0]) * scale + mTouchOffset,
					cv::Point2f(mTouchCorners[1][0]) * scale + mTouchOffset, cv::Scalar(0, 0, 255));
				cv::line(mTouchPad, cv::Point2f(mTouchCorners[1][0]) * scale + mTouchOffset,
					cv::Point2f(mTouchCorners[2][0]) * scale + mTouchOffset, cv::Scalar(0, 0, 255));
				cv::line(mTouchPad, cv::Point2f(mTouchCorners[2][0]) * scale + mTouchOffset,
					cv::Point2f(mTouchCorners[3][0]) * scale + mTouchOffset, cv::Scalar(0, 0, 255));
				cv::line(mTouchPad, cv::Point2f(mTouchCorners[3][0]) * scale + mTouchOffset,
					cv::Point2f(mTouchCorners[0][0]) * scale + mTouchOffset, cv::Scalar(0, 0, 255));

				mTracker.enumerateAll(
					boost::bind(&Main::drawPointsOnResetCorner, this, _1, _2, _3));
			}
			else
			{
				mTracker.enumerateAll(
					boost::bind(&Main::drawPoints, this, _1, _2, _3));
			}

			cv::imshow("touchpad", mTouchPad);
		}
	}
};

int _tmain(int argc, _TCHAR* argv[])
{
	{
		Main main;
		main.init();

		_main.run();
	}

	return 0;
}
