#include "Client.h"
#include "AdvancedHeapManager.h"
#include "NetUtils.h"
#include <thread>
#include <chrono>
#include <cstdlib>



void RunClient(int id, int numMessages)
{
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return;

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) { WSACleanup(); return; }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);
    serverAddr.sin_port = htons(DEFAULT_PORT);

    if (connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
    {
        closesocket(sock); WSACleanup(); return;
    }

    LOG("[CLIENT " << id << "] Connected");
    // javi da si povezan i sacekaj da svi klijenti budu spremni
    g_clientsConnected++;
    while (!g_startFlag)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

    for (int i = 0; i < numMessages; ++i)
    {
        int size = rand() % (MAX_MESSAGE_SIZE - MIN_MESSAGE_SIZE) + MIN_MESSAGE_SIZE;

        void* data = g_useAHM ? AHM::Malloc(size) : malloc(size);
        if (!data) break;

        if (!SendAll(sock, (char*)&size, sizeof(int)) ||
            !SendAll(sock, (char*)data, size))
        {
            g_useAHM ? AHM::Free(data) : free(data);
            break;
        }

        int replySize = 0;
        if (!RecvAll(sock, (char*)&replySize, sizeof(int)))
        {
            g_useAHM ? AHM::Free(data) : free(data);
            break;
        }

        void* reply = g_useAHM ? AHM::Malloc(replySize) : malloc(replySize);
        if (!reply)
        {
            g_useAHM ? AHM::Free(data) : free(data);
            break;
        }

        if (!RecvAll(sock, (char*)reply, replySize))
        {
            if (g_useAHM) { AHM::Free(data); AHM::Free(reply); }
            else { free(data); free(reply); }
            break;
        }

        if (g_useAHM) { AHM::Free(data); AHM::Free(reply); }
        else { free(data); free(reply); }
    }

    closesocket(sock);
    LOG("[CLIENT " << id << "] Completed");
    WSACleanup();
}
