#pragma once
#include "pch.h"
#include "comdef.h"
#include <string>
#include <iostream>
namespace RDMA
{
	#define RECV    0
	#define SEND    1
	#define BIND    2
	#define RECV_CTXT ((void *) 0x1000)
	#define SEND_CTXT ((void *) 0x2000)
	#define READ_CTXT ((void *) 0x3000)
	#define WRITE_CTXT ((void *) 0x4000)
	struct PeerInfo
	{
		UINT32 m_remoteToken;
		DWORD  m_nIncomingReadLimit;
		UINT64 m_remoteAddress;
	};

	struct RDMADataFormat
	{
		SIZE_T MyMemOffset;
		SIZE_T OppositeMemOffset;
		SIZE_T Size;
	};

	struct ROIBatchedInfo
	{
		SIZE_T myMemW;
		SIZE_T otherMemW;
		SIZE_T startX;
		SIZE_T startY;
		SIZE_T MyMemOffset;
		SIZE_T OtherMemOffset;
		int count;
		int* other_startX;
		int* other_startY;
		int* nWidth;
		int* nHeight;
	};

	const USHORT x_DefaultPort = 54326;
	const int m_msTimeOut = 5000;
	const SIZE_T x_MaxXfer = (1024);
	//const SIZE_T x_HdrLen = 0;
	const SIZE_T x_MaxVolume = (500 * x_MaxXfer);
	const SIZE_T x_MaxIterations = 500000;
	std::string hresult_to_string(HRESULT hr);
	bool checkResultOK(const std::string& res);
	void Log(std::string msg);
}
