#include "pch.h"
#include "RDMAServer.h"
using namespace RDMA;

RDMAServer::RDMAServer() : pAdapter(nullptr), pMr(nullptr), pCq(nullptr), pQp(nullptr), pConnector(nullptr), hAdapterFile(nullptr), Buffer(nullptr), pMw(nullptr)
{
	RtlZeroMemory(&ov, sizeof(ov));
}

std::string RDMAServer::Initialize(LPWSTR ip, USHORT port, void* buffer, SIZE_T bufferSize, bool bRead)
{
	std::vector<std::function<std::string()>> steps = {
	[&] { return Assign_IPAdress(ip, port); },
	[&] { return OpenAdapter(); },
	[&] { return SendAdapterInfoQuery(bRead); },
	[&] { return AdapterRegisterMemory(buffer, bufferSize); },
	[&] { return CreateMemoryWindow(); },
	[&] { return CreateCQ(); },
	[&] { return SetupSGE(); },
	[&] { return CreateConnector(); },
	[&] { return CreateEndPoint(); },
	[&] { return Listen(); },
	[&] { return GetConnectionRequest(); },
	[&] { return Reserve_ReceiveEnd(); },
	[&] { return Accept(); },
	[&] { return BindMemoryWindow(); },
	[&] { return SendMW_Descript(); }
	};
	for (const auto& step : steps) {
		std::string result = step();
		if (!checkResultOK(result)) {
			return result;
		}
	}
	return "OK";
}

std::string RDMAServer::Assign_IPAdress(LPWSTR ip, USHORT port)
{
	Log("Assign_IPAdress Start");
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
		nullptr,
		reinterpret_cast<struct sockaddr*>(&v4),
		&len
	);
	if (v4.sin_port == 0)
	{
		v4.sin_port = htons(port);
	}


	hr = NdStartup();

	if (FAILED(hr))
	{
		return "NdStartup failed with " + hresult_to_string(hr);
	}
	Log("Assign_IPAdress Done");
	return "OK";
}

std::string RDMAServer::OpenAdapter()
{
	Log("OpenAdapter Start");
	hr = NdOpenAdapter(
		IID_IND2Adapter,
		reinterpret_cast<const struct sockaddr*>(&v4),
		sizeof(v4),
		reinterpret_cast<void**>(&pAdapter)
	);
	if (FAILED(hr))
	{
		return "NdOpenV1Adapter failed with " + hresult_to_string(hr);
	}
	ov.hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (ov.hEvent == nullptr)
	{
		return "Failed to allocate event for overlapped operations";
	}
	hr = pAdapter->CreateOverlappedFile(&hAdapterFile);
	if (FAILED(hr))
	{
		return "IND2Adapter::CreateOverlappedFile failed";
	}
	m_bConnected = true;
	Log("OpenAdapter Done");
	return "OK";
}

std::string RDMAServer::SendAdapterInfoQuery(bool bRead)
{
	Log("SendAdapterInfoQuery Start");
	bReadMode = bRead;
	memset(&Adapter_Info, 0, sizeof(Adapter_Info));
	Adapter_Info.InfoVersion = ND_VERSION_2;
	ULONG AdapterInfo_Len = sizeof(Adapter_Info);
	hr = pAdapter->Query(&Adapter_Info, &AdapterInfo_Len);
	if (FAILED(hr))
	{
		return "INDAdapter::Query Failed with " + hresult_to_string(hr);
	}
	Log("SendAdapterInfoQuery Done");
	return "OK";
}

std::string ConvertLPWSTRToString(LPWSTR wstr) {
	int bufferSize = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
	if (bufferSize == 0) {
		return "";
	}

	char* buffer = new char[bufferSize];
	WideCharToMultiByte(CP_UTF8, 0, wstr, -1, buffer, bufferSize, NULL, NULL);

	std::string result(buffer);
	delete[] buffer;
	return result;
}

