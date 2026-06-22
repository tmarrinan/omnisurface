#include <iostream>
#include <cstdint>
#include <string>
#include <fstream>
#include "vk360.h"

int vk360::init(const char* config_filename, VulkanData *vk)
{
    // CONFIG:
    // OmniSurface Config
    // Base Shape: [cylinder/plane]
    // Facets: N
    // Radius: N (meters)
    // Height: N (meters)
    // Resolution: WxH (pixels)
    std::ifstream config(config_filename);
    if (!config.is_open())
    {
        fprintf(stderr, "Error: config file '%s' not found'\n", config_filename);
        return -1;
    }
    
    std::string base_shape = "";
    uint32_t facets = 0;
    double radius = 0;
    double height = 0;
    uint32_t resolution_w = 0;
    uint32_t resolution_h = 0;
    
    std::string line;
    int lineno = 0;
    while (std::getline(config, line))
    {
        if (lineno == 0 && line != "OmniSurface Config")
        {
            fprintf(stderr, "Error: config file format not recognized - 1st line should be 'OmniSurface Config'\n");
            return -1;
        }
        else if (line.length() >= 13 && line.substr(0, 12) == "Base Shape: ")
        {
            base_shape = line.substr(12);
        }
        else if (line.length() >= 9 && line.substr(0, 8) == "Facets: ")
        {
            facets = std::stoul(line.substr(8));
        }
        else if (line.length() >= 9 && line.substr(0, 8) == "Radius: ")
        {
            radius = std::stod(line.substr(8));
        }
        else if (line.length() >= 9 && line.substr(0, 8) == "Height: ")
        {
            height = std::stod(line.substr(8));
        }
        else if (line.length() >= 13 && line.substr(0, 12) == "Resolution: ")
        {
            std::string size = line.substr(12);
            size_t delim = size.find("x");
            if (delim == std::string::npos)
            {
                fprintf(stderr, "Error: config file format not recognized - Resolution should be WIDTHxHEIGHT\n");
                return -1;
            }
            resolution_w = std::stoul(size.substr(0, delim));
            resolution_h = std::stoul(size.substr(delim + 1));
        }
        
        lineno++;
    }
    printf("OmniSurface Config read in...\n");
    printf(" Base Shape = %s\n", base_shape.c_str());
    printf(" Facets = %u\n", facets);
    printf(" Radius = %.6lf\n", radius);
    printf(" Height = %.6lf\n", height);
    printf(" Resolution = %u x %u\n", resolution_w, resolution_h);
    
    config.close();
    
    return 0;
}
