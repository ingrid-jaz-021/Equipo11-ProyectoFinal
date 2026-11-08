#pragma once
#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
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
    std::vector<glm::mat4> restGlobalPose;
    struct ArmLimit {int upper,forearm,hand,chest,sign;glm::vec3 up,side,forward,elbowOffset,wristOffset,palm;};
    std::vector<ArmLimit> armLimits;
    struct {float back=-30,front=45,inward=18,outward=28,minFlex=6,maxFlex=100,clearance=.55f,handUp=.15f,forearmSide=.10f,handSide=0;} armSettings;
    std::unordered_map<std::string,std::pair<unsigned,float>> influenceStats;
    glm::mat4 inverseRoot = glm::mat4(1);
    int idle = -1, walk = -1, run = -1;
    float idleTime = 0.0f;

    void InitializeArmLimits()
    {
        restGlobalPose.resize(nodes.size());
        for(size_t i=0;i<nodes.size();++i)
        {
            const auto& n=nodes[i];const auto local=glm::translate(glm::mat4(1),n.p)*glm::mat4_cast(n.q)*glm::scale(glm::mat4(1),n.s);
            restGlobalPose[i]=(n.parent<0?glm::mat4(1):restGlobalPose[n.parent])*local;
        }
        if(!nodeIds.count("UpperArm.L")||!nodeIds.count("UpperArm.R"))return;
        std::ifstream input("Models/NinaRig/arm-limits.txt");
        if(!(input>>armSettings.back>>armSettings.front>>armSettings.inward>>armSettings.outward>>armSettings.minFlex>>armSettings.maxFlex>>armSettings.clearance>>armSettings.handUp>>armSettings.forearmSide>>armSettings.handSide))
            throw std::runtime_error("Falta el perfil obligatorio de limites anatomicos de la nina.");
        const int chest=nodeIds.at("Chest"),neck=nodeIds.at("Neck");
        const auto up=glm::normalize(glm::vec3(restGlobalPose[neck][3]-restGlobalPose[chest][3]));
        auto side=glm::vec3(restGlobalPose[nodeIds.at("UpperArm.L")][3]-restGlobalPose[nodeIds.at("UpperArm.R")][3]);
        side=glm::normalize(side-up*glm::dot(side,up));const auto forward=glm::normalize(glm::cross(side,up));
        const auto inverseChest=glm::inverse(glm::mat3(restGlobalPose[chest]));
        for(const char* suffix:{".L",".R"})
        {
            const int upper=nodeIds.at(std::string("UpperArm")+suffix),forearm=nodeIds.at(std::string("Forearm")+suffix),hand=nodeIds.at(std::string("Hand")+suffix);
            const auto palm=glm::normalize(glm::inverse(glm::mat3(restGlobalPose[hand]))*
                glm::vec3(restGlobalPose[hand][3]-restGlobalPose[forearm][3]));
            armLimits.push_back({upper,forearm,hand,chest,suffix[1]=='L'?1:-1,inverseChest*up,inverseChest*side,inverseChest*forward,
                nodes[forearm].p,nodes[hand].p,palm});
        }
    }
    static glm::vec3 LimitElevation(glm::vec3 direction,const glm::vec3& up,float ceiling,const glm::vec3& fallback)
    {
        direction=glm::normalize(direction);const float y=glm::dot(direction,up);
        if(y<=ceiling)return direction;
        auto horizontal=direction-up*y;
        if(glm::length(horizontal)<.00001f)horizontal=fallback;
        return glm::normalize(horizontal)*std::sqrt(std::max(0.0f,1-ceiling*ceiling))+up*ceiling;
    }
    static void Redirect(glm::mat4& pose,glm::vec3 from,glm::vec3 to)
    {
        from=glm::normalize(from);to=glm::normalize(to);const float dot=glm::clamp(glm::dot(from,to),-1.0f,1.0f);
        if(dot>.999999f)return;
        glm::vec3 axis=glm::cross(from,to);glm::quat q;
        if(dot<-.9999f)
        {axis=glm::cross(from,glm::vec3(1,0,0));if(glm::length(axis)<.001f)axis=glm::cross(from,glm::vec3(0,0,1));q=glm::angleAxis(3.14159265359f,glm::normalize(axis));}
        else q=glm::normalize(glm::quat(1+dot,axis.x,axis.y,axis.z));
        const glm::vec3 pivot=glm::vec3(pose[3]);
        pose=glm::translate(glm::mat4(1),pivot)*glm::mat4_cast(q)*glm::translate(glm::mat4(1),-pivot)*pose;
    }
    static glm::vec3 KeepOutside(glm::vec3 direction,glm::vec3 outward,float minimum)
    {
        direction=glm::normalize(direction);const float component=glm::dot(direction,outward);
        if(component>=minimum)return direction;
        return glm::normalize(direction-outward*component)*std::sqrt(1-minimum*minimum)+outward*minimum;
    }
    void ConstrainArm(int node)
    {
        for(const auto& arm:armLimits)
        {
            if(node!=arm.upper&&node!=arm.forearm&&node!=arm.hand)continue;
            const auto chest=glm::mat3(globalPose[arm.chest]);
            const auto up=glm::normalize(chest*arm.up),side=glm::normalize(chest*arm.side),forward=glm::normalize(chest*arm.forward);
            auto& pose=globalPose[node];
            if(node==arm.upper)
            {
                const auto direction=glm::normalize(glm::mat3(pose)*arm.elbowOffset);
                const float pitch=glm::clamp(std::atan2(glm::dot(direction,forward),-glm::dot(direction,up)),glm::radians(armSettings.back),glm::radians(armSettings.front));
                const float lateral=glm::clamp(std::asin(glm::clamp(arm.sign*glm::dot(direction,side),-1.0f,1.0f)),glm::radians(armSettings.inward),glm::radians(armSettings.outward));
                const auto limited=(-up*std::cos(pitch)+forward*std::sin(pitch))*std::cos(lateral)+side*(arm.sign*std::sin(lateral));
                Redirect(pose,direction,limited);
            }
            else if(node==arm.forearm)
            {
                const auto shoulder=glm::vec3(globalPose[arm.upper][3]),elbow=glm::vec3(pose[3]);
                const auto upper=glm::normalize(elbow-shoulder),wrist=glm::mat3(pose)*arm.wristOffset;
                auto direction=glm::normalize(wrist);
                const float angle=std::acos(glm::clamp(glm::dot(upper,direction),-1.0f,1.0f));
                const float limited=glm::clamp(angle,glm::radians(armSettings.minFlex),glm::radians(armSettings.maxFlex));
                auto bend=direction-upper*glm::dot(upper,direction);
                if(glm::length(bend)<.00001f)bend=forward-upper*glm::dot(upper,forward);
                direction=upper*std::cos(limited)+glm::normalize(bend)*std::sin(limited);
                const float maxY=glm::clamp((glm::dot(shoulder-elbow,up)-glm::length(elbow-shoulder)*armSettings.clearance)/glm::length(wrist),-.98f,.98f);
                direction=LimitElevation(KeepOutside(direction,side*static_cast<float>(arm.sign),armSettings.forearmSide),up,maxY,forward);
                Redirect(pose,wrist,direction);
            }
            else
            {
                const auto palm=glm::mat3(pose)*arm.palm;
                Redirect(pose,palm,LimitElevation(KeepOutside(palm,side*static_cast<float>(arm.sign),armSettings.handSide),up,armSettings.handUp,forward));
            }
        }
    }

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
    Node PoseNode(int node,int clip,float time,bool loop=true) const
    {
        Node result=nodes[node];
        if(clip<0) return result;
        const auto& c=clips[clip];
        const auto found=c.tracks.find(node);
        if(found==c.tracks.end()) return result;
        time=loop?std::fmod(std::max(0.0f,time),std::max(0.0001f,c.duration)):
            glm::clamp(time,0.0f,c.duration);
        result.p=Sample(found->second.p,time,result.p);
        result.s=Sample(found->second.s,time,result.s);
        result.q=Sample(found->second.q,time,result.q);
        return result;
    }
