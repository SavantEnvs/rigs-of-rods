// Truck/vehicle definition files (.truck/.car/.load/.boat/...): RigDef::Parser, then the
// Validator and Serializer the game runs on every parsed actor definition.
#include "RigDef_Parser.h"
#include "RigDef_Serializer.h"
#include "RigDef_Validator.h"
#include "ror_app_shim.h"

#include <OgreDataStream.h>
#include <cstdint>
#include <cstring>
#include <vector>

extern "C" int LLVMFuzzerInitialize(int*, char***) { ror_fuzz::Init(); return 0; }

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    ror_fuzz::Reset();
    std::vector<uint8_t> buf(data, data + size);
    Ogre::MemoryDataStream ds(buf.data(), buf.size(), /*freeOnClose=*/false, /*readOnly=*/true);

    RigDef::Parser parser;
    parser.Prepare();
    parser.ProcessOgreStream(&ds, "fuzz");
    parser.Finalize();
    RigDef::DocumentPtr doc = parser.GetFile();

    RigDef::Validator validator;
    validator.Setup(doc);
    validator.Validate();

    RigDef::Serializer serializer(doc);
    serializer.Serialize();
    volatile size_t n = serializer.GetOutput().size();
    (void)n;
    return 0;
}
