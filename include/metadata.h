#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include <map>

namespace mmd {
    void getImageMetaData(const char* filename, std::vector<std::string> fields, std::map<std::string, std::string>* metadata);
    void getImagesMetaData(std::vector<std::filesystem::path> files, std::vector<std::string> fields, std::map < std::string, std::map<std::string, std::string>>* metadata);
}
