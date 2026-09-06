#include <array>
#include <memory>
#include <sstream>

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

void mmd::getImageMetaData(const char* filename, std::vector<std::string> fields, std::map<std::string,std::string>* metadata)
{
    // Create `exiftool` command
    std::string cmd = "exiftool -s2";
    for (int i = 0; i < fields.size(); i++)
    {
        cmd += " -" + fields[i];
    }
    cmd += " \"" + std::string(filename) + "\"";

    // Loop through lines of `exiftool` output and populate metadata map
    std::istringstream stream(exec(cmd));
    std::string line;
    while (std::getline(stream, line))
    {
        size_t colon = line.find(':');
        if (colon != std::string::npos)
        {
            std::string key = line.substr(0, colon);
            std::string value = line.substr(colon + 2);
            (*metadata)[key] = value;
        }
    }
}

void mmd::getImagesMetaData(std::vector<std::filesystem::path> files, std::vector<std::string> fields, std::map < std::string, std::map<std::string, std::string>>* metadata)
{
    // Create `exiftool` command
    std::string cmd = "exiftool -s2";
    for (int i = 0; i < fields.size(); i++)
    {
        cmd += " -" + fields[i];
    }
    for (int i = 0; i < files.size(); i++)
    {
        cmd += " \"" + files[i].string() + "\"";
    }

    // Loop through lines of `exiftool` output and populate metadata map
    std::istringstream stream(exec(cmd));
    std::string line;
    std::string filename = "";
    while (std::getline(stream, line))
    {
        // Metadata for new file
        if (line.length() > 8 && line.substr(0, 8) == "========") { filename = line.substr(9); /*printf("METADATA: filename=%s\n", filename.c_str());*/ }
        else
        {
            size_t colon = line.find(':');
            if (colon != std::string::npos)
            {
                std::string key = line.substr(0, colon);
                std::string value = line.substr(colon + 2);
                (*metadata)[filename][key] = value;
            }
        }
    }
}