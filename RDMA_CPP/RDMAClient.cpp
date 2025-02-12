#include "pch.h"
#include "RDMAClient.h"
using namespace RDMA;

RDMAClient::RDMAClient()
{

}

std::string RDMAClient::Assign_IPAdress(LPWSTR ip, USHORT port)
{
	WSADATA wsaData;
	int ret = ::WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (ret != 0)
	{
		return "Failed to initialize Windows Sockets: " + ret;
	}

	INT len = sizeof(v4);
	WSAStringToAddress(
		ip,
		AF_INET,
		NULL,
		reinterpret_cast<struct sockaddr*>(&v4),
		&len
	);
	if (v4.sin_port == 0)
	{
		v4.sin_port = htons(port);
	}
	pOv = &ov;
	pOv->hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (pOv->hEvent == NULL)
	{
		return "Failed to allocate event for overlapped operations";
	}
	hr = NdStartup();
	if (FAILED(hr))
	{
		return "NdStartup failed with " + hresult_to_string(hr);
	}

	SIZE_T Len = sizeof(v4_Local);
	hr = NdResolveAddress(
		(const struct sockaddr*)&v4,
		sizeof(v4),
		(struct sockaddr*)&v4_Local,
		&Len
	);
	if (FAILED(hr))
	{
		return "NdResolveAddress failed with " + hresult_to_string(hr);
	}
	return "OK";
}

std::string RDMAClient::OpenAdapter()
{
	hr = NdOpenAdapter(
		IID_IND2Adapter,
		reinterpret_cast<const struct sockaddr*>(&v4_Local),
		sizeof(v4_Local),
		reinterpret_cast<void**>(&pAdapter)
	);
	if (FAILED(hr))
	{
		return "NdOpenV1Adapter failed with " + hresult_to_string(hr);
	}

	hr = pAdapter->CreateOverlappedFile(&hAdapterFile);
	if (FAILED(hr))
	{
		return "IND2Adapter CreateOverlappedFile failed with" + hresult_to_string(hr);
	}
	m_bConnected = true;
	return "OK";
}

std::string RDMAClient::SendAdapterInfoQuery(int& queueDepth, bool bRead)
{
	bReadMode = bRead;
	pAdapterInfo = &Adapter_Info;
	memset(pAdapterInfo, 0, sizeof(*pAdapterInfo));
	pAdapterInfo->InfoVersion = ND_VERSION_2;
	ULONG AdapterInfo_Len = sizeof(*pAdapterInfo);
	hr = pAdapter->Query(pAdapterInfo, &AdapterInfo_Len);
	if (FAILED(hr))
	{
		return "INDAdapter::Query Failed with " + hresult_to_string(hr);
	}
	CompleteQueueDepth = (CompleteQueueDepth > 0) ? min(CompleteQueueDepth, pAdapterInfo->MaxCompletionQueueDepth) : pAdapterInfo->MaxCompletionQueueDepth;
	CompleteQueueDepth = min(CompleteQueueDepth, pAdapterInfo->MaxInitiatorQueueDepth) - 100;
	ReceiveQueueDepth = 10;
	//if (bReadMode)
	//{
	//	QueueDepth = min(QueueDepth, pAdapterInfo->MaxOutboundReadLimit);
	//}
	queueDepth = CompleteQueueDepth;
	Sgl = new (std::nothrow) ND2_SGE[nSge];
	SglBatchRead = new (std::nothrow) ND2_SGE[nSge];
	pResult = new (std::nothrow) ND2_RESULT[CompleteQueueDepth];
	return "OK";
}

