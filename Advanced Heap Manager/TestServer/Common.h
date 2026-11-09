#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>
#include <atomic>
#include <iostream>

// Winsock library linker directive
#pragma comment(lib, "Ws2_32.lib")

// ===== DEBUG LOG SWITCH =====
#ifdef DEBUG_LOG
#define LOG(x) do { std::cout << x << std::endl; } while(0)
#else
#define LOG(x) do { } while(0)
#endif

// === Global variables (defined in Server.cpp) ===
extern std::atomic<bool> g_serverRunning;      // Indicates whether the server is running
extern std::atomic<long long> g_totalMessages; // Number of processed messages
extern std::atomic<int>  g_clientsConnected;   // Number of connected clients
extern std::atomic<bool> g_startFlag;          // true = clients are allowed to start sending
extern SOCKET g_listenSocket;                  // Socket on which the server listens
extern bool g_useAHM;                          // Indicates whether AHM or malloc/free is used

// === Constants ===
constexpr int DEFAULT_PORT = 5555;
constexpr int MIN_MESSAGE_SIZE = 64;
constexpr int MAX_MESSAGE_SIZE = 4096;