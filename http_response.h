#ifndef HTTPSERVER_HTTP_RESPONSE

#define HTTPSERVER_HTTP_RESPONSE

#include <string>
#include <map>
#include <filesystem>

#include "http_request.h"

#endif

class http_response {
    private:
        char response_content[2048];
        const http_request& request = nullptr;
        int response_code = -1;
        std::string response_message;
        std::string response_details; //a string for debugging describing any issues 
        std::string response_body;
        std::map<std::string, std::string> headers;
        std::filesystem::path request_path;
        bool complete = false;
        
        bool parse_get();
        bool parse_post();
        bool parse_head();
        bool contains_disallowed_chars(std::string);
        bool validate_path();
        bool validate_response();
        void compose_response();
        void compose_response(std::string);
        void compose_response(std::filesystem::path);
        void set_response_params(int, std::string);
        void finish_fill_buffer();
        std::string message_from_code(int);
        std::map<std::string, std::string> default_headers();

    public:
        http_response(const http_request*);
        const char *get_response();
        int get_code();
        std::string get_message();
        std::string get_details();
        std::map<std::string, std::string> get_headers();
};