std::string RDMAClient::AdapterRegisterMemory(LPWSTR MMF_id, SIZE_T size)
{
	//여기서부터
	hr = pAdapter->CreateMemoryRegion(
		IID_IND2MemoryRegion,
		hAdapterFile,
		reinterpret_cast<VOID**>(&pMr)
	);
	HANDLE hMap = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, MMF_id);

	Buffer = MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
	BuffSize = size;
	ULONG flags = ND_MR_FLAG_RDMA_READ_SINK || ND_MR_FLAG_ALLOW_LOCAL_WRITE;

	hr = pMr->Register(Buffer, BuffSize, flags, pOv);
	if (hr == ND_PENDING)
	{
		SIZE_T BytesRet;
		int temp = 0;
		do
		{
			if (temp > m_msTimeOut) return "INDCompletionQueue::Notify failed with GetOverlappedResult TimeOut";
			hr = pMr->GetOverlappedResult(pOv, FALSE);
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			temp += 10;
		} while (hr == ND_PENDING);
	}
	if (FAILED(hr) || hr == ND_TIMEOUT)
	{
		return "RegisterMemory Failed with " + hresult_to_string(hr);
	}
	return "OK";
}

std::string RDMAClient::SetupSGE()
{
	MemToken = pMr->GetLocalToken();
	for (int i = 0; i < nSge; i++)
	{
		Sgl[i].MemoryRegionToken = MemToken;
		SglBatchRead[i].MemoryRegionToken = MemToken;
	}
	Sgl[0].Buffer = Buffer;
	Sgl[0].BufferLength = (ULONG)x_MaxXfer;

	hr = pQp->Receive(RECV_CTXT, Sgl, 1);
	if (FAILED(hr))
	{
		return "Failed to allocate SGL";
	}
	return "OK";
}

std::string RDMAClient::CreateCQ()
{
	hr = pAdapter->CreateCompletionQueue(
		IID_IND2CompletionQueue,
		hAdapterFile,
		CompleteQueueDepth,
		0,
		0,
		reinterpret_cast<VOID**>(&pCq)
	);
	if (FAILED(hr))
	{
		return "INDAdapter::CreateCompletionQueue failed with " + hresult_to_string(hr);
	}
	hr = pAdapter->CreateCompletionQueue(
		IID_IND2CompletionQueue,
		hAdapterFile,
		ReceiveQueueDepth,
		0,
		0,
		reinterpret_cast<VOID**>(&pRq)
	);
	if (FAILED(hr))
	{
		return "INDAdapter::CreateCompletionQueue failed with " + hresult_to_string(hr);
	}
	return "OK";
}

std::string RDMAClient::CreateEndPoint()
{
	hr = pAdapter->CreateConnector(
		IID_IND2Connector,
		hAdapterFile,
		reinterpret_cast<VOID**>(&pConnector));
	if (FAILED(hr))
	{
		return "INDAdapter::CreateConnector failed with " + hresult_to_string(hr);
	}
	//min(QueueDepth, pAdapterInfo->MaxReceiveQueueDepth)
	hr = pAdapter->CreateQueuePair(
		IID_IND2QueuePair,
		pRq,
		pCq,
		nullptr,
		ReceiveQueueDepth,
		CompleteQueueDepth,
		nSge,
		nSge,
		0,
		reinterpret_cast<VOID**>(&pQp)
	);
	if (FAILED(hr))
	{
		return "INDConnector::CreateEndpoint failed with " + hresult_to_string(hr);
	}
	return "OK";
}

std::string RDMAClient::Receive_MWDescriptor()
{
	WaitForMWReceive(RECV_CTXT);

	PeerInfo* pInfo = reinterpret_cast<PeerInfo*>(Buffer);
	remoteToken = pInfo->m_remoteToken;
	remoteAddress = pInfo->m_remoteAddress;
	availCredits = CompleteQueueDepth;

	return "OK";
}

