#include <iostream>
#include <cstdint>
#include <string>
#include <fstream>
#include "vk360.h"

Vulkan360::Vulkan360(const char* config_filename)
{
    readDisplayConfig(config_filename);
}

Vulkan360::~Vulkan360()
{
    // Clean up
}

bool Vulkan360::hasValidDisplayConfig()
{
    return _config.load_success;
}

void Vulkan360::initializeWindow()
{
    // TODO: GLFW - DX12 (Windows) or OpenGL (Linux)
    //  * use NativeRenderHandler (`_native_renderer`)
}

///////////////////////////////////////////////////////////////
//   PRIVATE METHODS                                         //
///////////////////////////////////////////////////////////////

void Vulkan360::readDisplayConfig(const char* config_filename)
{
    _config.load_success = true;

    std::ifstream config_file(config_filename);
    if (!config_file.is_open())
    {
        fprintf(stderr, "Error: config file '%s' not found'\n", config_filename);
        return;
    }

    std::string line;
    int lineno = 0;
    while (std::getline(config_file, line))
    {
        if (lineno == 0 && line != "OmniSurface Config")
        {
            fprintf(stderr, "Error: config file format not recognized - 1st line should be 'OmniSurface Config'\n");
            _config.load_success = false;
        }
        else if (line.length() >= 13 && line.substr(0, 12) == "Base Shape: ")
        {
            std::string shape = line.substr(12);
            if (shape == "plane")
            {
                _config.base_shape = DisplayBaseShape::BASE_SHAPE_PLANE;
            }
            else if (shape == "cylinder")
            {
                _config.base_shape = DisplayBaseShape::BASE_SHAPE_CYLINDER;
            }
            else
            {
                fprintf(stderr, "Error: config file format not recognized - Base Shape must be 'plane' or 'cylinder'\n");
                _config.load_success = false;
            }
        }
        else if (line.length() >= 9 && line.substr(0, 8) == "Facets: ")
        {
            _config.facets = std::stoul(line.substr(8));
        }
        else if (line.length() >= 9 && line.substr(0, 8) == "Radius: ")
        {
            _config.radius = std::stod(line.substr(8));
        }
        else if (line.length() >= 9 && line.substr(0, 8) == "Height: ")
        {
            _config.height = std::stod(line.substr(8));
        }
        else if (line.length() >= 13 && line.substr(0, 12) == "Resolution: ")
        {
            std::string size = line.substr(12);
            size_t delim = size.find("x");
            if (delim == std::string::npos)
            {
                fprintf(stderr, "Error: config file format not recognized - Resolution should be WIDTHxHEIGHT\n");
                _config.load_success = false;
            }
            else
            {
                _config.resolution_w = std::stoul(size.substr(0, delim));
                _config.resolution_h = std::stoul(size.substr(delim + 1));
            }
        }

        lineno++;
    }

    config_file.close();
}