// GetLastError()로부터 얻은 오류 코드를 사람이 읽을 수 있는 문자열로 변환하는 함수
std::string GetLastErrorAsString() {
	DWORD errorMessageID = ::GetLastError();
	if (errorMessageID == 0) {
		return "No error"; // 오류가 발생하지 않음
	}

	LPWSTR messageBuffer = nullptr;

	// 오류 메시지를 가져옴
	size_t size = FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&messageBuffer, 0, NULL);

	// 유니코드 문자열을 멀티바이트 문자열로 변환 (ANSI 코드 페이지 사용)
	int messageLength = WideCharToMultiByte(CP_ACP, 0, messageBuffer, -1, NULL, 0, NULL, NULL);
	std::string message(messageLength, 0);
	WideCharToMultiByte(CP_ACP, 0, messageBuffer, -1, &message[0], messageLength, NULL, NULL);

	// Win32의 메시지 버퍼 해제
	LocalFree(messageBuffer);

	return message;
}

std::string RDMAServer::AdapterRegisterMemory(void* buffer, SIZE_T size)
{
	Log("AdapterRegisterMemory Start");
	//CreateMR
	hr = pAdapter->CreateMemoryRegion(
		IID_IND2MemoryRegion,
		hAdapterFile,
		reinterpret_cast<VOID**>(&pMr)
	);
	if (FAILED(hr))
	{
		return "pAdapter::Create MemoryRegion Fail";
	}
	//HANDLE hMap = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, MMF_id);
	Buffer = (BYTE*)buffer;
	if (Buffer == NULL) return "AdapterRegisterMemory MMF Buffer Null";
	BuffSize = size;
	ULONG flags = ND_MR_FLAG_ALLOW_LOCAL_WRITE | ND_MR_FLAG_ALLOW_REMOTE_READ;
	while (true)
	{
		hr = pMr->Register(Buffer, BuffSize, flags, &ov);
		if (hr == ND_PENDING)
		{
			int temp = 0;
			do
			{
				if (temp > m_msTimeOut) return "INDCompletionQueue::Notify failed with GetOverlappedResult TimeOut";
				hr = pMr->GetOverlappedResult(&ov, FALSE);
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
				temp += 10;
			} while (hr == ND_PENDING);
		}
		if (hr == ND_IO_TIMEOUT)
		{
			continue;
		}
		if (FAILED(hr))
		{
			return "RegisterMemory Failed with " + hresult_to_string(hr);
		}
		break;
	}
	Log("AdapterRegisterMemory Done");
	return "OK";
}

std::string RDMAServer::CreateMemoryWindow()
{
	Log("CreateMemoryWindow Start");
	hr = pAdapter->CreateMemoryWindow(
		IID_IND2MemoryWindow,
		reinterpret_cast<VOID**>(&pMw));
	if (FAILED(hr))
	{
		return "INDAdapter::CreateMemoryWindow Failed with " + hresult_to_string(hr);
	}
	Log("CreateMemoryWindow Done");
	return "OK";
}

std::string RDMAServer::Listen()
{
	Log("Listen Start");
	hr = pAdapter->CreateListener(
		IID_IND2Listener,
		hAdapterFile,
		reinterpret_cast<VOID**>(&pListen)
	);
	if (FAILED(hr))
	{
		return "INDAdapter::Failed to Make Listener with " + hresult_to_string(hr);
	}

	hr = pListen->Bind(
		reinterpret_cast<const sockaddr*>(&v4),
		sizeof(v4)
	);
	if (FAILED(hr))
	{
		return "INDAdapter::Listener Binding Failed with " + hresult_to_string(hr);
	}
	hr = pListen->Listen(0);
	Log("Listen Done");
	return "OK";
}


std::string RDMAServer::SetupSGE()
{
	Log("SetupSGE Start");
	Sgl = new (std::nothrow) ND2_SGE[nSge];
	Sgl[0].Buffer = Buffer;
	Sgl[0].BufferLength = (ULONG)x_MaxXfer;
	Sgl[0].MemoryRegionToken = pMr->GetLocalToken();
	Log("SetupSGE Done");
	return "OK";
}

