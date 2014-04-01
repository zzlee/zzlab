#ifndef __ZZLAB_NET_H__
#define __ZZLAB_NET_H__

#ifdef ZZNET_EXPORTS
#define ZZNET_API __declspec(dllexport)
#else
#define ZZNET_API __declspec(dllimport)
#endif

#include "zzlab.h"

#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/unordered_map.hpp>
#include <string>

namespace zzlab
{
	namespace net
	{
		ZZNET_API void install(void);

		class ZZNET_API ServiceBroker
		{
		public:
			boost::asio::ip::udp::endpoint listenEndpoint;
			boost::asio::ip::address multicastAddress;
			bool enableLoopback;
			std::string name;
			size_t maxLength;

			ServiceBroker();
			~ServiceBroker();

			void init();

		protected:
			boost::shared_ptr<boost::asio::ip::udp::socket> mSocket;

			std::string mPacket;
			std::vector<uint8_t> mData;

			boost::asio::ip::udp::endpoint mSenderEndpoint;

			boost::asio::coroutine __coro_main;
			void main(boost::system::error_code error = boost::system::error_code(), size_t bytes = 0);
		};

		class ZZNET_API ServiceDiscovery
		{
		public:
			typedef boost::unordered_map<std::string, boost::asio::ip::udp::endpoint> ServiceEndpoints;

			boost::asio::ip::udp::endpoint multicastEndpoint;
			std::pair<size_t, size_t> requestInterval;
			int responseTimeout;
			size_t maxLength;

			ServiceEndpoints serviceEndpoints;

			ServiceDiscovery();
			~ServiceDiscovery();

			void init();

		protected:
			boost::asio::deadline_timer mTimer;
			boost::asio::ip::udp::socket mSocket;

			std::string mPacket;
			std::vector<uint8_t> mData;

			boost::asio::ip::udp::endpoint mSenderEndpoint;

			boost::asio::coroutine __coro_main;
			void main(boost::system::error_code error = boost::system::error_code(), size_t bytes = 0);
			void waitForResponseTimeout(const boost::system::error_code &error);
			void updateService(const std::string &payload);
		};
	} // namespace net
} // namespace zzlab

#endif __ZZLAB_NET_H__
