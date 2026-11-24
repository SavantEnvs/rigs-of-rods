// Vehicle tuneups (.tuneup, user/mod supplied): RoR::TuneupUtil::ParseTuneups, then
// ExportTuneup on every parsed tuneup (the save path the game uses for tuneup files).
#include "TuneupFileFormat.h"
#include "ror_app_shim.h"

#include <OgreDataStream.h>
#include <cstdint>

extern "C" int LLVMFuzzerInitialize(int*, char***) { ror_fuzz::Init(); return 0; }

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    ror_fuzz::Reset();
    Ogre::DataStreamPtr src(new Ogre::MemoryDataStream((void*)data, size, false, true));
    Ogre::DataStreamPtr ds(new Ogre::MemoryDataStream(src));
    std::vector<RoR::TuneupDefPtr> tuneups = RoR::TuneupUtil::ParseTuneups(ds);
    for (RoR::TuneupDefPtr& t : tuneups)
    {
        Ogre::DataStreamPtr out(new Ogre::MemoryDataStream(size * 8 + 4096));
        RoR::TuneupUtil::ExportTuneup(out, t);
    }
    return 0;
}
