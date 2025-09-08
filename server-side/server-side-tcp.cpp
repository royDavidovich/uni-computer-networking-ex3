#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
using namespace std;
#pragma comment(lib, "Ws2_32.lib")
#include <winsock2.h>
#include <string>
#include <string.h>
#include <time.h>
#include <fstream>
#include <sstream>

using std::string;

struct SocketState
{
	SOCKET id;         // Socket handle
	int recv;          // Receiving?
	int send;          // Sending?
	int sendSubType;   // Sending sub-type (unused for HTTP)
	char buffer[4096]; // I/O buffer (bigger for HTTP)
	int len;           // current bytes in buffer
	int bytesToSend;   // response size when sending
};

const int TIME_PORT = 27015;
const int MAX_SOCKETS = 60;
const int EMPTY = 0;
const int LISTEN = 1;
const int RECEIVE = 2;
const int IDLE = 3;
const int SEND = 4;
const int SEND_TIME = 1;
const int SEND_SECONDS = 2;

bool addSocket(SOCKET id, int what);
void removeSocket(int index);
void acceptConnection(int index);
void receiveMessage(int index);
void sendMessage(int index);
static string buildHttpResponse(const string& body,
	const string& status = "200 OK",
	const string& contentType = "text/html");

struct SocketState sockets[MAX_SOCKETS] = { 0 };
int socketsCount = 0;


void main()
{
	// Initialize Winsock (Windows Sockets).

	// Create a WSADATA object called wsaData.
	// The WSADATA structure contains information about the Windows 
	// Sockets implementation.
	WSAData wsaData;

	// Call WSAStartup and return its value as an integer and check for errors.
	// The WSAStartup function initiates the use of WS2_32.DLL by a process.
	// First parameter is the version number 2.2.
	// The WSACleanup function destructs the use of WS2_32.DLL by a process.
	if (NO_ERROR != WSAStartup(MAKEWORD(2, 2), &wsaData))
	{
		cout << "Time Server: Error at WSAStartup()\n";
		return;
	}

	// Server side:
	// Create and bind a socket to an internet address.
	// Listen through the socket for incoming connections.

	// After initialization, a SOCKET object is ready to be instantiated.

	// Create a SOCKET object called listenSocket. 
	// For this application:	use the Internet address family (AF_INET), 
	//							streaming sockets (SOCK_STREAM), 
	//							and the TCP/IP protocol (IPPROTO_TCP).
	SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	// Check for errors to ensure that the socket is a valid socket.
	// Error detection is a key part of successful networking code. 
	// If the socket call fails, it returns INVALID_SOCKET. 
	// The if statement in the previous code is used to catch any errors that
	// may have occurred while creating the socket. WSAGetLastError returns an 
	// error number associated with the last error that occurred.
	if (INVALID_SOCKET == listenSocket)
	{
		cout << "Time Server: Error at socket(): " << WSAGetLastError() << endl;
		WSACleanup();
		return;
	}

	// For a server to communicate on a network, it must bind the socket to 
	// a network address.

	// Need to assemble the required data for connection in sockaddr structure.

	// Create a sockaddr_in object called serverService. 
	sockaddr_in serverService;
	// Address family (must be AF_INET - Internet address family).
	serverService.sin_family = AF_INET;
	// IP address. The sin_addr is a union (s_addr is a unsigned long 
	// (4 bytes) data type).
	// inet_addr (Iternet address) is used to convert a string (char *) 
	// into unsigned long.
	// The IP address is INADDR_ANY to accept connections on all interfaces.
	serverService.sin_addr.s_addr = INADDR_ANY;
	// IP Port. The htons (host to network - short) function converts an
	// unsigned short from host to TCP/IP network byte order 
	// (which is big-endian).
	serverService.sin_port = htons(TIME_PORT);

	// Bind the socket for client's requests.

	// The bind function establishes a connection to a specified socket.
	// The function uses the socket handler, the sockaddr structure (which
	// defines properties of the desired connection) and the length of the
	// sockaddr structure (in bytes).
	if (SOCKET_ERROR == bind(listenSocket, (SOCKADDR*)&serverService, sizeof(serverService)))
	{
		cout << "Time Server: Error at bind(): " << WSAGetLastError() << endl;
		closesocket(listenSocket);
		WSACleanup();
		return;
	}

	// Listen on the Socket for incoming connections.
	// This socket accepts only one connection (no pending connections 
	// from other clients). This sets the backlog parameter.
	if (SOCKET_ERROR == listen(listenSocket, 5))
	{
		cout << "Time Server: Error at listen(): " << WSAGetLastError() << endl;
		closesocket(listenSocket);
		WSACleanup();
		return;
	}
	addSocket(listenSocket, LISTEN);

	// Accept connections and handles them one by one.
	while (true)
	{
		// The select function determines the status of one or more sockets,
		// waiting if necessary, to perform asynchronous I/O. Use fd_sets for
		// sets of handles for reading, writing and exceptions. select gets "timeout" for waiting
		// and still performing other operations (Use NULL for blocking). Finally,
		// select returns the number of descriptors which are ready for use (use FD_ISSET
		// macro to check which descriptor in each set is ready to be used).
		fd_set waitRecv;
		FD_ZERO(&waitRecv);
		for (int i = 0; i < MAX_SOCKETS; i++)
		{
			if ((sockets[i].recv == LISTEN) || (sockets[i].recv == RECEIVE))
				FD_SET(sockets[i].id, &waitRecv);
		}

		fd_set waitSend;
		FD_ZERO(&waitSend);
		for (int i = 0; i < MAX_SOCKETS; i++)
		{
			if (sockets[i].send == SEND)
				FD_SET(sockets[i].id, &waitSend);
		}

		//
		// Wait for interesting event.
		// Note: First argument is ignored. The fourth is for exceptions.
		// And as written above the last is a timeout, hence we are blocked if nothing happens.
		//
		int nfd;
		nfd = select(0, &waitRecv, &waitSend, NULL, NULL);
		if (nfd == SOCKET_ERROR)
		{
			cout << "Time Server: Error at select(): " << WSAGetLastError() << endl;
			WSACleanup();
			return;
		}

		for (int i = 0; i < MAX_SOCKETS && nfd > 0; i++)
		{
			if (FD_ISSET(sockets[i].id, &waitRecv))
			{
				nfd--;
				switch (sockets[i].recv)
				{
				case LISTEN:
					acceptConnection(i);
					break;

				case RECEIVE:
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
				case SEND:
					sendMessage(i);
					break;
				}
			}
		}
	}

	// Closing connections and Winsock.
	cout << "Time Server: Closing Connection.\n";
	closesocket(listenSocket);
	WSACleanup();
}

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
			return (true);
		}
	}
	return (false);
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
	struct sockaddr_in from;		// Address of sending partner
	int fromLen = sizeof(from);

	SOCKET msgSocket = accept(id, (struct sockaddr*)&from, &fromLen);
	if (INVALID_SOCKET == msgSocket)
	{
		cout << "Time Server: Error at accept(): " << WSAGetLastError() << endl;
		return;
	}
	cout << "Time Server: Client " << inet_ntoa(from.sin_addr) << ":" << ntohs(from.sin_port) << " is connected." << endl;

	//
	// Set the socket to be in non-blocking mode.
	//
	unsigned long flag = 1;
	if (ioctlsocket(msgSocket, FIONBIO, &flag) != 0)
	{
		cout << "Time Server: Error at ioctlsocket(): " << WSAGetLastError() << endl;
	}

	if (addSocket(msgSocket, RECEIVE) == false)
	{
		cout << "\t\tToo many connections, dropped!\n";
		closesocket(id);
	}
	return;
}

