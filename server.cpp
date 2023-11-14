#define NOMINMAX
#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <thread>
#include <iostream>
#include <limits>
#include <chrono>
#include <streambuf>
#include <string>
#include <sstream>
#include <mutex>
#include <vector>
using namespace std;
// Display is not working right after 1699927628-1699931773 time with 3 clients
// try to break server with overflow
// fix numeric limts

class PriceAggregator : public streambuf {
private:
    int high, low, close;
    chrono::time_point<chrono::system_clock> start_time;
    const int interval = 60; // 1 minute in seconds
    mutex mtx;

    void reset() {
        high = numeric_limits<int>::min();
        low = numeric_limits<int>::max();
        close = 0;
    }

protected:
    // Process incoming trade data
    int underflow() override {
        string line;
        if (getline(cin, line)) {
            istringstream iss(line);
            int price;
            while (iss >> price) {
                lock_guard<mutex> lock(mtx);
                processTrade(price);
            }
        }
        return traits_type::eof();
    }

    // Output aggregated data
    int overflow(int c = traits_type::eof()) override {
        lock_guard<mutex> lock(mtx);
        emitBar(cout);
        return c;
    }

public:
    PriceAggregator() {
        reset();
        start_time = chrono::system_clock::now();
    }

    void processTrade(int price) {
        high = max(high, price);
        low = min(low, price);
        close = price;
    }

    void emitBar(ostream& bars_out) {
        auto now = chrono::system_clock::now();
        chrono::duration<double> elapsed_seconds = now - start_time;

        if (elapsed_seconds.count() >= interval) {
            bars_out << "{ \"start_time\": \"" << chrono::system_clock::to_time_t(start_time)
                     << "\", \"high\": " << high << ", \"low\": " << low << ", \"close\": " << close << " }\n";
            
            reset();
            start_time = now;
        }
    }
};
#pragma comment(lib, "Ws2_32.lib")

#define PORT 8080

mutex aggregatorMutex;

void handleClient(SOCKET clientSocket, PriceAggregator& aggregator) {
    char recvbuf[512];
    int recvbuflen = 512;
    int iResult;

    // Process incoming data from client
    while (true) {
        iResult = recv(clientSocket, recvbuf, recvbuflen, 0);
        if (iResult > 0) {
            lock_guard<mutex> lock(aggregatorMutex);
            // Process each received price
            istringstream iss(string(recvbuf, iResult));
            int price;
            while (iss >> price) {
                aggregator.processTrade(price);
            }
        } else if (iResult == 0) {
            cout << "Connection closing...\n";
            break;
        } else {
            cerr << "recv failed: " << WSAGetLastError() << '\n';
            break;
        }
    }

    closesocket(clientSocket);
}

void sendAggregatedData(SOCKET clientSocket, PriceAggregator& aggregator) {
    while (true) {
        this_thread::sleep_for(chrono::seconds(60)); // 60 seconds

        lock_guard<mutex> lock(aggregatorMutex);
        ostringstream oss;
        aggregator.emitBar(oss);
        string dataToSend = oss.str();

        if (!dataToSend.empty()) {
            int iSendResult = send(clientSocket, dataToSend.c_str(), dataToSend.length(), 0);
            if (iSendResult == SOCKET_ERROR) {
                cerr << "send failed: " << WSAGetLastError() << '\n';
                closesocket(clientSocket);
                break;
            }
        }
    }
}

int main() {
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        cerr << "WSAStartup failed: " << iResult << '\n';
        return 1;
    }

    struct addrinfo *result = NULL, hints;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    iResult = getaddrinfo(NULL, to_string(PORT).c_str(), &hints, &result);
    if (iResult != 0) {
        cerr << "getaddrinfo failed: " << iResult << '\n';
        WSACleanup();
        return 1;
    }

    SOCKET ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (ListenSocket == INVALID_SOCKET) {
        cerr << "socket failed: " << WSAGetLastError() << '\n';
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }

    iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        cerr << "bind failed: " << WSAGetLastError() << '\n';
        freeaddrinfo(result);
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    freeaddrinfo(result);

    iResult = listen(ListenSocket, SOMAXCONN);
    if (iResult == SOCKET_ERROR) {
        cerr << "listen failed: " << WSAGetLastError() << '\n';
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    PriceAggregator aggregator;
    vector<thread> clientThreads;

    while (true) {
        SOCKET ClientSocket = accept(ListenSocket, NULL, NULL);
        if (ClientSocket == INVALID_SOCKET) {
            cerr << "accept failed: " << WSAGetLastError() << '\n';
            continue;
        }

        // Handle client in a separate thread
        clientThreads.push_back(thread(handleClient, ClientSocket, ref(aggregator)));
       // Another thread for sending aggregated data
    clientThreads.push_back(thread(sendAggregatedData, ClientSocket, ref(aggregator)));
}

closesocket(ListenSocket);

// Join all threads
for (auto& thread : clientThreads) {
    if (thread.joinable()) {
        thread.join();
    }
}

// Clean up Winsock
WSACleanup();

return 0;
}