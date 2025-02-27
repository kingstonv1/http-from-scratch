#pragma once

#include <map>
#include <string>
#include <vector>

class http_request {
    private:
        enum http_verb {
            GET,
            HEAD,
            POST,
            BAD
        };

        http_verb http_method = http_verb::BAD; 
        const char* request_buffer = nullptr;
        std::string buffer_string;
        std::string resource_path;
        std::string request_string;
        std::string http_version;
        std::map<std::string, std::string> headers;
        std::vector<std::string> errors;

        void parse_request();
        void parse_headers();

    public:
        http_request(const char*);
        std::string get_method() const;
        std::string get_path() const;
        std::string get_version() const;
        std::string get_reqstr() const;
        std::map<std::string, std::string> get_headers() const;
};