public:
    explicit SkeletalAnimator(const aiScene* scene,bool requireLocomotion=true)
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
        if(bones.empty()||clips.empty()||(requireLocomotion&&(idle<0||walk<0||run<0)))
            throw std::runtime_error("El FBX necesita huesos y clips Idle, Walk y Run.");
        if(idle<0)idle=0;if(walk<0)walk=idle;if(run<0)run=idle;
        palette.resize(bones.size(),glm::mat4(1));
        InitializeArmLimits();
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
    float Duration(const std::string& name) const{return clips.at(FindClip(name)).duration;}
    bool HasClip(const std::string& name) const{return FindClip(name,false)>=0;}
    void UpdateLayered(float dt,float phase,float gait,float running,const std::string& clip,float time,float weight)
    {idleTime+=dt;Evaluate(phase,gait,running,clip,time,weight);}
    void Evaluate(float phase,float gait,float running,const std::string& overlay="",float overlayTime=0,float overlayWeight=0)
    {
        gait=glm::clamp(gait,0.0f,1.0f);running=glm::clamp(running,0.0f,1.0f);
        overlayWeight=glm::clamp(overlayWeight,0.0f,1.0f);
        const int overlayId=overlay.empty()?-1:FindClip(overlay);
        const float cycle=phase/6.28318530718f;
        globalPose.resize(nodes.size());
        auto& global=globalPose;
        for(int i=0;i<static_cast<int>(nodes.size());++i)
        {
            const Node a=PoseNode(i,idle,idleTime),b=PoseNode(i,walk,cycle*clips[walk].duration),
                c=PoseNode(i,run,cycle*clips[run].duration);
            glm::vec3 p=glm::mix(a.p,glm::mix(b.p,c.p,running),gait);
            glm::vec3 s=glm::mix(a.s,glm::mix(b.s,c.s,running),gait);
            glm::quat q=glm::normalize(glm::slerp(a.q,glm::slerp(b.q,c.q,running),gait));
            if(overlayId>=0)
            {
                const Node action=PoseNode(i,overlayId,overlayTime,false);
                p=glm::mix(p,action.p,overlayWeight);s=glm::mix(s,action.s,overlayWeight);
                q=glm::normalize(glm::slerp(q,action.q,overlayWeight));
            }
            const glm::mat4 local=glm::translate(glm::mat4(1),p)*glm::mat4_cast(q)*glm::scale(glm::mat4(1),s);
            global[i]=(nodes[i].parent<0?glm::mat4(1):global[nodes[i].parent])*local;
            // Ultima barrera, DESPUES de mezclar clips: tambien limita poses
            // interpoladas, overlays y cualquier accion futura del personaje.
            ConstrainArm(i);
        }
        for(size_t i=0;i<bones.size();++i)
            palette[i]=inverseRoot*global[bones[i].node]*bones[i].offset;
    }
    const std::vector<glm::mat4>& Palette() const { return palette; }
    int ClipIndex(const std::string& name) const{return FindClip(name);}
    void PlayClip(int clip,float time,bool loop=false)
    {
        if(clip<0||clip>=static_cast<int>(clips.size()))throw std::runtime_error("Indice de clip invalido.");
        for(size_t i=0;i<nodes.size();++i)
        {
            const auto pose=PoseNode(static_cast<int>(i),clip,time,loop);
            const auto local=glm::translate(glm::mat4(1),pose.p)*glm::mat4_cast(pose.q)*glm::scale(glm::mat4(1),pose.s);
            globalPose[i]=(nodes[i].parent<0?glm::mat4(1):globalPose[nodes[i].parent])*local;
            ConstrainArm(static_cast<int>(i));
        }
        for(size_t i=0;i<bones.size();++i)palette[i]=inverseRoot*globalPose[bones[i].node]*bones[i].offset;
    }
    glm::mat4 JointMatrix(const std::string& name) const{return inverseRoot*globalPose.at(nodeIds.at(name));}
    glm::vec3 JointPosition(const std::string& name) const
    {return glm::vec3(inverseRoot*globalPose.at(nodeIds.at(name))*glm::vec4(0,0,0,1));}
    bool HasArmSafety() const{return armLimits.size()==2;}
    float JointTurnDegrees(const std::string& name) const
    {
        const int id=nodeIds.at(name);
        const auto current=glm::mat3(inverseRoot*globalPose[id]),rest=glm::mat3(inverseRoot*restGlobalPose[id]);
        const auto forward=glm::normalize(current*glm::inverse(rest)*glm::vec3(0,0,1));
        return glm::degrees(std::atan2(forward.x,forward.z));
    }
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
private:
    int FindClip(std::string name,bool required=true) const
    {
        std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
        for(size_t i=0;i<clips.size();++i)
        {
            std::string candidate=clips[i].name;
            std::transform(candidate.begin(),candidate.end(),candidate.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
            if(candidate.find(name)!=std::string::npos)return static_cast<int>(i);
        }
        if(required)throw std::runtime_error("Falta el clip de animacion: "+name);
        return -1;
    }
};
