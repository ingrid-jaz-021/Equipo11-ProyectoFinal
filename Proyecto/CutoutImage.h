#pragma once
#include <algorithm>
#include <array>
#include <stdexcept>
#include <vector>

// Prepara una copia en RAM; los PNG originales permanecen intactos.
inline void PrepareCutoutImage(std::vector<unsigned char>& rgba,int& width,int& height)
{
    int left=width,top=height,right=-1,bottom=-1;
    for(int y=0;y<height;++y) for(int x=0;x<width;++x)
        if(rgba[(y*width+x)*4+3]>=26)
        {left=std::min(left,x);right=std::max(right,x);top=std::min(top,y);bottom=std::max(bottom,y);}
    if(right<left) throw std::runtime_error("Textura de fachada/arbol sin contenido visible.");
    const int croppedWidth=right-left+1,croppedHeight=bottom-top+1;
    std::vector<unsigned char> cropped(croppedWidth*croppedHeight*4);
    for(int y=0;y<croppedHeight;++y)
        std::copy_n(rgba.data()+((y+top)*width+left)*4,croppedWidth*4,cropped.data()+y*croppedWidth*4);
    rgba.swap(cropped);width=croppedWidth;height=croppedHeight;

    // Colores de referencia a dos texeles del contorno: elimina el matte claro
    // de los bordes semitransparentes y rellena RGB bajo alfa cero. El filtrado
    // lineal/mipmaps nunca vuelve a mezclar el blanco del fondo recortado.
    const int count=width*height;
    std::vector<int> owner(count,-1),queue;queue.reserve(count);
    for(int y=0;y<height;++y) for(int x=0;x<width;++x)
    {
        bool interior=rgba[(y*width+x)*4+3]==255;
        for(int dy=-2;dy<=2&&interior;++dy) for(int dx=-2;dx<=2;++dx)
        {
            const int nx=x+dx,ny=y+dy;
            if(nx<0||ny<0||nx>=width||ny>=height||rgba[(ny*width+nx)*4+3]!=255)
            {interior=false;break;}
        }
        if(interior){const int i=y*width+x;owner[i]=i;queue.push_back(i);}
    }
    // Siluetas muy finas tambien conservan su alfa y su color mas opaco.
    if(queue.empty())
        for(int i=0;i<count;++i) if(rgba[i*4+3]>=230){owner[i]=i;queue.push_back(i);}
    if(queue.empty()) throw std::runtime_error("Textura recortada sin pixeles de referencia.");
    for(size_t head=0;head<queue.size();++head)
    {
        const int i=queue[head],x=i%width,y=i/width;
        const std::array<int,4> adjacent={x>0?i-1:-1,x+1<width?i+1:-1,y>0?i-width:-1,y+1<height?i+width:-1};
        for(int next:adjacent) if(next>=0&&owner[next]<0){owner[next]=owner[i];queue.push_back(next);}
    }
    const auto original=rgba;
    for(int i=0;i<count;++i) if(owner[i]!=i)
        for(int c=0;c<3;++c) rgba[i*4+c]=original[owner[i]*4+c];
}
