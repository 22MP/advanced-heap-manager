#include "Server.h"
#include "AdvancedHeapManager.h"
#include "NetUtils.h"
#include <thread>

std::atomic<bool> g_serverRunning = true;
std::atomic<long long> g_totalMessages = 0;
SOCKET g_listenSocket = INVALID_SOCKET;
bool g_useAHM = true;


void ClientHandler(SOCKET clientSocket)
{
    while (true)
    {
        int size = 0;
        if (!RecvAll(clientSocket, (char*)&size, sizeof(int))) break;

        void* buffer = g_useAHM ? AHM::Malloc(size) : malloc(size);
        if (!buffer) break;

        if (!RecvAll(clientSocket, (char*)buffer, size))
        {
            g_useAHM ? AHM::Free(buffer) : free(buffer);
            break;
        }

        int replySize = rand() % (MAX_MESSAGE_SIZE - MIN_MESSAGE_SIZE) + MIN_MESSAGE_SIZE;
        void* reply = g_useAHM ? AHM::Malloc(replySize) : malloc(replySize);
        if (!reply)
        {
            g_useAHM ? AHM::Free(buffer) : free(buffer);
            break;
        }

        SendAll(clientSocket, (char*)&replySize, sizeof(int));
        SendAll(clientSocket, (char*)reply, replySize);

        if (g_useAHM) { AHM::Free(buffer); AHM::Free(reply); }
        else { free(buffer); free(reply); }

        g_totalMessages++;
    }

    closesocket(clientSocket);
}

void RunServer(bool useAHM)
{
    g_useAHM = useAHM;

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return;

    g_listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (g_listenSocket == INVALID_SOCKET) { WSACleanup(); return; }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(DEFAULT_PORT);

    bind(g_listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));
    listen(g_listenSocket, SOMAXCONN);

    LOG("[SERVER] Listening...");

    while (g_serverRunning)
    {
        SOCKET clientSocket = accept(g_listenSocket, nullptr, nullptr);
        if (clientSocket == INVALID_SOCKET) break;

        std::thread(ClientHandler, clientSocket).detach();
    }

    closesocket(g_listenSocket);
    WSACleanup();
    LOG("[SERVER] Stopped");
}
