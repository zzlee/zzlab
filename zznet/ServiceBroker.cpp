#include "stdafx.h"

#include "zzlab.h"
#include "zzlab/utils.h"
#include "zzlab/net.h"

#include <boost/bind.hpp>
#include <boost/make_shared.hpp>

ZZLAB_USE_LOG4CPLUS(zznet);

using namespace boost;

namespace zzlab
{
	namespace net
	{
		ServiceBroker::ServiceBroker()
		{
			ZZLAB_TRACE_THIS();

			listenEndpoint = asio::ip::udp::endpoint(asio::ip::address::from_string("0.0.0.0"), 31000);
			multicastAddress = asio::ip::address::from_string("239.255.0.1");
			enableLoopback = false;
			name = "dummy";
			maxLength = 1024;
		}

		ServiceBroker::~ServiceBroker()
		{
			ZZLAB_TRACE_THIS();

			//if (__coro_main.is_parent())
			//	ZZLAB_TRACE_THIS();

			//if (mSocket.get())
			//{
			//	try
			//	{
			//		mSocket->cancel();
			//	}
			//	catch (...) {}
			//}
		}

		void ServiceBroker::init()
		{
			ZZLAB_TRACE_THIS();

			main();
		}

#include <boost/asio/yield.hpp>
		void ServiceBroker::main(boost::system::error_code error, size_t bytes)
		{
			reenter(__coro_main)
			{
				mPacket = "Service: " + name;
				mData.resize(maxLength);

				do
				{
					mSocket = make_shared<asio::ip::udp::socket>(ref(_MainService), listenEndpoint.protocol());
					mSocket->set_option(boost::asio::ip::udp::socket::reuse_address(true));
					mSocket->set_option(boost::asio::ip::multicast::enable_loopback(enableLoopback));
					mSocket->bind(listenEndpoint);
					mSocket->set_option(boost::asio::ip::multicast::join_group(multicastAddress));

					yield mSocket->async_receive_from(
						boost::asio::buffer(mData), mSenderEndpoint,
						boost::bind(&ServiceBroker::main, this,
						boost::asio::placeholders::error,
						boost::asio::placeholders::bytes_transferred));
					if (error)
						return;

					fork ServiceBroker(*this).main(error, bytes);
				} while (__coro_main.is_parent());

				if (bytes == 5 && memcmp("Query", &mData[0], 5) == 0)
				{
					yield mSocket->async_send_to(
						boost::asio::buffer(mPacket), mSenderEndpoint,
						boost::bind(&ServiceBroker::main, *this,
						boost::asio::placeholders::error,
						boost::asio::placeholders::bytes_transferred));
					if (error)
						return;
				}
			}
		}
#include <boost/asio/unyield.hpp>
	}
}
