#pragma comment(lib, "ws2_32.lib")
#include <iostream>
#include <regex>
#include <tchar.h>
#include <cstdlib>
#include <RDMAServer.h>
#include <RDMAClient.h>
bool IsValidIPAddress(const TCHAR* ip) {
    std::wregex ipRegex(LR"(^(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3})$)");
    std::wcmatch match;

    if (std::regex_match(ip, match, ipRegex)) {
        for (int i = 1; i <= 4; i++) {
            int octet = std::stoi(match[i].str());
            if (octet < 0 || octet > 255)
                return false;
        }
        return true;
    }
    return false;
}

bool IsValidPort(const TCHAR* port) {
    std::wregex portRegex(LR"(^\d{1,5}$)");
    if (std::regex_match(port, portRegex)) {
        int portNum = _ttoi(port);
        return portNum > 0 && portNum <= 65535;
    }
    return false;
}

SIZE_T GetMaxAllowedMemory() {
    MEMORYSTATUSEX memStatus;
    memStatus.dwLength = sizeof(memStatus);
    if (GlobalMemoryStatusEx(&memStatus)) {
        return static_cast<SIZE_T>(memStatus.ullTotalPhys * 0.9); // 90% 제한
    }
    return 0; // 실패 시 0 반환
}

void ShowUsage()
{
    printf("PlugRDMA-Windows [options] <MB Size> <IP> <Port>\n"
        "Options:\n"
        "\t-s            - Start as server (listen on IP/Port)\n"
        "\t-c            - Start as client (connect to server IP/Port)\n"
        "\t-w            - Use RMA Write (client only, default)\n"
        "\t-r            - Use RMA Read (client only)\n"
        "\t-h            - Help Msg\n"
        "<memory MB>     - RDMA HeapAllocation Size(MegaByte)(Essential)\n"
        "<ip>            - IPv4 Address(Essential)\n"
        "<port>          - Port number(Essential)\n"
        "Example : -s -r 1024 10.0.0.1 8888\n"
    );
}

int _tmain(int argc, TCHAR* argv[]) {

    ShowUsage();

    if (argc < 6) {
        printf("You Must Enter argumnet : <IsServer> <IsRead> <MB> <IP> <Port>\n");
        return EXIT_FAILURE;
    }

    const TCHAR* memSizeArg = argv[argc - 3];
    const TCHAR* ipArg = argv[argc - 2];
    const TCHAR* portArg = argv[argc - 1];

    if (!IsValidIPAddress(ipArg) || !IsValidPort(portArg)) {
        printf("Error: Invalid IP or Port format.\n");
        exit(0);
    }

    size_t ipLen = wcslen(ipArg) + 1;
    LPWSTR ipAddress = new WCHAR[ipLen];
    wcscpy_s(ipAddress, ipLen, ipArg);

    // USHORT 변환
    USHORT portNumber = static_cast<USHORT>(_ttoi(portArg));

    SIZE_T requestedMemory = static_cast<SIZE_T>(_ttoi(memSizeArg)) * 1024 * 1024;

    // 최대 허용 가능한 메모리 계산 (90% 제한)
    SIZE_T maxAllowedMemory = GetMaxAllowedMemory();
    if (maxAllowedMemory == 0) {
        printf("Error: Failed to get system memory status.\n");
        return EXIT_FAILURE;
    }

    // 요청된 메모리가 90% 초과하면 종료
    if (requestedMemory > maxAllowedMemory) {
        printf("Error: Requested memory exceeds 90%% of total system memory (%lld MB max).\n",
            maxAllowedMemory / (1024 * 1024));
        return EXIT_FAILURE;
    }

    // 메모리 할당
    void* buffer = static_cast<char*>(HeapAlloc(GetProcessHeap(), 0, requestedMemory));
    if (!buffer) {
        printf("Error: Memory allocation failed.\n");
        return EXIT_FAILURE;
    }
    printf("Create HeapMemory Done");
    bool bServer = false;
    bool bRead = false;

    for (int i = 1; i < argc; i++)
    {
        TCHAR* arg = argv[i];
        if ((wcscmp(arg, L"-s") == 0) || (wcscmp(arg, L"-S") == 0))
        {
            bServer = true;
        }
        else if ((wcscmp(arg, L"-c") == 0) || (wcscmp(arg, L"-C") == 0))
        {
            bServer = false;
        }
        else if ((wcscmp(arg, L"-r") == 0) || (wcscmp(arg, L"--read") == 0))
        {
            bRead = true;
        }
        else if ((wcscmp(arg, L"-w") == 0) || (wcscmp(arg, L"--write") == 0))
        {
            bRead = false;
        }
        else if ((wcscmp(arg, L"-h") == 0) || (wcscmp(arg, L"--help") == 0))
        {
            ShowUsage();
            exit(0);
        }
    }
    if (bServer)
    {
        RDMA::RDMAServer rdmaServer = RDMA::RDMAServer();
        std::string res;
        res = rdmaServer.Initialize(ipAddress, portNumber, buffer, requestedMemory, bRead);
        if (res != "OK") std::cout << res << std::endl;
    }
    else
    {
        RDMA::RDMAClient rdmaClient = RDMA::RDMAClient();
        std::string res;
        res = rdmaClient.Initialize(ipAddress, portNumber, buffer, requestedMemory, bRead);
        if (res != "OK") std::cout << res << std::endl;
    }
    std::cout << "RDMA Ready!!" << std::endl;
    return 0;
}

// 프로그램 실행: <Ctrl+F5> 또는 [디버그] > [디버깅하지 않고 시작] 메뉴
// 프로그램 디버그: <F5> 키 또는 [디버그] > [디버깅 시작] 메뉴

// 시작을 위한 팁: 
//   1. [솔루션 탐색기] 창을 사용하여 파일을 추가/관리합니다.
//   2. [팀 탐색기] 창을 사용하여 소스 제어에 연결합니다.
//   3. [출력] 창을 사용하여 빌드 출력 및 기타 메시지를 확인합니다.
//   4. [오류 목록] 창을 사용하여 오류를 봅니다.
//   5. [프로젝트] > [새 항목 추가]로 이동하여 새 코드 파일을 만들거나, [프로젝트] > [기존 항목 추가]로 이동하여 기존 코드 파일을 프로젝트에 추가합니다.
//   6. 나중에 이 프로젝트를 다시 열려면 [파일] > [열기] > [프로젝트]로 이동하고 .sln 파일을 선택합니다.