std::string RDMAClient::Connect()
{
	hr = pConnector->Bind(
		reinterpret_cast<const sockaddr*>(&v4_Local),
		sizeof(v4_Local)
	);
	if (hr == ND_PENDING)
	{
		hr = pConnector->GetOverlappedResult(pOv, TRUE);
	}
	hr = pConnector->Connect(
		pQp,
		reinterpret_cast<const sockaddr*>(&v4),
		sizeof(v4),
		0,
		bReadMode ? CompleteQueueDepth : 0,
		NULL,
		0,
		pOv
	);
	if (hr == ND_PENDING)
	{
		SIZE_T BytesRet;
		int temp = 0;
		do
		{
			if (temp > m_msTimeOut) return "INDConnector::Notify failed with GetOverlappedResult TimeOut";
			hr = pConnector->GetOverlappedResult(pOv, FALSE);
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			temp += 10;
		} while (hr == ND_PENDING);
	}
	if (FAILED(hr))
	{
		return "INDEndpoint::Connect failed with " + hresult_to_string(hr);
	}
	return "OK";
}


std::string RDMAClient::CompleteConnect()
{
	ULONG* OutboundReadLimit = 0;
	///*hr = pConnector->GetPrivateData(
	//	nullptr,
	//	OutboundReadLimit
	//);*/
	//if (FAILED(hr) && hr != ND_BUFFER_OVERFLOW)
	//{
	//	return "NdGetCallerData failed with " + hresult_to_string(hr);
	//}
	hr = pConnector->CompleteConnect(pOv);
	if (hr == ND_PENDING)
	{
		int temp = 0;
		do
		{
			if (temp > m_msTimeOut) return "INDConnector::Notify failed with GetOverlappedResult TimeOut";
			hr = pConnector->GetOverlappedResult(pOv, FALSE);
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			temp += 10;
		} while (hr == ND_PENDING);
	}
	if (FAILED(hr) || hr == ND_TIMEOUT)
	{
		return "INDEndpoint::CompleteConnect failed with " + hresult_to_string(hr);
	}
	return "OK";
}
std::string RDMAClient::Read(RDMADataFormat data)
{
	Sgl[0].BufferLength = (ULONG)data.Size;
	Sgl[0].Buffer = (BYTE*)Buffer + data.MyMemOffset;
	hr = pQp->Read(READ_CTXT, Sgl, 1, remoteAddress + data.OppositeMemOffset, remoteToken, 0);
	if (FAILED(hr))
	{
		return "pQp Read failed with " + hresult_to_string(hr);
	}

	WaitForCompletion(pResult, 1);
	if (pResult[0].Status != S_OK)
	{
		return "RDMA Read WaitForCompletion Fail, ND2_Result Status Not OK";
	}
	if (pResult[0].RequestContext)
	{
		return "RDMA Read WaitForCompletion Fail, ND2_Result RequestContext Not Read";
	}
	return "OK";
}

std::string RDMAClient::Read(std::vector<RDMADataFormat> datas)
{
	SIZE_T sendcnt;
	SIZE_T remain = datas.size();
	RDMADataFormat data;
	ULONG m_inlineThreshold = pAdapterInfo->InlineRequestThreshold;

	if (remain > CompleteQueueDepth) sendcnt = CompleteQueueDepth;
	else sendcnt = remain;

	for (int i = 0; i < sendcnt; i += 1)
	{
		data = datas.back();
		Sgl[0].BufferLength = (ULONG)data.Size;
		Sgl[0].Buffer = (BYTE*)Buffer + data.MyMemOffset;
		hr = pQp->Read(READ_CTXT, Sgl, 1, remoteAddress + data.OppositeMemOffset, remoteToken, 0);
		if (FAILED(hr))
		{
			return "pQp Read failed with " + hresult_to_string(hr);
		}
		else datas.pop_back();
	}
	WaitForCompletion(pResult, sendcnt);
	if (pResult[0].Status != S_OK || pResult[sendcnt / 2].Status != S_OK || pResult[sendcnt - 1].Status != S_OK)
	{
		return "RDMA Read WaitForCompletion Fail, ND2_Result Status Not OK";
	}
	if (pResult[0].RequestContext != READ_CTXT || pResult[sendcnt / 2].RequestContext != READ_CTXT || pResult[sendcnt - 1].RequestContext != READ_CTXT)
	{
		return "RDMA Read WaitForCompletion Fail, ND2_Result RequestContext Not Read";
	}
	if (datas.size() > 0) Read(datas);
	return "OK";
}

