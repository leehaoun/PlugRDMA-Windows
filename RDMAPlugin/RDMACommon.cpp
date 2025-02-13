#include "pch.h"
#include "RDMACommon.h"
std::string RDMA::hresult_to_string(HRESULT hr)
{
	_com_error err(hr);
	LPCTSTR errMsg = err.ErrorMessage();
	int len = WideCharToMultiByte(CP_UTF8, 0, errMsg, -1, nullptr, 0, nullptr, nullptr);
	std::string message(len, 0);
	WideCharToMultiByte(CP_UTF8, 0, errMsg, -1, &message[0], len, nullptr, nullptr);
	return message;
}
bool RDMA::checkResultOK(const std::string& res)
{
	return res == "OK";
}
void RDMA::Log(std::string msg)
{
	std::cout << msg << std::endl;
}