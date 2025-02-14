#pragma once
#include "RDMACommon.h"
namespace RDMA
{
	class RDMAClient
	{
		IND2Adapter* pAdapter;
		HANDLE hAdapterFile;
		ND2_ADAPTER_INFO* pAdapterInfo;
		ND2_ADAPTER_INFO Adapter_Info = { 0 };
		IND2MemoryRegion* pMr;
		UINT32 MemToken;
		ND2_SGE* Sgl;
		ND2_SGE* SglBatchRead;
		IND2CompletionQueue* pCq;
		IND2CompletionQueue* pRq;
		IND2Connector* pConnector;
		IND2QueuePair* pQp;
		UINT64 remoteAddress = 0;
		UINT32 remoteToken = 0;
		ULONG availCredits = 0;
		SIZE_T CompleteQueueDepth = 0;
		SIZE_T ReceiveQueueDepth = 0;
		INDMemoryWindow* pMw;
		INDListen* pListen;
		OVERLAPPED* pOv;
		OVERLAPPED ov;
		HRESULT hr;
		struct sockaddr pAddress;
		struct sockaddr_in v4;
		struct sockaddr_in v4_Local;
		bool m_bConnected = false;
		bool bReadMode = false;
		void* Buffer = nullptr;
		SIZE_T BuffSize;
		SIZE_T nSge = 1;
		ND2_RESULT* pResult;
		void WaitForEventNotification();
		void WaitForCompletion(ND2_RESULT* Results, int expectedNum);
		void WaitForMWReceive(void* expectedContext);
		void WaitForRecieveQueue(
			const std::function<void(ND2_RESULT*)>& processCompletionFn);
		std::string Assign_IPAdress(LPWSTR ip, USHORT port);
		std::string OpenAdapter();
		std::string SendAdapterInfoQuery(bool bRead);
		std::string AdapterRegisterMemory(void* buffer, SIZE_T size);
		std::string CreateCQ();
		std::string CreateEndPoint();
		std::string SetupSGE();
		std::string Connect();
		std::string CompleteConnect();
		std::string Receive_MWDescriptor();
	public:
		RDMAClient();
		//Setup
		std::string Initialize(LPWSTR ip, USHORT port, void* buffer, SIZE_T bufferSize, bool bRead);
		//Data Transfer
		std::string Read(RDMADataFormat data);
		std::string Read(std::vector<RDMADataFormat> datas);
		//Destroy
		std::string DestroyConnection();
	};
}