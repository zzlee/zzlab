#include "stdafx.h"

#include "zzlab.h"
#include "zzlab/utils.h"
#include "zzlab/net.h"

#include <boost/bind.hpp>
#include <boost/locale.hpp>

ZZLAB_USE_LOG4CPLUS(zznet);

using namespace boost;

namespace zzlab
{
	namespace net
	{
		ServiceDiscovery::ServiceDiscovery() :
			mTimer(_MainService),
			mSocket(_MainService, asio::ip::udp::v4())
		{
			ZZLAB_TRACE_THIS();

			multicastEndpoint = asio::ip::udp::endpoint(asio::ip::address::from_string("239.255.0.1"), 31000);
			responseTimeout = 3000;
			requestInterval = std::make_pair(1000, 5000);
			maxLength = 1024;
		}

		ServiceDiscovery::~ServiceDiscovery()
		{
			ZZLAB_TRACE_THIS();

			mTimer.cancel();
			mSocket.cancel();
		}

		void ServiceDiscovery::init()
		{
			ZZLAB_TRACE_THIS();

			main();
		}

#include <boost/asio/yield.hpp>
		void ServiceDiscovery::main(system::error_code error, size_t bytes)
		{
			reenter(__coro_main)
			{
				mPacket = "Query";
				mData.resize(maxLength);

				while (true)
				{
					yield mSocket.async_send_to(asio::buffer(mPacket), multicastEndpoint,
						bind(&ServiceDiscovery::main, this,
						asio::placeholders::error,
						asio::placeholders::bytes_transferred));
					if (error)
						return;

					mTimer.expires_from_now(posix_time::milliseconds(responseTimeout));
					mTimer.async_wait(bind(&ServiceDiscovery::waitForResponseTimeout, this,
						asio::placeholders::error));
					yield mSocket.async_receive_from(asio::buffer(mData), mSenderEndpoint,
						bind(&ServiceDiscovery::main, this,
						asio::placeholders::error,
						asio::placeholders::bytes_transferred));
					if (!error)
					{
						mTimer.cancel();
						if (bytes > 9 && memcmp("Service: ", &mData[0], 9) == 0)
						{
							std::string payload((const char *)&mData[0] + 9, bytes - 9);
							updateService(payload);
						}
					}

					mTimer.expires_from_now(posix_time::milliseconds(
						requestInterval.first + rand() % (requestInterval.second - requestInterval.first)));
					yield mTimer.async_wait(bind(&ServiceDiscovery::main, this, asio::placeholders::error, 0));
					if (error)
						return;
				}
			}
		}
#include <boost/asio/unyield.hpp>

		void ServiceDiscovery::waitForResponseTimeout(const system::error_code &error)
		{
			//ZZLAB_INFO("waitForResponseTimeout: " << error);

			if (error)
				return;

			mSocket.cancel();
		}

		void ServiceDiscovery::updateService(const std::string &payload)
		{
			ServiceEndpoints::iterator i = serviceEndpoints.find(payload);
			if (i == serviceEndpoints.end())
			{
				std::wstring wPayload = locale::conv::to_utf<wchar_t>(payload, "UTF-8");
				ZZLAB_INFO(L"Service: " << wPayload << L" ! " << mSenderEndpoint);

				serviceEndpoints[payload] = mSenderEndpoint;
			}
			else if (i->second != mSenderEndpoint)
			{
				std::wstring wPayload = locale::conv::to_utf<wchar_t>(payload, "UTF-8");
				ZZLAB_INFO(L"Update service: " << wPayload << L" ! " << mSenderEndpoint);

				serviceEndpoints[payload] = mSenderEndpoint;
			}
		}
	}
}