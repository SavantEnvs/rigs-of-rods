// Actor skin definitions (.skin, shipped in mods): RoR::SkinParser::ParseSkins.
#include "SkinFileFormat.h"
#include "ror_app_shim.h"

#include <OgreDataStream.h>
#include <cstdint>

extern "C" int LLVMFuzzerInitialize(int*, char***) { ror_fuzz::Init(); return 0; }

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    ror_fuzz::Reset();
    Ogre::DataStreamPtr src(new Ogre::MemoryDataStream((void*)data, size, false, true));
    Ogre::DataStreamPtr ds(new Ogre::MemoryDataStream(src));
    std::vector<RoR::SkinDocumentPtr> skins = RoR::SkinParser::ParseSkins(ds);
    volatile size_t n = skins.size();
    (void)n;
    return 0;
}
