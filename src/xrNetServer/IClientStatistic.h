#pragma once
#include "NET_Shared.h"

#pragma warning(push)
#pragma warning(disable:4995)
#include <DPlay/dplay8.h>
#pragma warning(pop)

class XRNETSERVER_API IClientStatistic
{
	DPN_CONNECTION_INFO	ci_last;
	u32					mps_recive, mps_receive_base;
	u32					mps_send, mps_send_base;
	u32					dwBaseTime;
	CTimer*				device_timer;
public:
	IClientStatistic(CTimer* timer) { ZeroMemory(this, sizeof(*this)); device_timer = timer; dwBaseTime = TimeGlobal(device_timer); }

	void	Update(DPN_CONNECTION_INFO& CI);

	IC u32	getPing() { return ci_last.dwRoundTripLatencyMS; }
	IC u32	getBPS() { return ci_last.dwThroughputBPS; }
	IC u32	getPeakBPS() { return ci_last.dwPeakThroughputBPS; }
	IC u32	getDroppedCount() { return ci_last.dwPacketsDropped; }
	IC u32	getRetriedCount() { return ci_last.dwPacketsRetried; }
	IC u32	getMPS_Receive() { return mps_recive; }
	IC u32	getMPS_Send() { return mps_send; }
	IC u32	getReceivedPerSec() { return dwBytesReceivedPerSec; }
	IC u32	getSendedPerSec() { return dwBytesSendedPerSec; }


	IC void	Clear() { CTimer* timer = device_timer; ZeroMemory(this, sizeof(*this)); device_timer = timer; dwBaseTime = TimeGlobal(device_timer); }

	//-----------------------------------------------------------------------
	u32		dwTimesBlocked;

	u32		dwBytesSended;
	u32		dwBytesSendedPerSec;

	u32		dwBytesReceived;
	u32		dwBytesReceivedPerSec;
};