std::string RDMAServer::CreateCQ()
{
	Log("CreateCQ Start");
	QueueDepth = min(Adapter_Info.MaxCompletionQueueDepth, Adapter_Info.MaxReceiveQueueDepth) - 1;
	hr = pAdapter->CreateCompletionQueue(
		IID_IND2CompletionQueue,
		hAdapterFile,
		QueueDepth,
		0,
		0,
		reinterpret_cast<VOID**>(&pCq));
	if (FAILED(hr))
	{
		return "INDAdapter::CreateCompletionQueue failed with " + hresult_to_string(hr);
	}
	Log("CreateCQ Done");
	return "OK";
}

std::string RDMAServer::CreateConnector()
{
	Log("CreateConnector Start");
	hr = pAdapter->CreateConnector(
		IID_IND2Connector,
		hAdapterFile,
		reinterpret_cast<VOID**>(&pConnector));
	if (FAILED(hr))
	{
		return "INDAdapter::CreateConnector Failed with " + hresult_to_string(hr);
	}
	Results_Server = new (std::nothrow) ND2_RESULT[QueueDepth + 1];
	Log("CreateConnector Done");
	return "OK";
}


std::string RDMAServer::GetConnectionRequest()
{
	Log("GetConnectionRequest Start");
	hr = pListen->GetConnectionRequest(pConnector, &ov);
	if (hr == ND_PENDING)
	{
		int temp = 0;
		do
		{
			if (temp > 50000) return "pListen::GetConnectionRequest failed with GetOverlappedResult TimeOut";
			hr = pListen->GetOverlappedResult(&ov, FALSE);
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			temp += 10;
		} while (hr == ND_PENDING);
	}
	if (FAILED(hr) || hr == ND_TIMEOUT)
	{
		return "INDAdapter::GetConnectionRequest Failed with " + hresult_to_string(hr);
	}
	Log("GetConnectionRequest Done");
	return "OK";
}

//EndPoint -> QueuePair
//생성은 예제 참조
std::string RDMAServer::CreateEndPoint()
{
	Log("CreateEndPoint Start");
	hr = pAdapter->CreateQueuePair
	(
		IID_IND2QueuePair,
		pCq,
		pCq,
		nullptr,
		2,
		2,
		nSge,
		nSge,
		0,
		reinterpret_cast<VOID**>(&pQp)
	);
	if (FAILED(hr))
	{
		return "INDConnector::CreateEndpoint failed with " + hresult_to_string(hr);
	}
	Log("CreateEndPoint Done");
	return "OK";
}

std::string RDMAServer::Reserve_ReceiveEnd()
{
	Log("Reserve_ReceiveEnd Start");
	hr = pQp->Receive(RECV_CTXT, Sgl, 1);
	if (FAILED(hr))
	{
		return "INDEndpoint::Receive failed with " + hresult_to_string(hr);
	}
	Log("Reserve_ReceiveEnd Done");
	return "OK";
}

std::string RDMAServer::Accept()
{
	Log("Accept Start");
	pConnector->Accept(
		pQp,
		Adapter_Info.MaxInboundReadLimit,
		0,
		nullptr,
		0,
		&ov
	);
	if (hr == ND_PENDING)
	{
		int temp = 0;
		do
		{
			if (temp > m_msTimeOut) return "INDCompletionQueue::Notify failed with GetOverlappedResult TimeOut";
			hr = pListen->GetOverlappedResult(&ov, FALSE);
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			temp += 10;
		} while (hr == ND_PENDING);
	}
	if (FAILED(hr) || hr == ND_TIMEOUT)
	{
		return "INDListen::Accept failed with " + hresult_to_string(hr);
	}
	Log("Accept Done");
	return "OK";
}

