// RoR's generic tokenizer (utils/GenericFileFormat): the base format of .addonpart, .character,
// .tuneup-style and script-loaded documents. First byte = tokenizer options bitmask (the
// OPTION_* flags, bits 1..8), the rest is the document. The parsed document is serialized
// back out with saveToDataStream, which walks every token.
#include "GenericFileFormat.h"
#include "ror_app_shim.h"

#include <OgreDataStream.h>
#include <cstdint>

extern "C" int LLVMFuzzerInitialize(int*, char***) { ror_fuzz::Init(); return 0; }

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size < 1) return -1;
    ror_fuzz::Reset();
    const BitMask_t options = ((BitMask_t)data[0]) << 1;
    Ogre::DataStreamPtr src(new Ogre::MemoryDataStream((void*)(data + 1), size - 1, false, true));
    Ogre::DataStreamPtr in(new Ogre::MemoryDataStream(src));

    // Kept on the stack: GenericDocumentPtr's AddRef/Release assert against the game's main
    // thread id, which needs the full AppContext.
    RoR::GenericDocument doc;
    doc.loadFromDataStream(in, options);

    Ogre::DataStreamPtr out(new Ogre::MemoryDataStream(size * 4 + 1024));
    doc.saveToDataStream(out);
    return 0;
}
