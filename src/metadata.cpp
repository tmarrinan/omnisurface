#include "metadata.h"

int mmd::getImageLicenseAndStereo(const char* filename, std::string* license, bool* is_stereo)
{
    *license = "";
    *is_stereo = false;

    // TODO: popen to call `exiftool -s2 -Copyright -License -StereoMode -MultiViewCount "<filename>"`
    //  * each item will be on its own line if it exists
    //  * format = ItemName: ItemValue
    
    return 0;
}