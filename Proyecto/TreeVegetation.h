#pragma once
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>
#include "Model.h"
#include "AlphaBillboards.h"

class TreeVegetation
{
    std::vector<glm::vec3> sites;
public:
    TreeVegetation(AlphaBillboards& billboards,const GroundSurface& ground,const glm::vec3& origin,float scale)
    {
        const size_t texture=billboards.Load("images/Arbol_RGBA.png");
        std::ifstream input("Models/EntornoCompleto/Arboles.txt");
        if(!input) throw std::runtime_error("Faltan las posiciones de los arboles.");
        std::string line;
        while(std::getline(input,line))
        {
            if(line.empty()||line[0]=='#') continue;
            float bx,bz,x,z;std::istringstream values(line);
            if(!(values>>bx>>bz>>x>>z)) throw std::runtime_error("Posicion de arbol invalida.");
            const float distance=glm::length(glm::vec2(x-bx,z-bz));
            if(distance<2.5f||distance>6.0f) throw std::runtime_error("Arbol fuera del costado de su arbusto.");
            float y=origin.y;
            ground.Sample(x,z,origin.y+4.0f,y);
            sites.push_back((glm::vec3(x,y,z)-origin)*scale);
        }
        if(sites.size()!=8) throw std::runtime_error("Se esperan ocho arboles, uno por arbusto.");
        for(const auto& site:sites) for(int crossed=0;crossed<2;++crossed)
            billboards.Add(texture,site,glm::radians(90.0f*crossed),7.0f*scale,glm::vec3(.66f,.69f,.72f));
    }
    TreeVegetation(const TreeVegetation&)=delete;
    TreeVegetation& operator=(const TreeVegetation&)=delete;
    size_t Count() const {return sites.size();}
    const glm::vec3& Site(size_t i) const {return sites.at(i);}
};
