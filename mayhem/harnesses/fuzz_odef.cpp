// Static object definition files (.odef): RoR::ODefParser.
#include "ODefFileFormat.h"
#include "ror_app_shim.h"

#include <OgreDataStream.h>
#include <cstdint>
#include <vector>

extern "C" int LLVMFuzzerInitialize(int*, char***) { ror_fuzz::Init(); return 0; }

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    ror_fuzz::Reset();
    std::vector<uint8_t> buf(data, data + size);
    Ogre::MemoryDataStream ds(buf.data(), buf.size(), false, true);
    RoR::ODefParser parser;
    parser.Prepare();
    parser.ProcessOgreStream(&ds);
    std::shared_ptr<RoR::ODefDocument> doc = parser.Finalize();
    return 0;
}
