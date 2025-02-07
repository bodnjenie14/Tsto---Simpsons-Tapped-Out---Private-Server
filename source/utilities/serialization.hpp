#pragma once
#include <iostream>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

namespace utils::serialization {
    template<typename T>
    std::string serialize_protobuf(const T& message) {
        std::string output;
        if (!message.SerializeToString(&output)) {
            // Handle serialization failure
            std::cout << "[ERROR] Failed to serialize protobuf message" << std::endl;
            return "";
        }
        return output;
    }

    inline std::string serialize_json(const rapidjson::Document& doc) {
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        if (!doc.Accept(writer)) {
            // Handle serialization failure
            std::cout << "[ERROR] Failed to serialize JSON document" << std::endl;
            return "";
        }
        return std::string(buffer.GetString(), buffer.GetLength());
    }

    inline rapidjson::GenericValue<rapidjson::UTF8<char>> make_json_string(
        const std::string& str,
        rapidjson::MemoryPoolAllocator<rapidjson::CrtAllocator>& allocator) {
        return rapidjson::Value(str.c_str(), str.length(), allocator);
    }
}