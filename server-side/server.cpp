#include "server.h"
#include "http_utils.h"

#include <iostream>
#include <cstring>

using std::string;

// -----------------------
// Globals
// -----------------------
const std::string DOC_ROOT = "C:/temp/www";
SocketState sockets[MAX_SOCKETS] = { 0 };
int socketsCount = 0;

// -----------------------
// Internal helpers
// -----------------------
static void armSend(int index, const std::string& response)
{
    size_t toCopy = (response.size() < (sizeof(sockets[index].buffer) - 1))
                    ? response.size()
                    : (sizeof(sockets[index].buffer) - 1);
    memcpy(sockets[index].buffer, response.data(), toCopy);
    sockets[index].buffer[toCopy] = '\0';
    sockets[index].bytesToSend = (int)toCopy;
    sockets[index].send = SEND_STATE;
}

// -----------------------
// Server API
// -----------------------
bool addSocket(SOCKET id, int what)
{
    for (int i = 0; i < MAX_SOCKETS; i++)
    {
        if (sockets[i].recv == EMPTY)
        {
            sockets[i].id = id;
            sockets[i].recv = what;
            sockets[i].send = IDLE;
            sockets[i].len = 0;
            socketsCount++;
            return true;
        }
    }
    return false;
}

void removeSocket(int index)
{
    sockets[index].recv = EMPTY;
    sockets[index].send = EMPTY;
    socketsCount--;
}

void acceptConnection(int index)
{
    SOCKET id = sockets[index].id;
    struct sockaddr_in from;
    int fromLen = sizeof(from);

    SOCKET msgSocket = accept(id, (struct sockaddr*)&from, &fromLen);
    if (INVALID_SOCKET == msgSocket)
    {
        std::cout << "Web Server: Error at accept(): " << WSAGetLastError() << std::endl;
        return;
    }
    std::cout << "Web Server: Client " << inet_ntoa(from.sin_addr) << ":" << ntohs(from.sin_port) << " connected.\n";

    unsigned long flag = 1;
    if (ioctlsocket(msgSocket, FIONBIO, &flag) != 0)
    {
        std::cout << "Web Server: Error at ioctlsocket(): " << WSAGetLastError() << std::endl;
    }

    if (!addSocket(msgSocket, RECEIVE_STATE))
    {
        std::cout << "\tToo many connections, dropped!\n";
        closesocket(id);
    }
}