static bool readFileToString(const string& path, string& out)
{
	std::cout << "Trying to open file: " << path << std::endl;
	std::ifstream f(path, std::ios::binary);
	if (!f) return false;
	std::ostringstream oss;
	oss << f.rdbuf();
	out = oss.str();
	return true;
}

static bool isSafePath(const string& p)
{
	if (p.find("..") != string::npos) return false;
	for (unsigned char c : p) if (c < 32) return false; // no control chars
	return true;
}

static string toLower(string s)
{
	for (auto& c : s) c = (char)std::tolower((unsigned char)c);
	return s;
}

static string getQueryParam(const string& url, const string& key)
{
	size_t q = url.find('?');
	if (q == string::npos) return "";
	string qs = url.substr(q + 1);
	size_t pos = 0;
	while (pos < qs.size())
	{
		size_t amp = qs.find('&', pos);
		string pair = qs.substr(pos, amp == string::npos ? string::npos : amp - pos);
		size_t eq = pair.find('=');
		if (eq != string::npos)
		{
			string k = pair.substr(0, eq);
			string v = pair.substr(eq + 1);
			if (toLower(k) == toLower(key)) return v; // simple, no URL-decoding
		}
		if (amp == string::npos) break;
		pos = amp + 1;
	}
	return "";
}

static string getContentTypeByExt(const string& path)
{
	auto low = toLower(path);
	if (low.rfind(".html") != string::npos) return "text/html; charset=UTF-8";
	if (low.rfind(".htm") != string::npos) return "text/html; charset=UTF-8";
	if (low.rfind(".css") != string::npos) return "text/css";
	if (low.rfind(".js") != string::npos) return "application/javascript";
	if (low.rfind(".json") != string::npos) return "application/json; charset=UTF-8";
	if (low.rfind(".png") != string::npos) return "image/png";
	if (low.rfind(".jpg") != string::npos || low.rfind(".jpeg") != string::npos) return "image/jpeg";
	if (low.rfind(".gif") != string::npos) return "image/gif";
	if (low.rfind(".svg") != string::npos) return "image/svg+xml";
	return "text/plain; charset=UTF-8";
}

// Build a minimal HTTP/1.1 response with Connection: close
static string buildHttpResponse(const string& body, const string& status, const string& contentType)
{
	string headers = "HTTP/1.1 " + status + "\r\n"
		"Content-Type: " + contentType + "\r\n"
		"Content-Length: " + std::to_string(body.size()) + "\r\n"
		"Connection: close\r\n"
		"\r\n";
	return headers + body;
}

