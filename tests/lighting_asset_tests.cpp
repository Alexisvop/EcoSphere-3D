#include "assets/AssetManager.hpp"
#include "AssetFixture.hpp"
#include <iostream>

namespace {
using namespace fixture_test;
std::string materialJson(const std::string& material) {
    auto json=replace(valid,"\"meshes\":[","\"materials\":[{"+material+"}],\"meshes\":[");
    return replace(json,"\"indices\":3","\"indices\":3,\"material\":0");
}
}
int main(int argc,char** argv) {
    try {
        using namespace fixture_test;
        using namespace ecosphere;
        check(argc==2,"Need scratch path");
        const auto root=fs::absolute(argv[1]);fs::create_directories(root);
        AssetManager assets(root);
        const auto load=[&](const std::string& json) {
            assets.clear(); fixture(root/"material.glb",json); return assets.loadModel("material.glb");
        };
        for(const auto& mode: {std::string("OPAQUE"),std::string("MASK"),std::string("BLEND")}) {
            auto model=load(materialJson("\"alphaMode\":\""+mode+"\",\"alphaCutoff\":0.4,\"doubleSided\":true,"
                "\"pbrMetallicRoughness\":{\"baseColorFactor\":[0.1,0.2,0.3,0.35],\"metallicFactor\":0.7,\"roughnessFactor\":0.2}"));
            const auto& m=model->meshes[0];
            check(!m.material.unlit && m.material.doubleSided,"Lit/doubleSided flags");
            check(m.material.alphaMode==(mode=="OPAQUE"?AlphaMode::opaque:(mode=="MASK"?AlphaMode::mask:AlphaMode::blend)),"alphaMode preservation");
            check(m.color.a==0.35f && m.material.alphaCutoff==0.4f && m.material.metallic==0.7f && m.material.roughness==0.2f,"Material factors");
            check(assets.loadModel("./material.glb")==model,"Material cache identity");
        }
        auto unlit=load(materialJson("\"extensions\":{\"KHR_materials_unlit\":{}}"));
        check(unlit->meshes[0].material.unlit,"unlit extension lost");
        const auto fails=[&](const std::string& json,const char* reason) {
            try { load(json); } catch(const std::exception& e) { check(std::string(e.what()).find(reason)!=std::string::npos,e.what()); return; }
            throw std::runtime_error("Invalid GLB material accepted");
        };
        for(const auto& bad:{"\"metallicFactor\":-1","\"roughnessFactor\":1.1","\"roughnessFactor\":1e99"})
            fails(materialJson(std::string("\"pbrMetallicRoughness\":{")+bad+"}"),"material factor");
        fails(materialJson("\"pbrMetallicRoughness\":{\"baseColorFactor\":[1,1,1,-0.1]}"),"base color");
        fails(materialJson("\"alphaMode\":\"MASK\",\"alphaCutoff\":-1"),"material factor");
        fails(replace(valid,"\"scale\":[2,3,4]","\"scale\":[2,0,4]"),"Singular");
        fails(replace(valid,"\"componentType\":5126,\"count\":3,\"type\":\"VEC2\"","\"componentType\":5126,\"count\":2,\"type\":\"VEC2\""),"Invalid glTF");
        // PNG RGBA 2x2 propio: scanlines filter=0, zlib, CRC32 (sin asset externo).
        const std::vector<unsigned char> png{137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,2,0,0,0,2,8,6,0,0,0,114,182,13,36,0,0,0,23,73,68,65,84,120,156,99,248,207,192,224,192,240,159,225,0,3,3,144,252,15,4,0,55,229,7,250,238,231,183,220,0,0,0,0,73,69,78,68,174,66,96,130};
        auto imageJson=materialJson("\"alphaMode\":\"BLEND\",\"pbrMetallicRoughness\":{\"baseColorFactor\":[1,1,1,0.5],\"baseColorTexture\":{\"index\":0}}");
        imageJson=replace(imageJson,"\"byteLength\":104","\"byteLength\":"+std::to_string(104+png.size()));
        imageJson=replace(imageJson,"\"accessors\":[","\"images\":[{\"bufferView\":2,\"mimeType\":\"image/png\"}],\"textures\":[{\"source\":0}],\"accessors\":[");
        imageJson=replace(imageJson,"\"byteLength\":6}]","\"byteLength\":6},{\"buffer\":0,\"byteOffset\":104,\"byteLength\":"+std::to_string(png.size())+"}]");
        assets.clear(); fixture(root/"rgba.glb",imageJson,png);
        const auto rgba=assets.loadModel("rgba.glb");
        check(rgba->images[0].rgba[3]==64 && rgba->images[0].rgba[7]==192 && rgba->images[0].rgba[11]==0 &&
              rgba->meshes[0].color.a==0.5f && rgba->meshes[0].texture==0,"RGBA/factor alpha must remain linear and unmodified");
        auto noUv=replace(imageJson,",\"TEXCOORD_0\":2","");
        assets.clear(); fixture(root/"no-uv.glb",noUv,png);
        bool rejected=false;
        try { assets.loadModel("no-uv.glb"); } catch(const std::exception& e) { rejected=std::string(e.what()).find("requires TEXCOORD_0")!=std::string::npos; }
        check(rejected,"Texture without UV accepted");
        fails(materialJson("\"extensions\":{\"KHR_materials_transmission\":{\"transmissionFactor\":0.5}}"),"Unsupported material");
        std::cout<<"Lighting GLB PASS: unlit/OPAQUE/MASK/BLEND, doubleSided, factors/alpha, ranges, singular, UV, cache/retry\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
