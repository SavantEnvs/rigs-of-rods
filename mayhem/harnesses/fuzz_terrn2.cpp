// Terrain manifests (.terrn2, INI-like): RoR::Terrn2Parser::LoadTerrn2 over RoR::ConfigFile.
#include "Terrn2FileFormat.h"
#include "ror_app_shim.h"

#include <OgreDataStream.h>
#include <OgreException.h>
#include <cstdint>

extern "C" int LLVMFuzzerInitialize(int*, char***) { ror_fuzz::Init(); return 0; }

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    ror_fuzz::Reset();
    // MemoryDataStream copies the bytes (the stream owns its buffer).
    Ogre::DataStreamPtr ds(new Ogre::MemoryDataStream((void*)data, size, /*freeOnClose=*/false, /*readOnly=*/true));
    Ogre::DataStreamPtr copy(new Ogre::MemoryDataStream(ds));
    RoR::Terrn2Parser parser;
    try
    {
        parser.LoadTerrn2(copy);
    }
    catch (Ogre::Exception&)
    {
        // OGRE's ConfigFile/StringConverter report malformed input by throwing; the game's
        // caller (TerrainManager) catches these too.
    }
    return 0;
}
