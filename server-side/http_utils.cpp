#include "http_utils.h"

#include <fstream>
#include <sstream>
#include <cctype>
#include <cstring>

using std::string;

bool readFileToString(const string& path, string& out)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::ostringstream oss;
    oss << f.rdbuf();
    out = oss.str();
    return true;
}

bool writeStringToFile(const string& path, const string& data, bool& created)
{
    std::ifstream probe(path, std::ios::binary);
    created = !probe.good();
    probe.close();

    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out.write(data.data(), (std::streamsize)data.size());
    return out.good();
}

bool fileExists(const string& path)
{
    std::ifstream f(path, std::ios::binary);
    return f.good();
}

bool deleteFile(const string& path)
{
    return std::remove(path.c_str()) == 0;
}

bool isSafePath(const string& p)
{
    if (p.find("..") != string::npos) return false;
    for (unsigned char c : p) if (c < 32) return false;
    return true;
}

std::string toLower(std::string s)
{
    for (auto& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

bool iequals(const std::string& a, const std::string& b)
{
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
    {
        unsigned char ca = (unsigned char)a[i];
        unsigned char cb = (unsigned char)b[i];
        if (std::tolower(ca) != std::tolower(cb)) return false;
    }
    return true;
}

std::string getHeaderValue(const char* headersStart, const char* headersEnd, const std::string& name)
{
    const char* p = headersStart;
    while (p < headersEnd)
    {
        const char* lineEnd = (const char*)memchr(p, '\n', headersEnd - p);
        if (!lineEnd) lineEnd = headersEnd;

        const char* cr = lineEnd;
        if (cr > p && *(cr - 1) == '\r') --cr;

        const char* colon = (const char*)memchr(p, ':', cr - p);
        if (colon)
        {
            std::string key(p, colon - p);
            const char* valStart = colon + 1;
            if (valStart < cr && *valStart == ' ') ++valStart;
            std::string val(valStart, cr - valStart);

            if (iequals(key, name)) return val;
        }

        p = lineEnd + 1;
    }
    return "";
}

std::string getQueryParam(const std::string& url, const std::string& key)
{
    size_t q = url.find('?');
    if (q == std::string::npos) return "";
    std::string qs = url.substr(q + 1);
    size_t pos = 0;
    while (pos < qs.size())
    {
        size_t amp = qs.find('&', pos);
        std::string pair = qs.substr(pos, amp == std::string::npos ? std::string::npos : amp - pos);
        size_t eq = pair.find('=');
        if (eq != std::string::npos)
        {
            std::string k = pair.substr(0, eq);
            std::string v = pair.substr(eq + 1);
            if (toLower(k) == toLower(key)) return v;
        }
        if (amp == std::string::npos) break;
        pos = amp + 1;
    }
    return "";
}

std::string getContentTypeByExt(const std::string& path)
{
    auto low = toLower(path);
    if (low.rfind(".html") != std::string::npos) return "text/html; charset=UTF-8";
    if (low.rfind(".htm")  != std::string::npos) return "text/html; charset=UTF-8";
    if (low.rfind(".css")  != std::string::npos) return "text/css";
    if (low.rfind(".js")   != std::string::npos) return "application/javascript";
    if (low.rfind(".json") != std::string::npos) return "application/json; charset=UTF-8";
    if (low.rfind(".png")  != std::string::npos) return "image/png";
    if (low.rfind(".jpg")  != std::string::npos || low.rfind(".jpeg") != std::string::npos) return "image/jpeg";
    if (low.rfind(".gif")  != std::string::npos) return "image/gif";
    if (low.rfind(".svg")  != std::string::npos) return "image/svg+xml";
    return "text/plain; charset=UTF-8";
}

std::string buildHttpResponse(const std::string& body, const std::string& status, const std::string& contentType)
{
    std::string headers = "HTTP/1.1 " + status + "\r\n"
                          "Content-Type: " + contentType + "\r\n"
                          "Content-Length: " + std::to_string(body.size()) + "\r\n"
                          "Connection: close\r\n"
                          "\r\n";
    return headers + body;
}

std::string buildHttpHeaders(const std::string& status, const std::string& contentType, size_t contentLength,
                             const std::string& extraHeaders)
{
    std::string hdr = "HTTP/1.1 " + status + "\r\n";
    if (!contentType.empty())
        hdr += "Content-Type: " + contentType + "\r\n";
    hdr += "Content-Length: " + std::to_string(contentLength) + "\r\n";
    if (!extraHeaders.empty())
        hdr += extraHeaders;
    hdr += "Connection: close\r\n"
           "\r\n";
    return hdr;
}