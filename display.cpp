#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>

#pragma comment(lib, "Ws2_32.lib")

#define SERVER_PORT 8080
#define SERVER_ADDR "127.0.0.1"  // Assuming the server is running on localhost

int main() {
    WSADATA wsaData;
    SOCKET ConnectSocket = INVALID_SOCKET;
    struct addrinfo *result = NULL, *ptr = NULL, hints;

    // Initialize Winsock
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        std::cout << "WSAStartup failed: " << iResult << std::endl;
        return 1;
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    // Resolve the server address and port
    iResult = getaddrinfo(SERVER_ADDR, std::to_string(SERVER_PORT).c_str(), &hints, &result);
    if (iResult != 0) {
        std::cout << "getaddrinfo failed: " << iResult << std::endl;
        WSACleanup();
        return 1;
    }

    // Create a SOCKET for connecting to server
    ptr = result;
    ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
    if (ConnectSocket == INVALID_SOCKET) {
        std::cout << "socket failed: " << WSAGetLastError() << std::endl;
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }

    // Connect to server
    iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        closesocket(ConnectSocket);
        std::cout << "Unable to connect to server!" << std::endl;
        WSACleanup();
        return 1;
    }
    freeaddrinfo(result);

    // Receive data from server
    char recvbuf[512];
    int recvbuflen = 512;
    do {
        iResult = recv(ConnectSocket, recvbuf, recvbuflen, 0);
        if (iResult > 0) {
            std::cout << "Received data: " << std::string(recvbuf, iResult) << std::endl;
        }
        else if (iResult == 0) {
            std::cout << "Connection closed\n";
        }
        else {
            std::cout << "recv failed: " << WSAGetLastError() << std::endl;
        }
    } while (iResult > 0);

    // Cleanup
    closesocket(ConnectSocket);
    WSACleanup();
    return 0;
}