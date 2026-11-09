#include "Server.h"
#include "AdvancedHeapManager.h"
#include "NetUtils.h"
#include <thread>

// Global server state
std::atomic<bool> g_serverRunning = true;
std::atomic<long long> g_totalMessages = 0;
std::atomic<int>  g_clientsConnected{ 0 };
std::atomic<bool> g_startFlag{ false };
SOCKET g_listenSocket = INVALID_SOCKET;
bool g_useAHM = true;


// Handles communication with a single connected client
void ClientHandler(SOCKET clientSocket)
{
    while (true)
    {
        int size = 0;
        if (!RecvAll(clientSocket, (char*)&size, sizeof(int))) break;

        // Allocate memory for the incoming message
        void* buffer = g_useAHM ? AHM::Malloc(size) : malloc(size);
        if (!buffer) break;

        if (!RecvAll(clientSocket, (char*)buffer, size))
        {
            g_useAHM ? AHM::Free(buffer) : free(buffer);
            break;
        }

        // Generate a random response size and allocate memory for the response
        int replySize = rand() % (MAX_MESSAGE_SIZE - MIN_MESSAGE_SIZE) + MIN_MESSAGE_SIZE;
        void* reply = g_useAHM ? AHM::Malloc(replySize) : malloc(replySize);
        if (!reply)
        {
            g_useAHM ? AHM::Free(buffer) : free(buffer);
            break;
        }

        // Send the response size and response data back to the client
        SendAll(clientSocket, (char*)&replySize, sizeof(int));
        SendAll(clientSocket, (char*)reply, replySize);

        // Free allocated memory using the selected allocation strategy
        if (g_useAHM) { AHM::Free(buffer); AHM::Free(reply); }
        else { free(buffer); free(reply); }

        // Increment the total number of successfully processed messages
        g_totalMessages++;
    }

    // Close the client socket when communication ends
    closesocket(clientSocket);
}

// Initializes the server, listens for incoming connections, and starts client handlers
void RunServer(bool useAHM)
{
    g_useAHM = useAHM;

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return;

    // Create the listening socket
    g_listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (g_listenSocket == INVALID_SOCKET) { WSACleanup(); return; }

    // Configure the server address and port
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(DEFAULT_PORT);

    // Bind the socket and start listening for client connections
    bind(g_listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));
    listen(g_listenSocket, SOMAXCONN);

    LOG("[SERVER] Listening...");

    // Accept incoming connections while the server is running
    while (g_serverRunning)
    {
        SOCKET clientSocket = accept(g_listenSocket, nullptr, nullptr);
        if (clientSocket == INVALID_SOCKET) break;

        // Handle each client in a separate detached thread
        std::thread(ClientHandler, clientSocket).detach();
    }

    // Release server resources
    closesocket(g_listenSocket);
    WSACleanup();
    LOG("[SERVER] Stopped");
}