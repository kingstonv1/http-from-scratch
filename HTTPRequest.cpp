#include "HTTPRequest.h"

#include <map>
#include <string>
#include <sstream>
#include <vector>

void HTTPRequest::parse_request() {
    std::string rs = this->buffer_string;

    std::string verb;
    std::string path;
    std::string version;
    int count = 0;
    
    // Fill out our variables based on HTTP Request structure: GET / HTTP/1.1
    for (size_t i = 0, max = buffer_string.size(); i < max; i++) {
        if (std::isspace(rs[i])) {
            if (i == 0) { return; }
            if (!std::isspace(rs[i - 1])) { count++; }
            continue;
        }

        if (count == 0) {
            verb += toupper(rs[i]);
        } else if (count == 1) {
            path += rs[i];
        } else if (count == 2) {
            version += rs[i];
        } else {
            break;
        }
    }

    if (verb == "GET") {
        this->http_method = http_verb::GET;
    } else if (verb == "POST") {
        this->http_method = http_verb::POST;
    } else if (verb == "HEAD") {
        this->http_method = http_verb::HEAD;
    } else {
        this->http_method = http_verb::BAD;
    }

    this->resource_path = path;
    this->http_version = version;
}

void HTTPRequest::parse_headers() {
    std::istringstream stream(this->buffer_string);
    std::string ln;

    std::getline(stream, ln, '\r'); // the first (req) line is ignored
    stream.ignore(1); // ignore the \n after \r in http requests

    while (std::getline(stream, ln, '\r')) {
        stream.ignore(1);

        if (ln.empty()) continue;

        size_t colon = ln.find(':');

        if (colon == std::string::npos) {
            continue;
        }

        std::string key = ln.substr(0, colon); 
        std::string val = ln.substr(colon + 1);

        // Trim whitespace from value
        size_t start = val.find_first_not_of(" \t");
        size_t end = val.find_last_not_of(" \t");
        val = (start == std::string::npos) ? "" : val.substr(start, end - start + 1);

        this->headers[key] = val;
    }
}

HTTPRequest::HTTPRequest(const char* buf) {
    this->request_buffer = buf;
    this->buffer_string = std::string(buf);
    parse_request();
    parse_headers();
}

std::string HTTPRequest::get_method() const { 
    switch (this->http_method) {
        case (0):
            return "GET";
        case (1):
            return "HEAD";
        case (2):
            return "POST";
        case (3):
            return "BAD";
    }

    return "UNDEF";
}

std::string HTTPRequest::get_path() const { return resource_path; }
std::string HTTPRequest::get_version() const { return http_version; }
std::string HTTPRequest::get_reqstr() const { return request_string; }
std::map<std::string, std::string> HTTPRequest::get_headers() const { return headers; }