void receiveMessage(int index)
{
    SOCKET msgSocket = sockets[index].id;

    int len = sockets[index].len;
    int bytesRecv = recv(msgSocket, &sockets[index].buffer[len],
                         sizeof(sockets[index].buffer) - len, 0);

    if (SOCKET_ERROR == bytesRecv)
    {
        std::cout << "Web Server: Error at recv(): " << WSAGetLastError() << std::endl;
        closesocket(msgSocket);
        removeSocket(index);
        return;
    }
    if (bytesRecv == 0)
    {
        closesocket(msgSocket);
        removeSocket(index);
        return;
    }

    sockets[index].len += bytesRecv;
    sockets[index].buffer[sockets[index].len] = '\0';

    // Wait for end of headers: \r\n\r\n
    char* headersEnd = strstr(sockets[index].buffer, "\r\n\r\n");
    if (!headersEnd) return;

    const char* req = sockets[index].buffer;

    // Parse request line
    std::string requestLine;
    {
        const char* lineEnd = strstr(req, "\r\n");
        if (!lineEnd) lineEnd = headersEnd;
        requestLine.assign(req, lineEnd);
    }

    std::string method, path, version;
    {
        size_t p1 = requestLine.find(' ');
        size_t p2 = (p1 == std::string::npos) ? std::string::npos : requestLine.find(' ', p1 + 1);
        if (p1 != std::string::npos) method = requestLine.substr(0, p1);
        if (p1 != std::string::npos && p2 != std::string::npos) path = requestLine.substr(p1 + 1, p2 - (p1 + 1));
        if (p2 != std::string::npos) version = requestLine.substr(p2 + 1);
    }

    std::string response, body;
    bool isGET   = (_stricmp(method.c_str(), "GET")     == 0);
    bool isHEAD  = (_stricmp(method.c_str(), "HEAD")    == 0);
    bool isOPT   = (_stricmp(method.c_str(), "OPTIONS") == 0);
    bool isPOST  = (_stricmp(method.c_str(), "POST")    == 0);
    bool isPUT   = (_stricmp(method.c_str(), "PUT")     == 0);
    bool isDEL   = (_stricmp(method.c_str(), "DELETE")  == 0);
    bool isTRACE = (_stricmp(method.c_str(), "TRACE")   == 0);

    if (isGET || isHEAD)
    {
        std::string pathOnly = path;
        size_t qpos = pathOnly.find('?');
        if (qpos != std::string::npos) pathOnly = pathOnly.substr(0, qpos);

        std::string filePath;
        std::string contentType;

        if (pathOnly == "/" || pathOnly == "/index.html")
        {
            std::string lang = toLower(getQueryParam(path, "html"));
            if (lang.empty()) lang = toLower(getQueryParam(path, "lang"));
            if (lang != "he" && lang != "en") lang = "en";

            filePath = DOC_ROOT + "/index." + lang + ".html";
            contentType = "text/html; charset=UTF-8";
        }
        else if (pathOnly.rfind("/assets/", 0) == 0)
        {
            filePath = DOC_ROOT + pathOnly;
            contentType = getContentTypeByExt(filePath);
        }
        else
        {
            filePath = DOC_ROOT + pathOnly;
            contentType = getContentTypeByExt(filePath);
        }

        if (!isSafePath(filePath))
        {
            body = "<!doctype html><html><head><meta charset=\"UTF-8\"><title>400</title></head>"
                   "<body><h1>400 Bad Request</h1></body></html>";
            response = buildHttpResponse(isHEAD ? "" : body, "400 Bad Request", "text/html; charset=UTF-8");
        }
        else
        {
            std::string fileData;
            if (readFileToString(filePath, fileData))
            {
                if (isHEAD)
                    response = buildHttpHeaders("200 OK", contentType, fileData.size(), "");
                else
                    response = buildHttpResponse(fileData, "200 OK", contentType);
            }
            else
            {
                body = "<!doctype html><html><head><meta charset=\"UTF-8\"><title>404</title></head>"
                       "<body><h1>404 Not Found</h1><p>File not found.</p></body></html>";
                if (isHEAD)
                    response = buildHttpHeaders("404 Not Found", "text/html; charset=UTF-8", body.size(), "");
                else
                    response = buildHttpResponse(body, "404 Not Found", "text/html; charset=UTF-8");
            }
        }
    }
    else if (isOPT)
    {
        const std::string allow = "Allow: GET, HEAD, OPTIONS, POST, PUT, DELETE, TRACE\r\n";
        response = buildHttpHeaders("204 No Content", "text/plain; charset=UTF-8", 0, allow);
    }
    else if (isPOST)
    {
        const char* fullStart   = sockets[index].buffer;
        const char* firstCRLF   = strstr(fullStart, "\r\n");
        const char* headersStart= firstCRLF ? firstCRLF + 2 : fullStart;
        const char* headersEndPtr = headersEnd;
        const char* bodyStart   = headersEndPtr + 4;

        std::string clStr = getHeaderValue(headersStart, headersEndPtr, "Content-Length");
        int contentLength = 0;
        if (!clStr.empty())
        {
            contentLength = atoi(clStr.c_str());
            if (contentLength < 0) contentLength = 0;
        }

        int bytesSoFar  = sockets[index].len;
        int headerBytes = (int)(bodyStart - sockets[index].buffer);
        int haveBody    = bytesSoFar - headerBytes;

        if (haveBody < contentLength) return;

        std::string postBody(bodyStart, bodyStart + contentLength);
        std::cout << "[POST] Body (" << contentLength << " bytes): " << postBody << std::endl;

        std::string html =
            "<!doctype html><html><head><meta charset=\"UTF-8\"><title>POST OK</title></head>"
            "<body><h1>POST Received</h1><p>Thank you.</p></body></html>";

        response = buildHttpResponse(html, "200 OK", "text/html; charset=UTF-8");
    }
    else if (isPUT)
    {
        std::string pathOnly = path;
        size_t qpos = pathOnly.find('?');
        if (qpos != std::string::npos) pathOnly = pathOnly.substr(0, qpos);
        std::string filePath = (pathOnly == "/" || pathOnly.empty()) ? DOC_ROOT+ "/upload.txt" : DOC_ROOT + pathOnly;

        if (!isSafePath(filePath))
        {
            std::string err =
                "<!doctype html><html><head><meta charset=\"UTF-8\"><title>400</title></head>"
                "<body><h1>400 Bad Request</h1></body></html>";
            response = buildHttpResponse(err, "400 Bad Request", "text/html; charset=UTF-8");
        }
        else
        {
            const char* fullStart   = sockets[index].buffer;
            const char* firstCRLF   = strstr(fullStart, "\r\n");
            const char* headersStart= firstCRLF ? firstCRLF + 2 : fullStart;
            const char* headersEndPtr = headersEnd;
            const char* bodyStart   = headersEndPtr + 4;

            std::string clStr = getHeaderValue(headersStart, headersEndPtr, "Content-Length");
            int contentLength = 0;
            if (!clStr.empty())
            {
                contentLength = atoi(clStr.c_str());
                if (contentLength < 0) contentLength = 0;
            }

            int bytesSoFar  = sockets[index].len;
            int headerBytes = (int)(bodyStart - sockets[index].buffer);
            int haveBody    = bytesSoFar - headerBytes;

            if (haveBody < contentLength) return;

            std::string data(bodyStart, bodyStart + contentLength);

            bool created = false;
            if (writeStringToFile(filePath, data, created))
            {
                std::string msg =
                    "<!doctype html><html><head><meta charset=\"UTF-8\"><title>PUT OK</title></head>"
                    "<body><h1>PUT Stored</h1><p>Saved to " + filePath + "</p></body></html>";
                response = buildHttpResponse(msg, created ? "201 Created" : "200 OK", "text/html; charset=UTF-8");
            }
            else
            {
                std::string err =
                    "<!doctype html><html><head><meta charset=\"UTF-8\"><title>500</title></head>"
                    "<body><h1>500 Internal Server Error</h1></body></html>";
                response = buildHttpResponse(err, "500 Internal Server Error", "text/html; charset=UTF-8");
            }
        }
    }
    else if (isDEL)
    {
        std::string pathOnly = path;
        size_t qpos = pathOnly.find('?');
        if (qpos != std::string::npos) pathOnly = pathOnly.substr(0, qpos);

        if (pathOnly == "/" || pathOnly.empty())
        {
            std::string err =
                "<!doctype html><html><head><meta charset=\"UTF-8\"><title>400</title></head>"
                "<body><h1>400 Bad Request</h1><p>Cannot DELETE root.</p></body></html>";
            response = buildHttpResponse(err, "400 Bad Request", "text/html; charset=UTF-8");
        }
        else
        {
            std::string filePath = DOC_ROOT + pathOnly;

            if (!isSafePath(filePath))
            {
                std::string err =
                    "<!doctype html><html><head><meta charset=\"UTF-8\"><title>400</title></head>"
                    "<body><h1>400 Bad Request</h1></body></html>";
                response = buildHttpResponse(err, "400 Bad Request", "text/html; charset=UTF-8");
            }
            else if (!fileExists(filePath))
            {
                std::string err =
                    "<!doctype html><html><head><meta charset=\"UTF-8\"><title>404</title></head>"
                    "<body><h1>404 Not Found</h1></body></html>";
                response = buildHttpResponse(err, "404 Not Found", "text/html; charset=UTF-8");
            }
            else
            {
                if (deleteFile(filePath))
                {
                    std::string ok =
                        "<!doctype html><html><head><meta charset=\"UTF-8\"><title>200</title></head>"
                        "<body><h1>Deleted</h1><p>Removed " + filePath + "</p></body></html>";
                    response = buildHttpResponse(ok, "200 OK", "text/html; charset=UTF-8");
                }
                else
                {
                    std::string err =
                        "<!doctype html><html><head><meta charset=\"UTF-8\"><title>500</title></head>"
                        "<body><h1>500 Internal Server Error</h1></body></html>";
                    response = buildHttpResponse(err, "500 Internal Server Error", "text/html; charset=UTF-8");
                }
            }
        }
    }
    else if (isTRACE)
    {
        std::string echo(sockets[index].buffer, sockets[index].len);
        response = buildHttpResponse(echo, "200 OK", "message/http");
    }
    else
    {
        body = "<!doctype html><html><head><meta charset=\"UTF-8\"><title>405</title></head>"
               "<body><h1>405 Method Not Allowed</h1></body></html>";

        const std::string allow = "Allow: GET, HEAD, OPTIONS, POST, PUT, DELETE, TRACE\r\n";
        std::string headersOnly = buildHttpHeaders("405 Method Not Allowed",
                                                   "text/html; charset=UTF-8",
                                                   body.size(),
                                                   allow);
        response = headersOnly + body;
    }

    armSend(index, response);
}

void sendMessage(int index)
{
    SOCKET msgSocket = sockets[index].id;

    int bytesSent = send(msgSocket, sockets[index].buffer, sockets[index].bytesToSend, 0);
    if (SOCKET_ERROR == bytesSent)
    {
        std::cout << "Web Server: Error at send(): " << WSAGetLastError() << std::endl;
        closesocket(msgSocket);
        removeSocket(index);
        return;
    }

    std::cout << "Web Server: Sent " << bytesSent << "/" << sockets[index].bytesToSend << " bytes.\n";

    closesocket(msgSocket);
    removeSocket(index);
}