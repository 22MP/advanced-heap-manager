#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>
#include <atomic>
#include <iostream>

// Linker biblioteka za Winsock
#pragma comment(lib, "Ws2_32.lib")

// ===== DEBUG LOG SWITCH =====
#ifdef DEBUG_LOG
#define LOG(x) do { std::cout << x << std::endl; } while(0)
#else
#define LOG(x) do { } while(0)
#endif

// === Globalne promenljive (definisane u Server.cpp) ===
extern std::atomic<bool> g_serverRunning;     // Da li server radi
extern std::atomic<long long> g_totalMessages; // Broj obradjenih poruka
extern SOCKET g_listenSocket;                 // Socket na kome server slusa
extern bool g_useAHM;                         // Da li koristimo AHM ili malloc/free

// === Konstante ===
constexpr int DEFAULT_PORT = 5555;
constexpr int MIN_MESSAGE_SIZE = 64;
constexpr int MAX_MESSAGE_SIZE = 4096;
