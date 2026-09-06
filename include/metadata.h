#pragma once
#include <string>
#include <vector>
#include <map>

namespace mmd {
    int getImageMetaData(const char* filename, std::vector<std::string> fields, std::map<std::string, std::string>* metadata);
}