// Build only the headers (no body). Use for HEAD/OPTIONS.
static std::string buildHttpHeaders(const std::string& status, const std::string& contentType, 	size_t contentLength,
	const std::string& extraHeaders /*may be empty*/)
{
	std::string hdr = "HTTP/1.1 " + status + "\r\n";
	if (!contentType.empty())
		hdr += "Content-Type: " + contentType + "\r\n";
	hdr += "Content-Length: " + std::to_string(contentLength) + "\r\n";
	if (!extraHeaders.empty())
		hdr += extraHeaders;		// must already contain trailing \r\n if multiple
	hdr += "Connection: close\r\n"
		"\r\n";						// end of headers
	return hdr;						// headers only (no body)
}

void receiveMessage(int index)
{
	SOCKET msgSocket = sockets[index].id;

	int len = sockets[index].len;
	int bytesRecv = recv(msgSocket, &sockets[index].buffer[len],
		sizeof(sockets[index].buffer) - len, 0);

	if (SOCKET_ERROR == bytesRecv)
	{
		cout << "Web Server: Error at recv(): " << WSAGetLastError() << endl;
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

	// ---- parse request line: METHOD SP PATH SP VERSION ----
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
	bool isGET = (_stricmp(method.c_str(), "GET") == 0);
	bool isHEAD = (_stricmp(method.c_str(), "HEAD") == 0);
	bool isOPT = (_stricmp(method.c_str(), "OPTIONS") == 0);

	if (isGET || isHEAD)
	{
		// Split path and query
		std::string pathOnly = path;
		size_t qpos = pathOnly.find('?');
		if (qpos != std::string::npos) pathOnly = pathOnly.substr(0, qpos);

		// Decide which file to serve
		std::string filePath;
		std::string contentType;

		if (pathOnly == "/" || pathOnly == "/index.html")
		{
			// Choose language by ?html= or ?lang= (default = en)
			std::string lang = toLower(getQueryParam(path, "html"));
			if (lang.empty()) lang = toLower(getQueryParam(path, "lang"));
			if (lang != "he" && lang != "en") lang = "en";

			filePath = "www/index." + lang + ".html";
			contentType = "text/html; charset=UTF-8";
		}
		else if (pathOnly.rfind("/assets/", 0) == 0)
		{
			filePath = "www" + pathOnly;               // e.g., /assets/style.css
			contentType = getContentTypeByExt(filePath);  // -> text/css, etc.
		}
		else
		{
			// Optional: serve direct files under www (e.g., /foo.html)
			filePath = "www" + pathOnly;
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
			//std::cout << "[DEBUG] opening: " << filePath << std::endl;

			if (readFileToString(filePath, fileData))
			{
				if (isHEAD)
				{
					// HEAD: send headers only, with the body size we would have sent
					response = buildHttpHeaders("200 OK", contentType, fileData.size(), "");
				}
				else // GET
				{
					response = buildHttpResponse(fileData, "200 OK", contentType);
				}
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
		// OPTIONS: advertise supported methods
		// (No body needed; returning 204 No Content is fine)
		const std::string allow =
			"Allow: GET, HEAD, OPTIONS, POST, PUT, DELETE, TRACE\r\n";

		response = buildHttpHeaders("204 No Content",
			"text/plain; charset=UTF-8",
			0,
			allow);
	}
	else
	{
		// Not implemented yet (POST/PUT/DELETE/TRACE will be added later)
		body = "<!doctype html><html><head><meta charset=\"UTF-8\"><title>405</title></head>"
			"<body><h1>405 Method Not Allowed</h1></body></html>";

		// It’s nice to include Allow here too:
		const std::string allow =
			"Allow: GET, HEAD, OPTIONS, POST, PUT, DELETE, TRACE\r\n";

		// Send body for 405
		std::string headersOnly = buildHttpHeaders("405 Method Not Allowed",
			"text/html; charset=UTF-8",
			body.size(),
			allow);
		response = headersOnly + body;
	}

	// Arm for send
	size_t toCopy = (response.size() < (sizeof(sockets[index].buffer) - 1))
		? response.size() : (sizeof(sockets[index].buffer) - 1);
	memcpy(sockets[index].buffer, response.data(), toCopy);
	sockets[index].buffer[toCopy] = '\0';
	sockets[index].bytesToSend = (int)toCopy;
	sockets[index].send = SEND;
}

void sendMessage(int index)
{
	SOCKET msgSocket = sockets[index].id;

	int bytesSent = send(msgSocket, sockets[index].buffer, sockets[index].bytesToSend, 0);
	if (SOCKET_ERROR == bytesSent)
	{
		cout << "Web Server: Error at send(): " << WSAGetLastError() << endl;
		closesocket(msgSocket);
		removeSocket(index);
		return;
	}

	cout << "Web Server: Sent " << bytesSent << "/" << sockets[index].bytesToSend << " bytes.\n";

	// HTTP/1.1 with Connection: close -> close after single response
	closesocket(msgSocket);
	removeSocket(index);
}