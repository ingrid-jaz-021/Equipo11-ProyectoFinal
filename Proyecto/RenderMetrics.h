#pragma once
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>
#include <GL/glew.h>

namespace RenderMetrics
{
    inline uint64_t draws=0,triangles=0,culled=0;
    inline void Reset(){draws=triangles=culled=0;}
    inline void Draw(uint64_t count,uint64_t instances=1){++draws;triangles+=count/3*instances;}
}

// Timer GPU asincrono: nunca espera GL_QUERY_RESULT si aun no esta disponible.
class FrameProfiler
{
    using Clock=std::chrono::steady_clock;
    struct Sample{int frame;double cpuUpdate=0,cpuRender=0,gpu=-1;uint64_t draws=0,triangles=0,culled=0;};
    struct Query{GLuint id=0;int frame=-1;};
    std::array<Query,8> queries{};std::vector<Sample> samples;bool enabled=false,queryActive=false;
    Clock::time_point frameStart,renderStart;int frame=0;
    static double Ms(Clock::duration duration){return std::chrono::duration<double,std::milli>(duration).count();}
    void Poll(Query& q)
    {
        if(q.frame<0)return;
        GLint available=0;glGetQueryObjectiv(q.id,GL_QUERY_RESULT_AVAILABLE,&available);
        if(!available)return;
        GLuint64 elapsed=0;glGetQueryObjectui64v(q.id,GL_QUERY_RESULT,&elapsed);
        if(q.frame<static_cast<int>(samples.size()))samples[q.frame].gpu=elapsed/1000000.0;
        q.frame=-1;
    }
public:
    explicit FrameProfiler(bool enabled):enabled(enabled)
    {if(enabled){samples.reserve(1000);for(auto& q:queries)glGenQueries(1,&q.id);}}
    ~FrameProfiler(){if(enabled)for(auto& q:queries)glDeleteQueries(1,&q.id);}
    void BeginFrame(int index){frame=index;RenderMetrics::Reset();if(enabled)frameStart=Clock::now();}
    void BeginRender()
    {
        if(!enabled)return;
        renderStart=Clock::now();auto& q=queries[frame%queries.size()];Poll(q);
        queryActive=q.frame<0;
        if(queryActive){q.frame=frame;glBeginQuery(GL_TIME_ELAPSED,q.id);}
    }
    void EndRender()
    {
        if(!enabled)return;
        if(queryActive)glEndQuery(GL_TIME_ELAPSED);
        samples.push_back({frame,Ms(renderStart-frameStart),Ms(Clock::now()-renderStart),-1,
            RenderMetrics::draws,RenderMetrics::triangles,RenderMetrics::culled});
    }
    void Write(const std::string& file)
    {
        if(!enabled)return;
        for(auto& q:queries)Poll(q);
        std::ofstream output(file);output<<std::setprecision(9);
        output<<"{\n\"renderer\":"<<std::quoted(reinterpret_cast<const char*>(glGetString(GL_RENDERER)))<<",\n\"frames\":[\n";
        std::vector<double> cpu,gpu;double calls=0,triangles=0;
        for(size_t i=0;i<samples.size();++i)
        {
            const auto& s=samples[i];
            output<<(i?",\n":"")<<"{\"frame\":"<<s.frame<<",\"update_ms\":"<<s.cpuUpdate<<",\"render_ms\":"<<s.cpuRender
                <<",\"gpu_ms\":"<<s.gpu<<",\"draw_calls\":"<<s.draws<<",\"triangles\":"<<s.triangles<<",\"culled\":"<<s.culled<<"}";
            if(s.frame>=30){cpu.push_back(s.cpuRender);if(s.gpu>=0)gpu.push_back(s.gpu);calls+=s.draws;triangles+=s.triangles;}
        }
        const auto percentile=[](std::vector<double> values,float p){if(values.empty())return 0.0;std::sort(values.begin(),values.end());return values[static_cast<size_t>((values.size()-1)*p)];};
        output<<"\n],\n\"summary\":{\"cpu_render_p50_ms\":"<<percentile(cpu,.5f)<<",\"cpu_render_p95_ms\":"<<percentile(cpu,.95f)
            <<",\"gpu_p50_ms\":"<<percentile(gpu,.5f)<<",\"gpu_p95_ms\":"<<percentile(gpu,.95f)
            <<",\"mean_draw_calls\":"<<calls/std::max(size_t(1),cpu.size())<<",\"mean_triangles\":"<<triangles/std::max(size_t(1),cpu.size())<<"}}\n";
        std::cout<<"Perfil: CPU render p50/p95 "<<percentile(cpu,.5f)<<"/"<<percentile(cpu,.95f)<<" ms; GPU "
            <<percentile(gpu,.5f)<<"/"<<percentile(gpu,.95f)<<" ms; draws "<<calls/std::max(size_t(1),cpu.size())<<". "<<file<<std::endl;
    }
};
