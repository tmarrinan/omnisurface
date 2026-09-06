#include <array>
#include <memory>

#include "metadata.h"

#if defined(_WIN32)
#define pclose _pclose
#define popen _popen
#endif

static std::string exec(const std::string& cmd)
{
    std::array<char, 128> buffer;
    std::string result = "";

    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) return result;

    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) != nullptr)
    {
        result += buffer.data();
    }
    return result;
}

int mmd::getImageMetaData(const char* filename, std::vector<std::string> fields, std::map<std::string,std::string>* metadata)
{
    std::string cmd = "exiftool -s2";
    for (int i = 0; i < fields.size(); i++)
    {
        cmd += " -" + fields[i];
    }
    cmd += " \"" + std::string(filename) + "\"";

    std::string output = exec(cmd);

    // TODO: Loop through lines of result and populate metadata map
    
    // TEST
    (*metadata)["ALL"] = output;

    return 0;
}