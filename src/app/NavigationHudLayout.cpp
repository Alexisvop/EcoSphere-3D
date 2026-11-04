#include "NavigationHudLayout.hpp"
#include <algorithm>
#include <array>
#include <sstream>

namespace ecosphere {
namespace {
// Fuente bitmap 5x7 de trazos propios, limitada a los mensajes ASCII de navegación.
std::array<unsigned,7> glyph(char c) {
    switch(c) {
    case 'A':return {14,17,17,31,17,17,17}; case 'B':return {30,17,17,30,17,17,30};
    case 'C':return {14,17,16,16,16,17,14}; case 'D':return {30,17,17,17,17,17,30};
    case 'E':return {31,16,16,30,16,16,31}; case 'F':return {31,16,16,30,16,16,16};
    case 'G':return {14,17,16,23,17,17,15}; case 'H':return {17,17,17,31,17,17,17};
    case 'I':return {31,4,4,4,4,4,31}; case 'J':return {7,2,2,2,18,18,12};
    case 'K':return {17,18,20,24,20,18,17}; case 'L':return {16,16,16,16,16,16,31};
    case 'M':return {17,27,21,21,17,17,17}; case 'N':return {17,25,21,19,17,17,17};
    case 'O':return {14,17,17,17,17,17,14}; case 'P':return {30,17,17,30,16,16,16};
    case 'Q':return {14,17,17,17,21,18,13}; case 'R':return {30,17,17,30,20,18,17};
    case 'S':return {15,16,16,14,1,1,30}; case 'T':return {31,4,4,4,4,4,4};
    case 'U':return {17,17,17,17,17,17,14}; case 'V':return {17,17,17,17,17,10,4};
    case 'W':return {17,17,17,21,21,27,17}; case 'X':return {17,17,10,4,10,17,17};
    case 'Y':return {17,17,10,4,4,4,4}; case 'Z':return {31,1,2,4,8,16,31};
    case '1':return {4,12,4,4,4,4,14}; case '2':return {14,17,1,2,4,8,31};
    case '|':return {4,4,4,4,4,4,4}; case '-':return {0,0,0,31,0,0,0};
    default:return {};
    }
}
void quad(Mesh& mesh,float x,float y,float w,float h) {
    const auto i=static_cast<std::uint32_t>(mesh.vertices.size());
    for(const auto& p:{glm::vec3(x,y,0),glm::vec3(x+w,y,0),glm::vec3(x+w,y+h,0),glm::vec3(x,y+h,0)})
        mesh.vertices.push_back({p,{0,0,1},{0,0}});
    mesh.indices.insert(mesh.indices.end(),{i,i+1,i+2,i,i+2,i+3});
}
}
NavigationHudLayout layoutNavigationHud(const std::string& message,int width,int height) {
    NavigationHudLayout result;
    if(width<160 || height<96 || message.empty())return result;
    result.pixel=width<800 ? 1.5f : 2.f;
    const float padding=8, advance=6*result.pixel, lineHeight=9*result.pixel;
    const auto columns=static_cast<std::size_t>((std::min(float(width)-20,1100.f)-2*padding)/advance);
    const auto maxRows=static_cast<std::size_t>((height/3.f-2*padding)/lineHeight);
    std::istringstream paragraphs(message);std::string paragraph;
    while(std::getline(paragraphs,paragraph)) {
        std::istringstream words(paragraph);std::string word,row;std::vector<std::string> wrapped;
        while(words>>word) {
            while(word.size()>columns) {
                if(!row.empty()){wrapped.push_back(row);row.clear();}
                wrapped.push_back(word.substr(0,columns));word.erase(0,columns);
            }
            if(!row.empty() && row.size()+1+word.size()>columns){wrapped.push_back(row);row.clear();}
            if(!row.empty())row+=' ';
            row+=word;
        }
        if(!row.empty())wrapped.push_back(row);
        // Navigation/error paragraphs precede optional help; never show half a help paragraph.
        if(result.lines.size()+wrapped.size()>maxRows){result.truncated=true;break;}
        result.lines.insert(result.lines.end(),wrapped.begin(),wrapped.end());
    }
    if(result.lines.empty())return result;
    std::size_t longest=0;for(const auto& line:result.lines)longest=std::max(longest,line.size());
    result.width=2*padding+longest*advance;
    result.height=2*padding+result.lines.size()*lineHeight;
    result.y=height-10-result.height;
    return result;
}
std::shared_ptr<const Model> makeNavigationHudModel(const NavigationHudLayout& layout) {
    auto model=std::make_shared<Model>();
    Mesh panel;panel.material.unlit=true;panel.color={.008f,.025f,.035f,1};
    quad(panel,0,0,layout.width,layout.height);
    for(auto& v:panel.vertices)v.position.z=-.1f;
    model->meshes.push_back(std::move(panel));
    Mesh letters;letters.material.unlit=true;letters.color={.93f,.98f,.9f,1};
    for(std::size_t row=0;row<layout.lines.size();++row)for(std::size_t column=0;column<layout.lines[row].size();++column) {
        const auto pixels=glyph(layout.lines[row][column]);
        for(unsigned y=0;y<7;++y)for(unsigned x=0;x<5;++x)
            if(pixels[y] & (1u<<(4-x)))quad(letters,8+(column*6+x)*layout.pixel,
                layout.height-8-(row*9+y+1)*layout.pixel,layout.pixel,layout.pixel);
    }
    model->meshes.push_back(std::move(letters));
    ModelNode node;node.meshes={0,1};model->nodes.push_back(std::move(node));
    return model;
}
}
