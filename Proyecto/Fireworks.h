#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>
#include "Shader.h"
#include "RenderMetrics.h"
#include <glm/gtc/type_ptr.hpp>

class Fireworks
{
    struct Particle{glm::vec3 p{},v{};glm::vec4 color{};float age=0,life=0,size=0;bool rocket=false;};
    struct DrawVertex{glm::vec3 p;glm::vec4 color;float size;};
    static constexpr size_t Capacity=2048;
    std::array<Particle,Capacity> particles{};std::vector<DrawVertex> vertices;
    Shader shader{"Shader/fireworks.vs","Shader/fireworks.frag"};GLuint vao=0,vbo=0;
    glm::vec3 center{};float scale=1;double time=0,nextLaunch=1.2,accumulator=0;uint32_t random=0x11F357A1u;
    unsigned launches=0,bursts=0;bool configured=false;
    float Rand(){random=random*1664525u+1013904223u;return static_cast<float>(random>>8)/16777216.0f;}
    static glm::vec4 Color(unsigned i)
    {const glm::vec4 colors[]={{1,.71f,.16f,1},{1,.28f,.045f,1},{1,.12f,.65f,1},{.12f,.85f,1,1}};return colors[i%4];}
    void Spawn(const Particle& value){for(auto& p:particles)if(p.life<=0){p=value;return;}}
    void Tick(float dt)
    {
        time+=dt;
        if(time>=nextLaunch)
        {
            const float angle=launches*2.399963f;
            auto p=center+glm::vec3(std::cos(angle)*38*scale,1.5f*scale,std::sin(angle)*32*scale);
            Spawn({p,{(Rand()-.5f)*3*scale,55*scale,(Rand()-.5f)*3*scale},Color(launches),0,1.45f,.65f*scale,true});
            ++launches;nextLaunch+=5.0;
        }
        std::array<glm::vec3,4> explosions{};std::array<glm::vec4,4> colors{};size_t count=0;
        for(auto& p:particles)if(p.life>0)
        {
            const float old=p.age;p.age+=dt;
            if(p.age>=p.life)
            {if(p.rocket&&count<explosions.size()){explosions[count]=p.p;colors[count++]=p.color;++bursts;}p.life=0;continue;}
            p.p+=p.v*dt+glm::vec3(0,-4.9f*scale*dt*dt,0);p.v.y-=9.8f*scale*dt;
            if(!p.rocket)p.v*=std::exp(-.45f*dt);
            else if(static_cast<int>(p.age/.04f)!=static_cast<int>(old/.04f))
                Spawn({p.p,{0,-2*scale,0},{1,.5f,.12f,1},0,.55f,.42f*scale,false});
        }
        for(size_t b=0;b<count;++b)for(unsigned i=0;i<112;++i)
        {
            const float y=2*Rand()-1,a=6.2831853f*Rand(),r=std::sqrt(std::max(0.0f,1-y*y));
            const glm::vec3 direction(r*std::cos(a),y,r*std::sin(a));
            const auto c=i%5==0?glm::vec4(1,.78f,.3f,1):colors[b];
            Spawn({explosions[b],direction*((11+9*Rand())*scale),c,0,2.4f+Rand()*1.2f,.7f*scale,false});
        }
    }
public:
    Fireworks()
    {
        vertices.reserve(Capacity);glGenVertexArrays(1,&vao);glGenBuffers(1,&vbo);
        RenderState::BindVao(vao);glBindBuffer(GL_ARRAY_BUFFER,vbo);glBufferData(GL_ARRAY_BUFFER,Capacity*sizeof(DrawVertex),nullptr,GL_STREAM_DRAW);
        glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(DrawVertex),reinterpret_cast<void*>(offsetof(DrawVertex,p)));
        glEnableVertexAttribArray(1);glVertexAttribPointer(1,4,GL_FLOAT,GL_FALSE,sizeof(DrawVertex),reinterpret_cast<void*>(offsetof(DrawVertex,color)));
        glEnableVertexAttribArray(2);glVertexAttribPointer(2,1,GL_FLOAT,GL_FALSE,sizeof(DrawVertex),reinterpret_cast<void*>(offsetof(DrawVertex,size)));
        RenderState::BindVao(0);
    }
    ~Fireworks(){glDeleteBuffers(1,&vbo);glDeleteVertexArrays(1,&vao);}
    void Configure(const glm::vec3& lo,const glm::vec3& hi,const glm::vec3& origin,float worldScale)
    {scale=worldScale;center=(glm::vec3((lo.x+hi.x)*.5f,lo.y,(lo.z+hi.z)*.5f)-origin)*scale;configured=true;}
    void Update(float dt)
    {if(!configured)return;accumulator+=std::max(0.0f,dt);while(accumulator>=1.0/120){Tick(1.0f/120);accumulator-=1.0/120;}}
    void Draw(const glm::mat4& view,const glm::mat4& projection,int height)
    {
        if(!configured)return;vertices.clear();
        for(const auto& p:particles)if(p.life>0)
        {
            auto color=p.color;const float t=p.age/p.life;color.a=(1-t)*(1-t)*(p.rocket?1.0f:std::min(1.0f,p.age*16));
            vertices.push_back({p.p,color,p.size});
        }
        if(vertices.empty())return;
        shader.Use();glUniformMatrix4fv(shader.Uniform("view"),1,GL_FALSE,glm::value_ptr(view));
        glUniformMatrix4fv(shader.Uniform("projection"),1,GL_FALSE,glm::value_ptr(projection));glUniform1f(shader.Uniform("viewportHeight"),static_cast<float>(height));
        RenderState::BindVao(vao);glBindBuffer(GL_ARRAY_BUFFER,vbo);
        glBufferData(GL_ARRAY_BUFFER,Capacity*sizeof(DrawVertex),nullptr,GL_STREAM_DRAW);
        glBufferSubData(GL_ARRAY_BUFFER,0,vertices.size()*sizeof(DrawVertex),vertices.data());
        glEnable(GL_PROGRAM_POINT_SIZE);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE);glDepthMask(GL_FALSE);
        glDrawArrays(GL_POINTS,0,static_cast<GLsizei>(vertices.size()));++RenderMetrics::draws;
        glDepthMask(GL_TRUE);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDisable(GL_BLEND);glDisable(GL_PROGRAM_POINT_SIZE);
    }
    void Validate()
    {
        for(int i=0;i<120*13;++i)Update(1.0f/120);
        size_t alive=0;for(const auto& p:particles)if(p.life>0){++alive;if(!std::isfinite(p.p.y)||p.age<0||p.age>p.life)throw std::runtime_error("Particula invalida.");}
        if(launches!=3||bursts<2||alive>400)throw std::runtime_error("Secuencia/capacidad de fuegos artificiales incorrecta.");
        particles={};time=accumulator=0;nextLaunch=1.2;launches=bursts=0;random=0x11F357A1u;
        std::cout<<"Pirotecnia OK: lanzamiento cada 5 s, estela, 112 chispas, gravedad/desvanecido; un draw por frame."<<std::endl;
    }
};
