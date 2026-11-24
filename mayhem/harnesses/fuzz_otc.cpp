// Terrain geometry configs (.otc master + page configs): RoR::OTCParser.
// First byte: low bit selects master (0) or page (1) config; the rest is the file.
#include "OTCFileFormat.h"
#include "ror_app_shim.h"

#include <OgreDataStream.h>
#include <cstdint>

extern "C" int LLVMFuzzerInitialize(int*, char***) { ror_fuzz::Init(); return 0; }

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size < 1) return -1;
    ror_fuzz::Reset();
    const bool page = data[0] & 1;
    Ogre::DataStreamPtr src(new Ogre::MemoryDataStream((void*)(data + 1), size - 1, false, true));
    Ogre::DataStreamPtr ds(new Ogre::MemoryDataStream(src));
    RoR::OTCParser parser;
    if (!page)
    {
        parser.LoadMasterConfig(ds, "fuzz.otc");
    }
    else
    {
        RoR::OTCPage otc_page(0, 0, "fuzz-page.otc", false, false, 0, 0);
        parser.LoadPageConfig(ds, otc_page, "fuzz-page.otc");
    }
    return 0;
}
