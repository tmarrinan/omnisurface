#pragma once
#include <string>

namespace mmd {
    int getImageLicenseAndStereo(const char* filename, std::string* license, bool* is_stereo);
}
