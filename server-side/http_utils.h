#ifndef WEBSERVER_HTTP_UTILS_H
#define WEBSERVER_HTTP_UTILS_H

#include <string>

// -------------
// Path helpers
// -------------
bool isSafePath(const std::string& p);

// -------------
// String utils
// -------------
std::string toLower(std::string s);
bool iequals(const std::string& a, const std::string& b);

// --------------------
// HTTP header parsing
// --------------------
std::string getHeaderValue(const char* headersStart, const char* headersEnd, const std::string& name);
std::string getQueryParam(const std::string& url, const std::string& key);

// --------------------
// Content type & body
// --------------------
std::string getContentTypeByExt(const std::string& path);

// Build a full HTTP response (headers + body)
std::string buildHttpResponse(const std::string& body,
                              const std::string& status,
                              const std::string& contentType);

// Build only the headers (no body) for HEAD / OPTIONS
std::string buildHttpHeaders(const std::string& status,
                             const std::string& contentType,
                             size_t contentLength,
                             const std::string& extraHeaders);

// --------------------
// File I/O helpers
// --------------------
bool readFileToString(const std::string& path, std::string& out);
bool writeStringToFile(const std::string& path, const std::string& data, bool& created);
bool fileExists(const std::string& path);
bool deleteFile(const std::string& path);

#endif // WEBSERVER_HTTP_UTILS_H