// Known-answer self-test for the parsers the fuzz targets reach (mayhem/test.sh runs it).
// Each case parses a fixed input and checks parsed values; prints "PASS <name>"/"FAIL <name>"
// per case and a final "SUMMARY passed=<p> failed=<f>" line.
#include "BBDocument.h"
#include "GenericFileFormat.h"
#include "ODefFileFormat.h"
#include "RigDef_Parser.h"
#include "RigDef_Serializer.h"
#include "RigDef_Validator.h"
#include "TObjFileFormat.h"
#include "ProceduralManager.h"
#include "Terrn2FileFormat.h"
#include "SkinFileFormat.h"
#include "TuneupFileFormat.h"
#include "ror_app_shim.h"

#include <OgreDataStream.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

static int g_pass = 0, g_fail = 0;
static void Check(const char* name, bool ok)
{
    printf("%s %s\n", ok ? "PASS" : "FAIL", name);
    (ok ? g_pass : g_fail)++;
}
static Ogre::DataStreamPtr Stream(const std::string& s)
{
    Ogre::DataStreamPtr src(new Ogre::MemoryDataStream((void*)s.data(), s.size(), false, true));
    return Ogre::DataStreamPtr(new Ogre::MemoryDataStream(src));
}
static bool Near(float a, float b) { return std::fabs(a - b) < 1e-4f; }

static void TestRigDef()
{
    const std::string truck =
        "Selftest truck\n"
        "globals\n1000, 0, tracks/transred\n"
        "nodes\n"
        "0, 0.0, 0.0, 0.0\n"
        "1, 1.5, 0.0, 0.0\n"
        "2, 0.0, 2.5, -1.0, l\n"
        "beams\n0, 1\n1, 2\n2, 0\n"
        "end\n";
    Ogre::DataStreamPtr ds = Stream(truck);
    RigDef::Parser parser;
    parser.Prepare();
    parser.ProcessOgreStream(ds.get(), "selftest");
    parser.Finalize();
    RigDef::DocumentPtr doc = parser.GetFile();
    auto& m = doc->root_module;
    Check("rigdef_name", doc->name == "Selftest truck");
    Check("rigdef_nodes", m->nodes.size() == 3);
    Check("rigdef_beams", m->beams.size() == 3);
    Check("rigdef_node_pos", m->nodes.size() == 3 && Near(m->nodes[1].position.x, 1.5f) &&
                             Near(m->nodes[2].position.y, 2.5f) && Near(m->nodes[2].position.z, -1.0f));
    Check("rigdef_globals_mass", m->globals.size() == 1 && Near(m->globals[0].dry_mass, 1000.f));

    RigDef::Validator v;
    v.Setup(doc);
    Check("rigdef_validate", v.Validate());

    RigDef::Serializer s(doc);
    s.Serialize();
    const std::string out = s.GetOutput();
    Check("rigdef_serialize", out.find("nodes") != std::string::npos && out.find("beams") != std::string::npos);
}

static void TestODef()
{
    Ogre::DataStreamPtr ds = Stream("160mairportgrass.mesh\n2,1,0.5\nbeginmesh\nmesh 160mairportgrass.mesh\nendmesh\nend\n");
    RoR::ODefParser p;
    p.Prepare();
    p.ProcessOgreStream(ds.get());
    auto doc = p.Finalize();
    Check("odef_mesh", doc && doc->header.mesh_name == "160mairportgrass.mesh");
    Check("odef_scale", doc && Near(doc->header.scale.x, 2.f) && Near(doc->header.scale.y, 1.f) && Near(doc->header.scale.z, 0.5f));
}

static void TestTObj()
{
    Ogre::DataStreamPtr ds = Stream("// comment\n10.5, 2, -3, 0, 90, 0, bench\n1, 2, 3, 0, 0, 0, house\n");
    RoR::TObjParser p;
    p.Prepare();
    p.ProcessOgreStream(ds.get());
    auto doc = p.Finalize();
    Check("tobj_count", doc && doc->objects.size() == 2);
    Check("tobj_values", doc && doc->objects.size() == 2 && Near(doc->objects[0].position.x, 10.5f) &&
                         Near(doc->objects[0].rotation.y, 90.f) && std::string(doc->objects[1].odef_name) == "house");
}

