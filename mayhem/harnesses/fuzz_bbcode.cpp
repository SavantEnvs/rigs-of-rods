// BBCode from the online repository (resource descriptions shown in GUI_RepositorySelector):
// bbcpp::BBDocument::load, then a walk of the resulting node tree.
#include "BBDocument.h"

#include <cstdint>
#include <string>

static size_t Walk(const bbcpp::BBNode& node, int depth)
{
    size_t n = node.getNodeName().size();
    for (const auto& child : node.getChildren())
        n += 1 + Walk(*child, depth + 1);
    return n;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    std::string s(reinterpret_cast<const char*>(data), size);
    auto doc = bbcpp::BBDocument::create();
    doc->load(s.begin(), s.end());
    volatile size_t n = Walk(*doc, 0);
    (void)n;
    return 0;
}
