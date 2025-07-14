#include "http_response.h"
#include "http_request.h"

#include <string>
#include <sstream>
#include <map>
#include <vector>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <utility>
#include <cassert>
#include <ctime>
#include <cstring>

std::map<std::string, std::string> http_response::default_headers() {
    std::time_t now = std::time(nullptr);
    std::tm gmt_time = *std::gmtime(&now);

    std::ostringstream oss;
    oss << std::put_time(&gmt_time, "%a, %d %b %Y %H:%M:%S GMT");

    std::map<std::string, std::string> d = {
        {"Connection", "Close"},
        {"Date", oss.str()},
        {"Server", "Custom HTTP v0.2"}
    };

    return d;
}

bool http_response::parse_get() { 
    std::ifstream served_file(request_path);

    if (!served_file.is_open()) {
        set_response_params(500, "Error opening file");
        return false;
    }

    std::string file_contents((std::istreambuf_iterator<char>(served_file)),
                                 std::istreambuf_iterator<char>());

    response_body = file_contents;

    int body_size = response_body.length();
    std::string size_header = std::to_string(body_size);
    headers.insert(std::make_pair("Content-Length", size_header));

    set_response_params(200, "OK");
    compose_response();

    return true;
}

bool http_response::parse_post() { 
    return true;
}

bool http_response::parse_head() { 
    return true;
}

bool http_response::validate_response() {
    return true;
}

std::string http_response::message_from_code(int code) {
    switch (code) {
        case (200):
            return "OK";
        case (201):
            return "Created";
        case (204):
            return "No Content";
        case (304):
            return "Not Modified";
        case (400):
            return "Bad Request";
        case (401):
            return "Unauthorized";
        case (403):
            return "Forbidden";
        case (404):
            return "Not Found";
        case (408):
            return "Request Timeout";
        case (429):
            return "Too Many Requests";
        case (500):
            return "Internal Server Error";
        case (501):
            return "Not Implemented";
        case (505):
            return "HTTP Version Not Supported";
    }

    assert(false && "Unhandled status code " + code);
    return "Unknown Status Code";
}

// precondition: set_response_params() should have been called already.
void http_response::compose_response() {
    if (complete) { return; }

    response_message = "HTTP/1.1";

    response_message += " " + std::to_string(response_code) + " ";
    response_message += message_from_code(response_code) + "\n";

    for (const auto& pair : headers) { response_message += pair.first + ": " + pair.second + "\n"; }

    response_message += "\n";

    response_message += response_body;
    response_message += '\n';

    complete = true;
}

void http_response::finish_fill_buffer() {
    assert(response_message.size() < 2048 && "Message size too long, split up");
    assert(response_message.size() > 0 && "Need to send some kind of response.");
    strcpy(response_content, response_message.c_str());
}

void http_response::set_response_params(int code, std::string msg) {
    this->response_code = code;
    this->response_details = msg;
    this->response_message = message_from_code(code);
}

// weirdly placed method but....
bool http_response::contains_disallowed_chars(std::string s) {
    std::vector<char> disallowed {'\0', '%', '#', '\\', ':', '*', '<', '>', '|', '"'};
    return std::any_of(
        disallowed.begin(),
        disallowed.end(), 
        [&](char c){ return s.find(c) != std::string::npos; });
}

bool http_response::validate_path() {
    if (request.get_path().size() > 255) {
        set_response_params(400, "Requested path too long.");
        return false;
    }

    if (contains_disallowed_chars(request.get_path())) {
        set_response_params(403, "Requested path is malformed.");
        return false;
    }

    std::string file_req;
    std::string default_file = "/index.html";
    std::string server_path = "./content";

    if (request.get_path() == "/") { file_req = default_file; } 
    else { file_req = request.get_path(); }

    std::filesystem::path requested;
    std::filesystem::path server;

    try {
        requested = std::filesystem::canonical(server_path + file_req);
        server = std::filesystem::canonical(server_path);
    } catch (const std::filesystem::filesystem_error&) {
        set_response_params(500, "Error parsing file path");
        return false;
    }

    if (requested.string().rfind(server.string(), 0) != 0) {
        set_response_params(403, "Requested resource " + requested.string() + " not allowed.");
        return false;
    }

    if (!std::filesystem::exists(requested)) {
        set_response_params(404, "Requested resource " + requested.string() + " not found.");
        return false;
    }

    request_path = requested;

    return true;
}

const char* http_response::get_response() { return this->response_content; }
int http_response::get_code() { return this->response_code; }
std::string http_response::get_message() { return this->response_message; }
std::string http_response::get_details() { return this->response_details; }
std::map<std::string, std::string> http_response::get_headers() { return this->headers; }

http_response::http_response(const http_request* req) : request(*req) {
    // default to empty response body. Replaced during succesful requests.
    response_content[0] = '\0'; 
    headers = default_headers();

    // assert stays during debugging/development
    assert(req != nullptr && "HTTP Request passed into response parser is null.");
    
    if (!validate_path()) { 
        compose_response();
        finish_fill_buffer();

        // DEBUG
        validate_response();

        return;
    }

    std::string method = request.get_method(); 

    if (method == "GET") {
        parse_get();
    } else if (method == "HEAD") {
        parse_head();
    } else if (method == "POST") {
        parse_post();
    } else {
        set_response_params(501, "Request method not supported.");
        compose_response();
    }

    finish_fill_buffer();


    // DEBUG
    validate_response();


    complete = true;
}

