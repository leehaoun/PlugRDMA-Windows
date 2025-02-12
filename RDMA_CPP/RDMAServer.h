#pragma once
#include "ndcommon.h"
#include "RDMACommon.h"
#include <string>
#include <functional>
#include <thread>
namespace RDMA
{
	class RDMAServer
	{
		IND2Adapter* pAdapter;
		IND2Listener* pListen;
		IND2Connector* pConnector;
		IND2MemoryRegion* pMr;
		IND2CompletionQueue* pCq;
		HANDLE hAdapterFile;
		IND2QueuePair* pQp;
		OVERLAPPED ov;
		HRESULT hr;
		struct sockaddr pAddress;
		struct sockaddr_in v4 = { 0 };
		ND2_ADAPTER_INFO Adapter_Info = { 0 };
		bool m_bConnected = false;
		bool bReadMode = false;
		void* Buffer = nullptr;
		//ND_MR_HANDLE hMr;
		IND2MemoryWindow* pMw;
		ND2_SGE* Sgl;
		ND2_RESULT* Results_Server;
		SIZE_T BuffSize;
		SIZE_T nSge = 1;
		DWORD QueueDepth;
		void WaitForEventNotification();
		void WaitForCompletion(ND2_RESULT* pResult);
		void WaitForCompletion(
			const std::function<void(ND2_RESULT*)>& processCompletionFn);
		void WaitForCompletion(
			HRESULT expectedResult = ND_SUCCESS,
			const char* errorMessage = "IND2QueuePair:GetResults failed");
		void WaitForCompletionAndCheckContext(void* expectedContext);
	public:
		RDMAServer();
		//Setup
		std::string Assign_IPAdress(LPWSTR ip, USHORT port);
		std::string OpenAdapter();
		std::string SendAdapterInfoQuery(bool bRead);
		std::string AdapterRegisterMemory(LPWSTR MMF_id, SIZE_T size);
		std::string CreateMemoryWindow();
		std::string CreateCQ();
		std::string SetupSGE();
		std::string CreateConnector();
		std::string CreateEndPoint();
		std::string Listen();
		std::string GetConnectionRequest();
		std::string Reserve_ReceiveEnd();
		std::string Accept();
		std::string BindMemoryWindow();
		std::string SendMW_Descript();
		//Destroy
		std::string DestroyConnection();
		std::string DestroyConnection_Adapter();
	};
}