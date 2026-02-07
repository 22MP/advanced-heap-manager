#pragma once
#include <winsock2.h>

inline bool RecvAll(SOCKET sock, char* buffer, int totalBytes)
{
    int received = 0;
    while (received < totalBytes)
    {
        int r = recv(sock, buffer + received, totalBytes - received, 0);
        if (r <= 0) return false;
        received += r;
    }
    return true;
}

inline bool SendAll(SOCKET sock, const char* buffer, int totalBytes)
{
    int sent = 0;
    while (sent < totalBytes)
    {
        int s = send(sock, buffer + sent, totalBytes - sent, 0);
        if (s == SOCKET_ERROR) return false;
        sent += s;
    }
    return true;
}

