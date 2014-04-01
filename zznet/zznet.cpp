// zznet.cpp : �w�q DLL ���ε{�����ץX�禡�C
//

#include "stdafx.h"
#include "zzlab.h"
#include "zzlab/net.h"

ZZLAB_USE_LOG4CPLUS(zznet);

namespace zzlab
{
	namespace net
	{
		static bool initialize()
		{
			ZZLAB_TRACE_FUNCTION();

			return true;
		}

		static void uninitialize()
		{
			ZZLAB_TRACE_FUNCTION();
		}

		void install(void)
		{
			Plugin plugin = {
				L"zznet", initialize, uninitialize
			};

			addPlugin(plugin);
		}
	}
}
