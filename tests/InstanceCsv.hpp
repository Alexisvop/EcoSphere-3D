#pragma once
#include <algorithm>
#include <cmath>
#include <limits>
#include <locale>
#include <sstream>
#include <string>

// IDs/paths/header/order/count stay exact. Numeric serialization permits eight
// float epsilons of relative error (absolute near zero) for compiler trig rounding.
inline bool sameInstanceCsv(const std::string& recorded,const std::string& actual) {
    std::istringstream a(recorded),b(actual);std::string x,y;
    if(!std::getline(a,x) || !std::getline(b,y) || x!=y)return false;
    while(std::getline(a,x)) {
        if(!std::getline(b,y))return false;
        std::istringstream ar(x),br(y);std::string ac,bc;unsigned column=0;
        while(std::getline(ar,ac,',')) {
            if(!std::getline(br,bc,','))return false;
            if(column++<4) { if(ac!=bc)return false; }
            else {
                double av=0,bv=0;std::istringstream an(ac),bn(bc);
                an.imbue(std::locale::classic());bn.imbue(std::locale::classic());
                if(!(an>>av) || !(bn>>bv) || !an.eof() || !bn.eof() || !std::isfinite(av) || !std::isfinite(bv))return false;
                if(std::abs(av-bv)>8*std::numeric_limits<float>::epsilon()*std::max({1.,std::abs(av),std::abs(bv)}))return false;
            }
        }
        if(column!=14 || std::getline(br,bc,','))return false;
    }
    return !std::getline(b,y);
}
