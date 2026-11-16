#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>
#include "Shader.h"
#include "RenderMetrics.h"
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Fireworks
{
    struct Particle{glm::vec3 p{},v{};glm::vec4 color{};float age=0,life=0,size=0;bool rocket=false;};
    struct DrawVertex{glm::vec3 p;glm::vec4 color;float size;};
    static constexpr size_t Capacity=2048;
    std::array<Particle,Capacity> particles{};std::vector<DrawVertex> vertices;
    Shader shader{"Shader/fireworks.vs","Shader/fireworks.frag"};GLuint vao=0,vbo=0;
    glm::vec3 center{};float scale=1;double time=0,nextLaunch=1.2,accumulator=0;uint32_t random=0x11F357A1u;
    unsigned launches=0,bursts=0;bool configured=false,active=false;
    void Reset(){particles={};vertices.clear();time=accumulator=0;nextLaunch=1.2;launches=bursts=0;random=0x11F357A1u;}
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
    void Update(float dt,bool playerAtLagoon)
    {
        if(!configured)return;
        if(!playerAtLagoon){if(active)Reset();active=false;return;}
        active=true;accumulator+=std::max(0.0f,dt);
        while(accumulator>=1.0/120){Tick(1.0f/120);accumulator-=1.0/120;}
    }
    bool Active() const{return active;}
    void Draw(const glm::mat4& view,const glm::mat4& projection,int height)
    {
        if(!configured||!active)return;vertices.clear();
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
        glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LESS);
        glEnable(GL_PROGRAM_POINT_SIZE);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE);glDepthMask(GL_FALSE);
        glDrawArrays(GL_POINTS,0,static_cast<GLsizei>(vertices.size()));++RenderMetrics::draws;
        glDepthMask(GL_TRUE);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDisable(GL_BLEND);glDisable(GL_PROGRAM_POINT_SIZE);
    }
    void Validate()
    {
        for(int i=0;i<120*13;++i)Update(1.0f/120,false);
        if(launches||active)throw std::runtime_error("Pirotecnia activa fuera de la Laguna.");
        for(int i=0;i<120*13;++i)Update(1.0f/120,true);
        size_t alive=0;for(const auto& p:particles)if(p.life>0){++alive;if(!std::isfinite(p.p.y)||p.age<0||p.age>p.life)throw std::runtime_error("Particula invalida.");}
        if(launches!=3||bursts<2||alive>400)throw std::runtime_error("Secuencia/capacidad de fuegos artificiales incorrecta.");
        ValidateDepth();Update(0,false);
        if(launches||active)throw std::runtime_error("Pirotecnia residual al salir de la Laguna.");
        std::cout<<"Pirotecnia OK: apagada fuera de Laguna/bote; profundidad GPU oculta las particulas detras de opacos."<<std::endl;
    }
private:
    void ValidateDepth()
    {
        Reset();active=true;particles[0]={center,{0,0,0},{1,.5f,.1f,1},.1f,2,40*scale,false};
        GLint viewport[4];glGetIntegerv(GL_VIEWPORT,viewport);
        GLuint fbo=0,color=0,depth=0;glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);
        glGenTextures(1,&color);RenderState::BindTexture2D(color);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,32,32,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,color,0);
        glGenRenderbuffers(1,&depth);glBindRenderbuffer(GL_RENDERBUFFER,depth);
        glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,32,32);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth);
        if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)throw std::runtime_error("FBO pirotecnia incompleto.");
        glViewport(0,0,32,32);glClearColor(0,0,0,0);
        const auto view=glm::lookAt(center+glm::vec3(0,0,10*scale),center,glm::vec3(0,1,0));
        const auto projection=glm::ortho(-2*scale,2*scale,-2*scale,2*scale,scale,20*scale);
        unsigned sums[2]{};std::array<unsigned char,32*32*4> pixels{};
        for(int pass=0;pass<2;++pass)
        {
            glDepthMask(GL_TRUE);glClearDepth(pass?1:0);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
            glDisable(GL_DEPTH_TEST); // Draw must establish its own depth test.
            Draw(view,projection,32);glReadPixels(0,0,32,32,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
            for(size_t i=0;i<pixels.size();i+=4)sums[pass]+=pixels[i];
        }
        glClearDepth(1);glBindFramebuffer(GL_FRAMEBUFFER,0);glDeleteRenderbuffers(1,&depth);
        glDeleteTextures(1,&color);glDeleteFramebuffers(1,&fbo);glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
        RenderState::Invalidate();
        if(sums[0]!=0||sums[1]<20)throw std::runtime_error("Profundidad pirotecnia: oculto="+std::to_string(sums[0])+", visible="+std::to_string(sums[1]));
    }
};
