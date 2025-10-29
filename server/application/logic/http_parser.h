#pragma once
#include <string>
#include <jsoncpp/json/json.h>

class HttpParser {
public:
    static bool parseHttpRequest(const std::string& request, 
                               std::string& method,
                               std::string& path,
                               std::string& body) {
        // 解析请求行
        size_t methodEnd = request.find(' ');
        if (methodEnd == std::string::npos) return false;
        method = request.substr(0, methodEnd);

        size_t pathEnd = request.find(' ', methodEnd + 1);
        if (pathEnd == std::string::npos) return false;
        path = request.substr(methodEnd + 1, pathEnd - methodEnd - 1);

        // 查找body
        size_t bodyStart = request.find("\r\n\r\n");
        if (bodyStart != std::string::npos) {
            body = request.substr(bodyStart + 4);
        }

        return true;
    }

    static bool parseJsonBody(const std::string& body, Json::Value& json) {
        Json::CharReaderBuilder builder;
        Json::CharReader* reader = builder.newCharReader();
        std::string errors;
        
        bool success = reader->parse(body.c_str(), 
                                   body.c_str() + body.length(), 
                                   &json, &errors);
        delete reader;
        return success;
    }
}; 