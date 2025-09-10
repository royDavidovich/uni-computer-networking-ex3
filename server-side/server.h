#ifndef WEBSERVER_SERVER_H
#define WEBSERVER_SERVER_H

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#include <winsock2.h>
#include <string>

// -----------------------
// Socket & Server Globals
// -----------------------
struct SocketState
{
    SOCKET id;         // Socket handle
    int recv;          // Receiving state
    int send;          // Sending state
    int sendSubType;   // Unused for HTTP
    char buffer[65536]; // I/O buffer
    int len;           // Current bytes in buffer
    int bytesToSend;   // Response bytes to send
};

// -----------------------
// Constants (header-only)
// -----------------------
constexpr int TIME_PORT      = 27015;
constexpr int MAX_SOCKETS    = 60;
constexpr int EMPTY          = 0;
constexpr int LISTEN_STATE   = 1;
constexpr int RECEIVE_STATE  = 2;
constexpr int IDLE           = 3;
constexpr int SEND_STATE     = 4;

// Global sockets array and count (defined in server.cpp)
extern SocketState sockets[];
extern int socketsCount;

// -----------------------
// Server API
// -----------------------
bool addSocket(SOCKET id, int what);
void removeSocket(int index);
void acceptConnection(int index);
void receiveMessage(int index);
void sendMessage(int index);

#endif // WEBSERVER_SERVER_H