static void TestTerrn2()
{
    Ogre::DataStreamPtr ds = Stream("[General]\nName = Selftest Island\nGeometryConfig = island.otc\nWater=1\nWaterLine = 12.5\nGravity = -5\n");
    RoR::Terrn2Parser p;
    auto doc = p.LoadTerrn2(ds);
    Check("terrn2_name", doc && doc->name == "Selftest Island");
    Check("terrn2_values", doc && doc->ogre_ter_conf_filename == "island.otc" && Near(doc->water_height, 12.5f) && Near(doc->gravity, -5.f));
}

static void TestGenericDoc()
{
    Ogre::DataStreamPtr ds = Stream("keyword 1 2.5 \"str\" true\n");
    RoR::GenericDocument doc;
    doc.loadFromDataStream(ds, RoR::GenericDocument::OPTION_ALLOW_NAKED_STRINGS);
    using RoR::TokenType;
    bool ok = doc.tokens.size() >= 5 && doc.tokens[0].type == TokenType::KEYWORD &&
              doc.tokens[1].type == TokenType::INT && Near(doc.tokens[1].data, 1.f) &&
              doc.tokens[2].type == TokenType::FLOAT && Near(doc.tokens[2].data, 2.5f) &&
              doc.tokens[3].type == TokenType::STRING && doc.tokens[4].type == TokenType::BOOL;
    Check("genericdoc_tokens", ok);
}

static void TestBBCode()
{
    auto doc = bbcpp::BBDocument::create();
    doc->load(std::string("hello [b]bold[/b] [url=http://x]link[/url]"));
    const auto& ch = doc->getChildren();
    bool has_b = false, has_url = false;
    for (auto& c : ch) { has_b |= c->getNodeName() == "b"; has_url |= c->getNodeName() == "url"; }
    Check("bbcode_tree", ch.size() >= 3 && ch[0]->getNodeName() == "hello " && has_b && has_url);
}

static void TestSkin()
{
    Ogre::DataStreamPtr ds = Stream("Red paint\n{\n\tdescription = Bright red\n\tauthorid = 42\n\treplacetexture = body.dds, body-red.dds\n}\n");
    auto skins = RoR::SkinParser::ParseSkins(ds);
    Check("skin_count", skins.size() == 1);
    Check("skin_values", skins.size() == 1 && skins[0]->name == "Red paint" && skins[0]->description == "Bright red" &&
                         skins[0]->author_id == 42 && skins[0]->replace_textures.count("body.dds") == 1 &&
                         skins[0]->replace_textures["body.dds"] == "body-red.dds");
}

static void TestTuneup()
{
    Ogre::DataStreamPtr ds = Stream("My tuneup\n{\n\tauthor_id = 42\n\tguid = ABC-DEF\n\tunwanted_prop = 3\n\tfilename = car.truck\n}\n");
    auto tuneups = RoR::TuneupUtil::ParseTuneups(ds);
    Check("tuneup_count", tuneups.size() == 1);
    Check("tuneup_values", tuneups.size() == 1 && tuneups[0]->name == "My tuneup" && tuneups[0]->author_id == 42 &&
                           tuneups[0]->guid == "abc-def" && tuneups[0]->filename == "car.truck" &&
                           tuneups[0]->unwanted_props.count(3) == 1);
    if (tuneups.size() == 1)
    {
        Ogre::MemoryDataStream* mem = new Ogre::MemoryDataStream(4096);
        Ogre::DataStreamPtr out(mem);
        RoR::TuneupUtil::ExportTuneup(out, tuneups[0]);
        std::string text((const char*)mem->getPtr(), mem->tell());
        Check("tuneup_export", text.find("My tuneup") == 0 && text.find("unwanted_prop") != std::string::npos);
    }
    else
    {
        Check("tuneup_export", false);
    }
}

int main()
{
    ror_fuzz::Init();
    TestRigDef();
    TestODef();
    TestTObj();
    TestTerrn2();
    TestGenericDoc();
    TestBBCode();
    TestSkin();
    TestTuneup();
    printf("SUMMARY passed=%d failed=%d\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