std::string RDMAClient::DestroyConnection()
{
	if (!m_bConnected) return "OK";
	if (pConnector)
	{
		try
		{
			hr = pConnector->Disconnect(pOv);
			if (hr == ND_PENDING)
			{
				SIZE_T BytesRet;
				int temp = 0;
				do
				{
					if (temp > m_msTimeOut) return "INDCompletionQueue::Notify failed with GetOverlappedResult TimeOut";
					hr = pConnector->GetOverlappedResult(pOv, FALSE);
					std::this_thread::sleep_for(std::chrono::milliseconds(10));
					temp += 10;
				} while (hr == ND_PENDING);
			}
			else if (hr == ND_CONNECTION_INVALID);
			else if (FAILED(hr) || hr == ND_TIMEOUT)
			{
				return "INDEndpoint::Disconnect failed with " + hresult_to_string(hr);
			}
		}
		catch (const std::exception& e) {
		}
	}

	if (pQp) pQp->Release();
	if (pConnector)pConnector->Release();
	if (pCq) pCq->Release();
	if (pMr)
	{
		hr = pMr->Deregister(pOv);
		if (hr == ND_PENDING)
		{
			SIZE_T BytesRet;
			int temp = 0;
			do
			{
				if (temp > m_msTimeOut) return "INDCompletionQueue::Notify failed with GetOverlappedResult TimeOut";
				hr = pMr->GetOverlappedResult(pOv, FALSE);
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
				temp += 10;
			} while (hr == ND_PENDING);
		}
		if (FAILED(hr) || hr == ND_TIMEOUT)
		{
			return "INDAdapter::DeregisterMemory failed with " + hresult_to_string(hr);
		}
	}
	if (pAdapter) pAdapter->Release();
	//delete[] Sgl;
	delete[] pResult;
	m_bConnected = false;
	return "OK";
};

void RDMAClient::WaitForEventNotification()
{
	HRESULT hr = pCq->Notify(ND_CQ_NOTIFY_ANY, pOv);
	if (hr == ND_PENDING)
	{
		hr = pCq->GetOverlappedResult(pOv, TRUE);
	}
}
void RDMAClient::WaitForCompletion(ND2_RESULT* Results, int expectedNum)
{
	int timeOutCnt = 0;
	int resultCnt = 0;
	while (true)
	{
		resultCnt += pCq->GetResults(&Results[resultCnt], expectedNum);
		if (resultCnt == expectedNum)
		{
			break;
		}
		else if (resultCnt == 0)
		{
			Results[0].Status = ND_IO_TIMEOUT;
			break;
		}
	};
}
void RDMAClient::WaitForMWReceive(void* expectedContext)
{
	WaitForRecieveQueue([&expectedContext](ND2_RESULT* pComp)
		{
			if (ND_SUCCESS != pComp->Status)
			{
				return;
			}
			if (expectedContext != pComp->RequestContext)
			{
				return;
			}
		});
}
void RDMAClient::WaitForRecieveQueue(
	const std::function<void(ND2_RESULT*)>& processCompletionFn)
{
	int timeOutCnt = 0;
	ND2_RESULT ndRes;
	while (true)
	{
		if (pRq->GetResults(&ndRes, 1) == 1)
		{
			processCompletionFn(&ndRes);
			break;
		}
		else
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			timeOutCnt++;
			if (timeOutCnt > 60000)
			{
				ndRes.Status = ND_IO_TIMEOUT;
				processCompletionFn(&ndRes);
				return;
			}
		}
	};
}