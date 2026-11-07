#pragma once
#include <algorithm>
#include <cctype>
#include <cmath>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include <assimp/scene.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "Skinning.h"

class SkeletalAnimator
{
    struct Node { int parent; glm::vec3 p, s; glm::quat q; };
    struct VecKey { float time; glm::vec3 value; };
    struct QuatKey { float time; glm::quat value; };
    struct Track { std::vector<VecKey> p, s; std::vector<QuatKey> q; };
    struct Clip { std::string name; float duration; std::unordered_map<int, Track> tracks; };
    struct Bone { int node; glm::mat4 offset; };
    std::vector<Node> nodes;
    std::unordered_map<std::string, int> nodeIds, boneIds;
    std::vector<Bone> bones;
    std::vector<Clip> clips;
    std::vector<glm::mat4> palette;
    std::vector<glm::mat4> globalPose;
    std::unordered_map<std::string,std::pair<unsigned,float>> influenceStats;
    glm::mat4 inverseRoot = glm::mat4(1);
    int idle = -1, walk = -1, run = -1;
    float idleTime = 0.0f;

    static glm::vec3 V(const aiVector3D& v) { return {v.x,v.y,v.z}; }
    static glm::quat Q(const aiQuaternion& q) { return glm::normalize(glm::quat(q.w,q.x,q.y,q.z)); }
    static glm::mat4 M(const aiMatrix4x4& a)
    {
        return glm::mat4(a.a1,a.b1,a.c1,a.d1, a.a2,a.b2,a.c2,a.d2,
            a.a3,a.b3,a.c3,a.d3, a.a4,a.b4,a.c4,a.d4);
    }
    void ReadNode(const aiNode* source, int parent)
    {
        aiVector3D p,s; aiQuaternion q;
        source->mTransformation.Decompose(s,q,p);
        const int index=static_cast<int>(nodes.size());
        nodeIds[source->mName.C_Str()]=index;
        nodes.push_back({parent,V(p),V(s),Q(q)});
        for(unsigned i=0;i<source->mNumChildren;++i) ReadNode(source->mChildren[i],index);
    }
    static glm::vec3 Sample(const std::vector<VecKey>& keys,float time,const glm::vec3& fallback)
    {
        if(keys.empty()) return fallback;
        auto next=std::upper_bound(keys.begin(),keys.end(),time,[](float t,const VecKey& k){return t<k.time;});
        if(next==keys.begin()) return next->value;
        if(next==keys.end()) return keys.back().value;
        const auto& a=*(next-1); const auto& b=*next;
        return glm::mix(a.value,b.value,glm::clamp((time-a.time)/std::max(0.000001f,b.time-a.time),0.0f,1.0f));
    }
    static glm::quat Sample(const std::vector<QuatKey>& keys,float time,const glm::quat& fallback)
    {
        if(keys.empty()) return fallback;
        auto next=std::upper_bound(keys.begin(),keys.end(),time,[](float t,const QuatKey& k){return t<k.time;});
        if(next==keys.begin()) return next->value;
        if(next==keys.end()) return keys.back().value;
        const auto& a=*(next-1); const auto& b=*next;
        return glm::normalize(glm::slerp(a.value,b.value,glm::clamp(
            (time-a.time)/std::max(0.000001f,b.time-a.time),0.0f,1.0f)));
    }
    Node PoseNode(int node,int clip,float time) const
    {
        Node result=nodes[node];
        if(clip<0) return result;
        const auto& c=clips[clip];
        const auto found=c.tracks.find(node);
        if(found==c.tracks.end()) return result;
        time=std::fmod(std::max(0.0f,time),std::max(0.0001f,c.duration));
        result.p=Sample(found->second.p,time,result.p);
        result.s=Sample(found->second.s,time,result.s);
        result.q=Sample(found->second.q,time,result.q);
        return result;
    }
public:
    explicit SkeletalAnimator(const aiScene* scene)
    {
        ReadNode(scene->mRootNode,-1);
        inverseRoot=glm::inverse(M(scene->mRootNode->mTransformation));
        for(unsigned mi=0;mi<scene->mNumMeshes;++mi)
            for(unsigned bi=0;bi<scene->mMeshes[mi]->mNumBones;++bi)
            {
                const aiBone* bone=scene->mMeshes[mi]->mBones[bi];
                const std::string name=bone->mName.C_Str();
                auto& influence=influenceStats[name];
                influence.first+=bone->mNumWeights;
                for(unsigned w=0;w<bone->mNumWeights;++w)
                    influence.second=std::max(influence.second,bone->mWeights[w].mWeight);
                if(boneIds.count(name)) continue;
                if(bones.size()>=MAX_SKIN_BONES) throw std::runtime_error("Demasiados huesos para el shader.");
                const auto node=nodeIds.find(name);
                if(node==nodeIds.end()) throw std::runtime_error("Hueso sin nodo: "+name);
                boneIds[name]=static_cast<int>(bones.size());
                bones.push_back({node->second,M(bone->mOffsetMatrix)});
            }
        for(unsigned i=0;i<scene->mNumAnimations;++i)
        {
            const aiAnimation* animation=scene->mAnimations[i];
            const double ticks=animation->mTicksPerSecond>0?animation->mTicksPerSecond:30.0;
            Clip clip; clip.name=animation->mName.C_Str(); clip.duration=static_cast<float>(animation->mDuration/ticks);
            for(unsigned j=0;j<animation->mNumChannels;++j)
            {
                const aiNodeAnim* channel=animation->mChannels[j];
                auto node=nodeIds.find(channel->mNodeName.C_Str()); if(node==nodeIds.end()) continue;
                Track t;
                for(unsigned k=0;k<channel->mNumPositionKeys;++k)
                    t.p.push_back({static_cast<float>(channel->mPositionKeys[k].mTime/ticks),V(channel->mPositionKeys[k].mValue)});
                for(unsigned k=0;k<channel->mNumScalingKeys;++k)
                    t.s.push_back({static_cast<float>(channel->mScalingKeys[k].mTime/ticks),V(channel->mScalingKeys[k].mValue)});
                for(unsigned k=0;k<channel->mNumRotationKeys;++k)
                    t.q.push_back({static_cast<float>(channel->mRotationKeys[k].mTime/ticks),Q(channel->mRotationKeys[k].mValue)});
                clip.tracks.emplace(node->second,std::move(t));
            }
            std::string lower=clip.name;
            std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
            const int id=static_cast<int>(clips.size());
            if(lower.find("idle")!=std::string::npos) idle=id;
            if(lower.find("walk")!=std::string::npos) walk=id;
            if(lower.find("run")!=std::string::npos) run=id;
            clips.push_back(std::move(clip));
        }
        if(bones.empty()||idle<0||walk<0||run<0)
            throw std::runtime_error("El FBX necesita huesos y clips Idle, Walk y Run.");
        palette.resize(bones.size(),glm::mat4(1));
        Evaluate(0.0f,0.0f,0.0f);
    }
    std::vector<SkinVertex> Weights(const aiMesh* mesh) const
    {
        std::vector<SkinVertex> result(mesh->mNumVertices);
        for(unsigned b=0;b<mesh->mNumBones;++b)
        {
            const auto* bone=mesh->mBones[b]; const int id=boneIds.at(bone->mName.C_Str());
            for(unsigned w=0;w<bone->mNumWeights;++w)
            {
                auto& vertex=result.at(bone->mWeights[w].mVertexId);
                const float value=bone->mWeights[w].mWeight;
                int slot=0;
                for(int k=1;k<4;++k) if(vertex.weights[k]<vertex.weights[slot]) slot=k;
                if(value>vertex.weights[slot]) {vertex.weights[slot]=value;vertex.joints[slot]=id;}
            }
        }
        for(auto& vertex:result)
        {
            const float total=glm::dot(vertex.weights,glm::vec4(1));
            if(total<=0.000001f) throw std::runtime_error("Vertice del personaje sin pesos de Blender.");
            vertex.weights/=total;
        }
        return result;
    }
    glm::vec3 BindPosition(const glm::vec3& position,const SkinVertex& skin) const
    {
        glm::vec4 p(0);
        for(int i=0;i<4;++i) p+=palette.at(skin.joints[i])*glm::vec4(position,1)*skin.weights[i];
        return glm::vec3(p);
    }
    void Update(float dt,float phase,float gait,float running)
    {
        idleTime+=dt;
        Evaluate(phase,gait,running);
    }
    void Reset() { idleTime=0.0f; Evaluate(0.0f,0.0f,0.0f); }
    void Evaluate(float phase,float gait,float running)
    {
        gait=glm::clamp(gait,0.0f,1.0f);running=glm::clamp(running,0.0f,1.0f);
        const float cycle=phase/6.28318530718f;
        globalPose.resize(nodes.size());
        auto& global=globalPose;
        for(int i=0;i<static_cast<int>(nodes.size());++i)
        {
            const Node a=PoseNode(i,idle,idleTime),b=PoseNode(i,walk,cycle*clips[walk].duration),
                c=PoseNode(i,run,cycle*clips[run].duration);
            const glm::vec3 p=glm::mix(a.p,glm::mix(b.p,c.p,running),gait);
            const glm::vec3 s=glm::mix(a.s,glm::mix(b.s,c.s,running),gait);
            const glm::quat q=glm::normalize(glm::slerp(a.q,glm::slerp(b.q,c.q,running),gait));
            const glm::mat4 local=glm::translate(glm::mat4(1),p)*glm::mat4_cast(q)*glm::scale(glm::mat4(1),s);
            global[i]=(nodes[i].parent<0?glm::mat4(1):global[nodes[i].parent])*local;
        }
        for(size_t i=0;i<bones.size();++i)
            palette[i]=inverseRoot*global[bones[i].node]*bones[i].offset;
    }
    const std::vector<glm::mat4>& Palette() const { return palette; }
    size_t BoneCount() const { return bones.size(); }
    size_t ClipCount() const { return clips.size(); }
    bool HasDeformingJoint(const std::string& name) const
    {
        const auto found=influenceStats.find(name);
        return found!=influenceStats.end()&&found->second.first>=12&&found->second.second>0.75f;
    }
    float JointFlexDegrees(const std::string& upper,const std::string& joint,const std::string& lower) const
    {
        const glm::vec3 a=glm::vec3(globalPose.at(nodeIds.at(upper))[3]);
        const glm::vec3 b=glm::vec3(globalPose.at(nodeIds.at(joint))[3]);
        const glm::vec3 c=glm::vec3(globalPose.at(nodeIds.at(lower))[3]);
        return glm::degrees(std::acos(glm::clamp(glm::dot(glm::normalize(b-a),glm::normalize(c-b)),-1.0f,1.0f)));
    }
};
