#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <winsock2.h>
#pragma comment(lib, "Ws2_32.lib")

#include "server.h"

int main()
{
    // Initialize Winsock
    WSAData wsaData;
    if (NO_ERROR != WSAStartup(MAKEWORD(2, 2), &wsaData))
    {
        std::cout << "Time Server: Error at WSAStartup()\n";
        return 0;
    }

    // Create listening socket
    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (INVALID_SOCKET == listenSocket)
    {
        std::cout << "Time Server: Error at socket(): " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 0;
    }

    // Bind
    sockaddr_in serverService;
    serverService.sin_family = AF_INET;
    serverService.sin_addr.s_addr = INADDR_ANY;
    serverService.sin_port = htons(TIME_PORT);

    if (SOCKET_ERROR == bind(listenSocket, (SOCKADDR*)&serverService, sizeof(serverService)))
    {
        std::cout << "Time Server: Error at bind(): " << WSAGetLastError() << std::endl;
        closesocket(listenSocket);
        WSACleanup();
        return 0;
    }

    // Listen
    if (SOCKET_ERROR == listen(listenSocket, 5))
    {
        std::cout << "Time Server: Error at listen(): " << WSAGetLastError() << std::endl;
        closesocket(listenSocket);
        WSACleanup();
        return 0;
    }
    addSocket(listenSocket, LISTEN_STATE);

    // Main loop (select)
    while (true)
    {
        fd_set waitRecv;
        FD_ZERO(&waitRecv);
        for (int i = 0; i < MAX_SOCKETS; i++)
        {
            if ((sockets[i].recv == LISTEN_STATE) || (sockets[i].recv == RECEIVE_STATE))
                FD_SET(sockets[i].id, &waitRecv);
        }

        fd_set waitSend;
        FD_ZERO(&waitSend);
        for (int i = 0; i < MAX_SOCKETS; i++)
        {
            if (sockets[i].send == SEND_STATE)
                FD_SET(sockets[i].id, &waitSend);
        }

        int nfd = select(0, &waitRecv, &waitSend, NULL, NULL);
        if (nfd == SOCKET_ERROR)
        {
            std::cout << "Time Server: Error at select(): " << WSAGetLastError() << std::endl;
            WSACleanup();
            return 0;
        }

        for (int i = 0; i < MAX_SOCKETS && nfd > 0; i++)
        {
            if (FD_ISSET(sockets[i].id, &waitRecv))
            {
                nfd--;
                switch (sockets[i].recv)
                {
                case LISTEN_STATE:
                    acceptConnection(i);
                    break;
                case RECEIVE_STATE:
                    receiveMessage(i);
                    break;
                }
            }
        }

        for (int i = 0; i < MAX_SOCKETS && nfd > 0; i++)
        {
            if (FD_ISSET(sockets[i].id, &waitSend))
            {
                nfd--;
                switch (sockets[i].send)
                {
                case SEND_STATE:
                    sendMessage(i);
                    break;
                }
            }
        }
    }

    std::cout << "Time Server: Closing Connection.\n";
    closesocket(listenSocket);
    WSACleanup();
    return 0;
}