std::string RDMAServer::BindMemoryWindow()
{
	Log("BindMemoryWindow Start");
	hr = pQp->Bind(
		&Results_Server[BIND],
		pMr,
		pMw,
		Buffer,
		BuffSize,
		bReadMode ? ND_OP_FLAG_ALLOW_READ : ND_OP_FLAG_ALLOW_WRITE
	);
	if (FAILED(hr))
	{
		return "INDEndpoint::Bind failed with " + hresult_to_string(hr);
	}
	ND2_RESULT ndRes;
	WaitForCompletion(&ndRes);

	if (FAILED(hr))
	{
		return "INDEndpoint::Bind failed with Waiting";
	}
	Log("BindMemoryWindow Done");
	return "OK";
}



std::string RDMAServer::SendMW_Descript()
{
	Log("SendMW_Descript Start");
	void* buf2 = (BYTE*)Buffer + BuffSize - x_MaxXfer;
	Sgl[0].Buffer = buf2;
	Sgl[0].BufferLength = x_MaxXfer;
	Sgl[0].MemoryRegionToken = pMr->GetLocalToken();

	PeerInfo* pInfo = static_cast<PeerInfo*> (buf2);
	pInfo->m_remoteToken = pMw->GetRemoteToken();
	pInfo->m_nIncomingReadLimit = Adapter_Info.MaxInboundReadLimit;
	pInfo->m_remoteAddress = reinterpret_cast<UINT64>(Buffer);

	hr = pQp->Send(SEND_CTXT, Sgl, 1, 0);


	if (FAILED(hr))
	{
		return "INDEndpoint::SendMW_Descript failed with " + hresult_to_string(hr);
	}

	// wait for send completion
	WaitForCompletionAndCheckContext(SEND_CTXT);

	Log("SendMW_Descript Done");
	return "OK";
}

std::string RDMAServer::DestroyConnection()
{
	Log("DestroyConnection Start");
	if (!m_bConnected) return "OK";
	if (pMw) pMw->Release();
	if (pConnector)
	{
		hr = pConnector->Disconnect(&ov);
		if (hr == ND_PENDING)
		{
			int temp = 0;
			do
			{
				if (temp > m_msTimeOut) return "INDConnector::Notify failed with GetOverlappedResult TimeOut";
				hr = pConnector->GetOverlappedResult(&ov, TRUE);
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
	if (pQp) pQp->Release();
	if (pConnector) pConnector->Release();
	if (pCq) pCq->Release();

	if (pListen) pListen->Release();

	if (pMr) hr = pMr->Deregister(&ov);

	if (pAdapter) pAdapter->Release();

	delete[] Results_Server;

	hr = NdCleanup();
	if (FAILED(hr))
	{
		return "NdCleanup failed with " + hresult_to_string(hr);
	}

	CloseHandle(ov.hEvent);

	_fcloseall();
	WSACleanup();
	m_bConnected = false;
	Log("DestroyConnection Done");
	return "OK";
}

void RDMAServer::WaitForEventNotification()
{
	HRESULT hr = pCq->Notify(ND_CQ_NOTIFY_ANY, &ov);
	if (hr == ND_PENDING)
	{
		hr = pCq->GetOverlappedResult(&ov, TRUE);
	}
}


void RDMAServer::WaitForCompletion(
	const std::function<void(ND2_RESULT*)>& processCompletionFn)
{
	int timeOutCnt = 0;
	ND2_RESULT ndRes;
	while (true)
	{
		if (pCq->GetResults(&ndRes, 1) == 1)
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

void RDMAServer::WaitForCompletionAndCheckContext(void* expectedContext)
{
	WaitForCompletion([&expectedContext](ND2_RESULT* pComp)
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

void RDMAServer::WaitForCompletion(ND2_RESULT* pResult)
{
	WaitForCompletion([&pResult](ND2_RESULT* pCompRes)
		{
			*pResult = *pCompRes;
		});
}

void RDMAServer::WaitForCompletion(HRESULT expectedResult, const char* errorMessage)
{
	ND2_RESULT ndRes;
	WaitForCompletion(&ndRes);
}