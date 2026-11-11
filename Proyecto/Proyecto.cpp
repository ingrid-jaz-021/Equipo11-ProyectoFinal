#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#undef STB_IMAGE_IMPLEMENTATION
#include "Shader.h"
#include "Camera.h"
#include "Model.h"
#include "Texture.h"
#include "CharacterController.h"
#include "ZocaloLighting.h"
#include "LagunaSpotLighting.h"
#include "LagunaDancer.h"
#include "LagunaAlmas.h"
#include "GuiaAntorchas.h"
#include "OfrendaLighting.h"
#include "TreeVegetation.h"
#include "ZocaloBackdrop.h"
#include "WaterSurface.h"
#include "LakeFence.h"
#include "CompanionController.h"
#include "BoatNavigation.h"
#include "KioskNpc.h"
#include "NpcDialogue.h"
#include "AxoloteNpc.h"
#include "SolidObstacles.h"
#include "MoonSky.h"

namespace
{
constexpr int WIDTH = 1280, HEIGHT = 720;
constexpr float ENVIRONMENT_SCALE = 10.0f;
const glm::vec3 SOURCE_SPAWN(-114.0f, 2.005836f, 290.0f);
const glm::vec3 ENVIRONMENT_TRANSLATION = -SOURCE_SPAWN * ENVIRONMENT_SCALE;
Camera freeCamera(glm::vec3(30.0f, 30.0f, -60.0f), glm::vec3(0, 1, 0), 115.0f, -12.0f);
CharacterController player;
CompanionController companion;
FollowCamera followCamera;
enum class CameraMode { Free, ThirdPerson };
CameraMode cameraMode = CameraMode::ThirdPerson;
BoatNavigation* activeBoat=nullptr; // Propiedad de RunScene; callbacks solo durante su vida.
AxoloteNpc* activeAxolote=nullptr;
SolidObstacles* activeDoors=nullptr;
bool vsyncEnabled=false;
bool firstMouse = true;
double lastX = 0.0, lastY = 0.0;

void UpdateWindowTitle(GLFWwindow* window)
{
    glfwSetWindowTitle(window, cameraMode == CameraMode::ThirdPerson
        ? (activeBoat&&activeBoat->IsNavigating()?"Chignahuapan | Navegando | E: desembarcar | C: camara libre":"Chignahuapan | Caminando | E: embarcar | C: camara libre")
        : "Chignahuapan | Camara libre | C: tercera persona");
}
void UpdateFollowCamera(float dt,bool snap=false)
{
    if(activeBoat&&activeBoat->IsNavigating())
        followCamera.UpdateAnchor(activeBoat->Position(),activeBoat->Yaw(),player.height,dt,ENVIRONMENT_SCALE,11.5f,snap);
    else followCamera.Update(player,dt,ENVIRONMENT_SCALE,snap);
}
void ResetScene()
{
    if(activeBoat)activeBoat->Reset();
    if(activeAxolote)activeAxolote->ResetInteraction();
    cameraMode = CameraMode::ThirdPerson;
    player.SetFrozen(false);
    player.Reset();
    companion.ResetBeside(player,ENVIRONMENT_SCALE);
    followCamera.Reset();
    UpdateFollowCamera(0,true);
    freeCamera = Camera(glm::vec3(30.0f, 30.0f, -60.0f), glm::vec3(0, 1, 0), 115.0f, -12.0f);
    firstMouse = true;
}
bool SetResourceDirectory(bool needEnvironment)
{
    namespace fs = std::filesystem;
    wchar_t executablePath[32768]{};
    const DWORD length = GetModuleFileNameW(nullptr, executablePath, 32768);
    if (!length || length >= 32768) return false;
    const fs::path executableDirectory = fs::path(executablePath).parent_path();
    const std::vector<fs::path> candidates = {
        fs::current_path(), fs::current_path() / "Proyecto", executableDirectory,
        executableDirectory / "Proyecto", executableDirectory.parent_path() / "Proyecto",
        executableDirectory.parent_path().parent_path() / "Proyecto"
    };
    for (const auto& candidate : candidates)
        if ((!needEnvironment || fs::exists(candidate / "Models/EntornoCompleto/EntornoCompleto.obj")) &&
            fs::exists(candidate / "Shader/character.vs") &&
            fs::exists(candidate / "Models/NinaRig/NinaRig.fbx"))
        {
            fs::current_path(candidate);
            return true;
        }
    return false;
}
void MouseCallback(GLFWwindow* window, double x, double y)
{
    if (glfwGetInputMode(window, GLFW_CURSOR) != GLFW_CURSOR_DISABLED) return;
    if (firstMouse) { lastX = x; lastY = y; firstMouse = false; }
    const float dx = static_cast<float>(x - lastX), dy = static_cast<float>(lastY - y);
    if (cameraMode == CameraMode::Free) freeCamera.ProcessMouseMovement(dx, dy);
    else followCamera.Mouse(dx, dy);
    lastX = x; lastY = y;
}
void KeyCallback(GLFWwindow* window, int key, int, int action, int)
{
    if (action != GLFW_PRESS) return;
    if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(window, GLFW_TRUE);
    if(key==GLFW_KEY_V){vsyncEnabled=!vsyncEnabled;glfwSwapInterval(vsyncEnabled?1:0);std::cout<<"V-Sync "<<(vsyncEnabled?"activado":"desactivado")<<" (V)."<<std::endl;}
    // F: pausa seguimiento del xolo; olfatea, gira y da un paso de carrera.
    if(key==GLFW_KEY_F&&(!activeBoat||!activeBoat->IsNavigating())&&companion.TriggerSpecial())
        std::cout<<"Xolo: olfatear -> giro -> paso de carrera; despues vuelve al lado de la nina."<<std::endl;
    if (key == GLFW_KEY_C)
    {
        cameraMode = cameraMode == CameraMode::ThirdPerson ? CameraMode::Free : CameraMode::ThirdPerson;
        player.SetFrozen(cameraMode == CameraMode::Free||(activeBoat&&activeBoat->IsNavigating()));
        if(activeBoat&&cameraMode==CameraMode::Free)activeBoat->Anchor();
        UpdateFollowCamera(0,true);
        firstMouse = true;
        UpdateWindowTitle(window);
    }
    // G: avanzar los tres mensajes del axolote. Sin autorepeticion ni conflicto con E/F.
    if(key==GLFW_KEY_G&&cameraMode==CameraMode::ThirdPerson&&activeAxolote&&(!activeBoat||!activeBoat->IsNavigating()))
        activeAxolote->Interact(player.position);
    // E en tercera persona: subir/bajar. En camara libre sigue siendo ascenso.
    if(key==GLFW_KEY_E&&cameraMode==CameraMode::ThirdPerson&&activeBoat)
    {
        const auto action=activeBoat->Interact(player,companion);
        if(action==BoatNavigation::Interaction::Boarded)std::cout<<"Navegando: W/S avanzar/retroceder, A/D timon, Shift acelerar, E desembarcar."<<std::endl;
        else if(action==BoatNavigation::Interaction::Disembarked)std::cout<<"Caminando: nina y xolo en tierra firme; bote anclado."<<std::endl;
        else if(action==BoatNavigation::Interaction::NoLanding)std::cout<<"Acercate al muelle o a una orilla para desembarcar."<<std::endl;
        else std::cout<<"Acercate al bote para embarcar con E."<<std::endl;
        UpdateFollowCamera(0,true);UpdateWindowTitle(window);
    }
    if (key == GLFW_KEY_TAB)
    {
        const int mode = glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED
            ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED;
        glfwSetInputMode(window, GLFW_CURSOR, mode);
        firstMouse = true;
    }
    if (key == GLFW_KEY_R)
    {
        if (cameraMode == CameraMode::Free) freeCamera = Camera(glm::vec3(30,30,-60), glm::vec3(0,1,0),115,-12);
        else {ResetScene();UpdateWindowTitle(window);}
    }
}
void ApplyMovement(glm::vec2 input, bool sprint, float vertical, float dt,bool animate=true)
{
    if (cameraMode == CameraMode::Free)
    {
        // Exclusividad estricta: no se actualiza movimiento, pose ni reloj de la niña.
        freeCamera.SetMovementSpeed(ENVIRONMENT_SCALE * (sprint ? MovementSettings::FlyFastSpeed : MovementSettings::FlySpeed));
        if (input.y > 0) freeCamera.ProcessFlyKeyboard(FORWARD, dt);
        if (input.y < 0) freeCamera.ProcessFlyKeyboard(BACKWARD, dt);
        if (input.x < 0) freeCamera.ProcessFlyKeyboard(LEFT, dt);
        if (input.x > 0) freeCamera.ProcessFlyKeyboard(RIGHT, dt);
        if (vertical != 0) freeCamera.ProcessVertical(vertical, dt);
        return;
    }
    if(activeBoat&&activeBoat->IsNavigating())activeBoat->Update(input,sprint,dt);
    else player.Update(input, followCamera.Forward(), sprint, dt, ENVIRONMENT_SCALE,animate);
}
void SimulateFrame(glm::vec2 input,bool sprint,float vertical,float dt,const GroundSurface* ground)
{
    const int steps=cameraMode==CameraMode::Free?1:std::max(1,static_cast<int>(std::ceil(dt*60)));
    const float step=dt/steps;
    for(int i=0;i<steps;++i)
    {
        const auto before=player.position,petBefore=companion.position;
        ApplyMovement(input,sprint,vertical,step,false);
        const bool sailing=activeBoat&&activeBoat->IsNavigating();
        if(sailing)activeBoat->PlacePassengers(player,companion);
        else
        {
            if(activeDoors&&cameraMode==CameraMode::ThirdPerson)activeDoors->Resolve(player,before);
            if(activeBoat&&cameraMode==CameraMode::ThirdPerson)activeBoat->GuardWalking(player,before);
            if(ground&&cameraMode==CameraMode::ThirdPerson)player.ConformToGround(*ground,SOURCE_SPAWN,ENVIRONMENT_SCALE,step);
            companion.UpdateCompanion(player,ground,SOURCE_SPAWN,ENVIRONMENT_SCALE,step,false);
            if(activeDoors)activeDoors->Resolve(companion,petBefore);
            if(activeBoat)activeBoat->GuardWalking(companion,petBefore);
        }
    }
    // Evaluar cada esqueleto UNA vez por frame, no por cada subpaso fisico.
    if(!activeBoat||!activeBoat->IsNavigating()){player.AnimateFrame(dt);companion.AnimateFrame(dt);}
}
void DoMovement(GLFWwindow* window, float dt,const GroundSurface* ground)
{
    const auto down = [window](int key) {
        return glfwGetWindowAttrib(window, GLFW_FOCUSED) && glfwGetKey(window, key) == GLFW_PRESS;
    };
    const glm::vec2 input(static_cast<float>(down(GLFW_KEY_D) - down(GLFW_KEY_A)),
        static_cast<float>(down(GLFW_KEY_W) - down(GLFW_KEY_S)));
    SimulateFrame(input, down(GLFW_KEY_LEFT_SHIFT),
        static_cast<float>(down(GLFW_KEY_E) - down(GLFW_KEY_Q)), dt,ground);
}
void CheckOpenGL(const char* stage)
{
    const GLenum error = glGetError();
    if (error != GL_NO_ERROR)
        throw std::runtime_error(std::string("Error OpenGL en ") + stage + ": " + std::to_string(error));
}
class Skybox
{
public:
    Skybox()
    {
        const std::vector<const GLchar*> faces = {
            "SkyBox/galaxy+X.tga", "SkyBox/galaxy-X.tga",
            "SkyBox/galaxy+Y.tga", "SkyBox/galaxy-Y.tga",
            "SkyBox/galaxy+Z.tga", "SkyBox/galaxy-Z.tga"
        };
        cubemap = TextureLoading::LoadCubemap(faces);
        const GLfloat vertices[] = {
		// Positions
		-1.0f,  1.0f, -1.0f,
		-1.0f, -1.0f, -1.0f,
		1.0f, -1.0f, -1.0f,
		1.0f, -1.0f, -1.0f,
		1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,

		-1.0f, -1.0f,  1.0f,
		-1.0f, -1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f,  1.0f,
		-1.0f, -1.0f,  1.0f,

		1.0f, -1.0f, -1.0f,
		1.0f, -1.0f,  1.0f,
		1.0f,  1.0f,  1.0f,
		1.0f,  1.0f,  1.0f,
		1.0f,  1.0f, -1.0f,
		1.0f, -1.0f, -1.0f,

		-1.0f, -1.0f,  1.0f,
		-1.0f,  1.0f,  1.0f,
		1.0f,  1.0f,  1.0f,
		1.0f,  1.0f,  1.0f,
		1.0f, -1.0f,  1.0f,
		-1.0f, -1.0f,  1.0f,

		-1.0f,  1.0f, -1.0f,
		1.0f,  1.0f, -1.0f,
		1.0f,  1.0f,  1.0f,
		1.0f,  1.0f,  1.0f,
		-1.0f,  1.0f,  1.0f,
		-1.0f,  1.0f, -1.0f,

		-1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f,  1.0f,
		1.0f, -1.0f, -1.0f,
		1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f,  1.0f,
		1.0f, -1.0f,  1.0f
        };
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), nullptr);
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
    ~Skybox()
    {
        glDeleteTextures(1, &cubemap);
        glDeleteBuffers(1, &VBO);
        glDeleteVertexArrays(1, &VAO);
    }
    Skybox(const Skybox&) = delete;
    Skybox& operator=(const Skybox&) = delete;
    void Draw() const
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap);
        RenderState::BindVao(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        RenderMetrics::Draw(36);
        RenderState::BindVao(0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    }
private:
    GLuint VAO = 0, VBO = 0, cubemap = 0;
};

void ValidateControls()
{
    const auto require = [](bool condition, const char* message) {
        if (!condition) throw std::runtime_error(message);
    };
    CharacterController a, b, diagonal;
    for (int i=0;i<30;++i) a.Update({0,1},{0,0,1},false,1.0f/30,ENVIRONMENT_SCALE);
    for (int i=0;i<144;++i) b.Update({0,1},{0,0,1},false,1.0f/144,ENVIRONMENT_SCALE);
    for (int i=0;i<30;++i) diagonal.Update({1,1},{0,0,1},false,1.0f/30,ENVIRONMENT_SCALE);
    require(glm::length(a.position-b.position)<0.002f,"Movimiento dependiente de FPS.");
    require(std::abs(glm::length(a.position)-glm::length(diagonal.position))<0.002f,"Diagonal acelerada.");
    for(int i=0;i<120;++i) a.Update({0,0},{0,0,1},false,1.0f/60,ENVIRONMENT_SCALE);
    require(glm::length(a.velocity)<0.001f&&a.gait<0.001f,"El personaje no frena.");
    b.SetFrozen(true);
    const glm::vec3 position=b.position,velocity=b.velocity;
    const float phase=b.phase,gait=b.gait;
    for(int i=0;i<120;++i) b.Update({1,1},{1,0,0},true,1.0f/60,ENVIRONMENT_SCALE);
    require(glm::length(b.position-position)==0&&glm::length(b.velocity-velocity)==0&&
        b.phase==phase&&b.gait==gait,"El personaje cambia mientras la camara esta libre.");
    const glm::vec2 tile=ZocaloTileSize("Material.006");
    const auto u=ProjectFloorUV({0,0,0},{0,1,0},tile);
    const auto v=ProjectFloorUV({tile.x,0,0},{0,1,0},tile);
    require(std::abs(v.x-u.x-1.0f)<0.00001f,"Escala UV de piso incorrecta.");
    const auto frustum=ExtractFrustum(glm::mat4(1));
    require(BoundsVisible(frustum,{-.5f,-.5f,-.5f},{.5f,.5f,.5f})&&
        BoundsVisible(frustum,{.9f,-.5f,-.5f},{1.2f,.5f,.5f})&&
        !BoundsVisible(frustum,{2,-.5f,-.5f},{3,.5f,.5f})&&
        !TransformedBoundsVisible(frustum,glm::vec3(-.5f),glm::vec3(.5f),glm::translate(glm::mat4(1),glm::vec3(5,0,0))),
        "El frustum descarta objetos visibles o conserva objetos fuera del campo.");
    std::cout<<"Controles OK: FPS, diagonales, bloqueo del personaje y escala UV."<<std::endl;
}
void ValidateFrozenCharacter(GLFWwindow* window)
{
    for(int i=0;i<40;++i) player.Update({0,1},{0,0,1},false,1.0f/60,ENVIRONMENT_SCALE);
    KeyCallback(window,GLFW_KEY_C,0,GLFW_PRESS,0);
    const auto pose=player.Pose();
    const auto position=player.position;
    const float phase=player.phase;
    const auto eye=freeCamera.GetPosition();
    for(int i=0;i<60;++i) ApplyMovement({1,1},true,0.5f,1.0f/60);
    if(glm::length(player.position-position)!=0.0f || player.phase!=phase ||
        pose.size()!=player.Pose().size() || std::memcmp(pose.data(),player.Pose().data(),pose.size()*sizeof(glm::mat4))!=0 ||
        glm::length(freeCamera.GetPosition()-eye)<1.0f)
        throw std::runtime_error("La camara libre altera el personaje o no responde a WASD.");
    KeyCallback(window,GLFW_KEY_C,0,GLFW_PRESS,0);
    ResetScene();
    std::cout<<"Pausa real de huesos OK: C congela pose y posicion; WASD mueve solo la camara."<<std::endl;
}
void ValidateArticulation(SkeletalAnimator& rig)
{
    if(!rig.HasArmSafety())throw std::runtime_error("Los brazos no tienen proteccion anatomica activa.");
    const char* upper[4]={"UpperArm.L","UpperArm.R","Thigh.L","Thigh.R"};
    const char* hinge[4]={"Forearm.L","Forearm.R","Shin.L","Shin.R"};
    const char* lower[4]={"Hand.L","Hand.R","Foot.L","Foot.R"};
    for(int mode=0;mode<2;++mode)
    {
        float minima[4]={180,180,180,180},maxima[4]={0,0,0,0};
        for(int step=0;step<120;++step)
        {
            rig.Evaluate(step*6.28318530718f/120.0f,1.0f,static_cast<float>(mode));
            for(int joint=0;joint<4;++joint)
            {
                if(!rig.HasDeformingJoint(hinge[joint]))
                    throw std::runtime_error(std::string("Articulacion sin pesos suficientes: ")+hinge[joint]);
                const float angle=rig.JointFlexDegrees(upper[joint],hinge[joint],lower[joint]);
                if(!std::isfinite(angle)) throw std::runtime_error("Angulo de articulacion invalido.");
                minima[joint]=std::min(minima[joint],angle);
                maxima[joint]=std::max(maxima[joint],angle);
            }
        }
        for(int joint=0;joint<4;++joint)
        {
            const float required=joint<2?(mode?55.0f:20.0f):(mode?80.0f:45.0f);
            if(maxima[joint]<required||maxima[joint]-minima[joint]<(joint<2?5.0f:25.0f)||maxima[joint]>155.0f)
                throw std::runtime_error(std::string("Flexion insuficiente o excesiva: ")+hinge[joint]+" "+std::to_string(minima[joint])+".."+std::to_string(maxima[joint]));
        }
        std::cout<<(mode?"Carrera":"Marcha")<<" - codo: "<<minima[0]<<".."<<maxima[0]
            <<" grados; rodilla: "<<minima[2]<<".."<<maxima[2]<<" grados."<<std::endl;
    }
    rig.Reset();
    for(int g=0;g<=4;++g)for(int r=0;r<=4;++r)for(int phase=0;phase<180;++phase)
    {
        rig.Update(1.0f/60,phase*6.28318530718f/180.0f,g*.25f,r*.25f);
        const auto up=glm::normalize(rig.JointPosition("Neck")-rig.JointPosition("Chest"));
        for(const char* suffix:{".L",".R"})
        {
            const auto shoulder=rig.JointPosition(std::string("UpperArm")+suffix);
            const auto elbow=rig.JointPosition(std::string("Forearm")+suffix);
            const auto wrist=rig.JointPosition(std::string("Hand")+suffix);
            const float length=glm::length(elbow-shoulder);
            if(glm::dot(elbow-shoulder,up)>-length*.58f+.001f||glm::dot(wrist-shoulder,up)>-length*.50f+.001f)
                throw std::runtime_error("Un brazo supera su hombro durante una mezcla de animaciones.");
        }
    }
    rig.Reset();
    std::cout<<"Brazos OK: 4500 poses/mezclas limitadas; codos y manos por debajo de sus hombros."<<std::endl;
}

void ValidateCompanion(Model& dog)
{
    auto* rig=dog.Animator();
    if(!rig||!rig->HasClip("Special")||rig->BoneCount()<20||std::abs(rig->Duration("Special")-3.8f)>.05f)
        throw std::runtime_error("Esqueleto o secuencia del xolo incompletos.");
    for(const char* joint:{"Neck","Head","FrontLower.L","FrontLower.R","HindLower.L","HindLower.R"})
        if(!rig->HasDeformingJoint(joint))throw std::runtime_error(std::string("Xolo sin pesos en ")+joint);
    rig->Evaluate(0,0,0);
    const auto standingHead=rig->JointPosition("Head"),root=rig->JointPosition("Root");
    rig->Evaluate(0,0,0,"Special",.95f,1);
    if(rig->JointPosition("Head").y>standingHead.y*.7f)
        throw std::runtime_error("El clip importado no baja la cabeza al olfatear.");
    rig->Evaluate(0,0,0,"Special",1.60f,1);
    if(rig->JointTurnDegrees("Head")<rig->JointTurnDegrees("Chest")+8||std::abs(rig->JointTurnDegrees("Root"))>2)
        throw std::runtime_error("El xolo no anticipa el giro con la cabeza.");
    float minLeg=180,maxLeg=0,maxTorsoLag=0;
    for(int i=0;i<90;++i)
    {
        rig->Evaluate(0,0,0,"Special",1.68f+i/90.0f,1);
        const float flex=rig->JointFlexDegrees("FrontUpper.L","FrontLower.L","FrontPaw.L");
        minLeg=std::min(minLeg,flex);maxLeg=std::max(maxLeg,flex);
        maxTorsoLag=std::max(maxTorsoLag,std::abs(std::remainder(rig->JointTurnDegrees("Chest")-rig->JointTurnDegrees("Hips"),360.0f)));
    }
    if(maxLeg-minLeg<8||maxTorsoLag<8)throw std::runtime_error("El giro del xolo sigue siendo un bloque rigido.");
    rig->Evaluate(0,0,0,"Special",2.175f,1);
    auto before=standingHead-root,turned=rig->JointPosition("Head")-root;before.y=turned.y=0;
    if(glm::dot(glm::normalize(before),glm::normalize(turned))>-.65f)
        throw std::runtime_error("La fase de giro no rota sobre su propio eje.");
    rig->Evaluate(0,0,0,"Special",rig->Duration("Special"),1);const auto finalPose=rig->Palette();
    rig->Evaluate(0,0,0,"Special",rig->Duration("Special")+1,1);
    if(std::memcmp(finalPose.data(),rig->Palette().data(),finalPose.size()*sizeof(glm::mat4))!=0)
        throw std::runtime_error("La accion especial vuelve a empezar en lugar de terminar.");
    rig->Reset();
    for(int fps:{30,60,144})
    {
        CharacterController owner;CompanionController test;
        test.AttachCompanion(rig);test.Configure(dog.GetMinBounds(),dog.GetMaxBounds(),MovementSettings::CompanionHeight*ENVIRONMENT_SCALE);
        test.ResetBeside(owner,ENVIRONMENT_SCALE);test.position+=glm::vec3(-20,0,-30);
        for(int frame=0;frame<fps*6;++frame)
        {
            owner.velocity=frame<fps*2?glm::vec3(0,0,MovementSettings::WalkSpeed*ENVIRONMENT_SCALE):glm::vec3(0);
            owner.position+=owner.velocity/static_cast<float>(fps);
            test.UpdateCompanion(owner,nullptr,SOURCE_SPAWN,ENVIRONMENT_SCALE,1.0f/fps);
        }
        if(glm::length(test.position-test.FollowTarget(owner,ENVIRONMENT_SCALE))>.15f*ENVIRONMENT_SCALE)
            throw std::runtime_error("El xolo no mantiene su offset de seguimiento.");
        test.ResetBeside(owner,ENVIRONMENT_SCALE);
        const auto begin=test.position;const auto idle=test.Pose();
        if(!test.TriggerSpecial()||test.TriggerSpecial())throw std::runtime_error("Transicion de accion especial invalida.");
        for(int frame=0;frame<fps;++frame)test.UpdateCompanion(owner,nullptr,SOURCE_SPAWN,ENVIRONMENT_SCALE,1.0f/fps);
        if(glm::length(test.position-begin)>.001f||std::memcmp(idle.data(),test.Pose().data(),idle.size()*sizeof(glm::mat4))==0)
            throw std::runtime_error("Olfatear no pausa el seguimiento o no anima los huesos.");
        int watchdog=0;
        while(test.IsSpecial()&&watchdog++<fps*4)test.UpdateCompanion(owner,nullptr,SOURCE_SPAWN,ENVIRONMENT_SCALE,1.0f/fps);
        const float advance=glm::dot(test.position-begin,owner.Forward())/ENVIRONMENT_SCALE;
        if(test.IsSpecial()||advance<1.4f||advance>1.7f)throw std::runtime_error("La secuencia del xolo no completa el paso hacia delante.");
        for(int frame=0;frame<fps*4;++frame)test.UpdateCompanion(owner,nullptr,SOURCE_SPAWN,ENVIRONMENT_SCALE,1.0f/fps);
        if(glm::length(test.position-test.FollowTarget(owner,ENVIRONMENT_SCALE))>.15f*ENVIRONMENT_SCALE)
            throw std::runtime_error("El xolo no retoma el seguimiento al terminar.");
    }
    rig->Reset();
    std::cout<<"Xolo OK: huesos, seguimiento lateral, olfateo/giro/avance y regreso a 30/60/144 FPS."<<std::endl;
}

void ValidateRaisedGround()
{
    GroundSurface terrain;
    terrain.Add({-10,2,-10},{-10,2,10},{10,2,10});
    terrain.Add({-10,2,-10},{10,2,10},{10,2,-10});
    terrain.Add({2,3.6f,-1},{2,3.6f,1},{4,3.6f,1},true);
    terrain.Add({2,3.6f,-1},{4,3.6f,1},{4,3.6f,-1},true);
    float height=0;
    if(!terrain.SampleCharacter(3,0,2,height)||std::abs(height-3.6f)>0.001f)
        throw std::runtime_error("No sube al pasto desde el nivel del piso.");
    if(!terrain.SampleCharacter(5,0,3.6f,height)||std::abs(height-2.0f)>0.001f)
        throw std::runtime_error("El raycast conserva la altura del pasto al salir.");
    GroundSurface triangle;
    triangle.Add({-10,2,-10},{-10,2,10},{10,2,10});
    triangle.Add({-10,2,-10},{10,2,10},{10,2,-10});
    triangle.Add({2,3.6f,-1},{2,3.6f,1},{4,3.6f,1},true);
    if(!triangle.SampleCharacter(3.8f,-0.8f,2,height)||height>2.01f)
        throw std::runtime_error("Falso positivo de AABB fuera de una jardinera triangular.");
    for(int fps:{30,60,144})
    {
        CharacterController test;
        test.position=glm::vec3(30,0,0);
        test.ConformToGround(terrain,{0,2,0},10.0f,1.0f/fps);
        if(std::abs(test.position.y-16.0f)>0.001f) throw std::runtime_error("El personaje penetra el pasto.");
        test.position.x=50;
        for(int i=0;i<fps;++i) test.ConformToGround(terrain,{0,2,0},10.0f,1.0f/fps);
        if(std::abs(test.position.y)>0.001f) throw std::runtime_error("El personaje no vuelve al adoquin.");
    }
    std::cout<<"Altura OK: entrada/salida de pasto, triangulos y 30/60/144 FPS."<<std::endl;
}
void ValidateRamps()
{
    GroundSurface terrain;
    terrain.Add({-10,2,-3},{-10,2,3},{10,2,3});terrain.Add({-10,2,-3},{10,2,3},{10,2,-3});
    // Rampa y tapa con winding invertido a proposito.
    terrain.Add({2,2,-1},{4,3.5f,1},{2,2,1});terrain.Add({2,2,-1},{4,3.5f,-1},{4,3.5f,1});
    terrain.Add({4,3.5f,-1},{6,3.5f,1},{4,3.5f,1});terrain.Add({4,3.5f,-1},{6,3.5f,-1},{6,3.5f,1});
    float hit=0;
    if(!terrain.SampleCharacter(5,0,2,hit)||std::abs(hit-3.5f)>.001f)
        throw std::runtime_error("No detecta una subida que no sea pasto o una cara invertida.");
    for(int fps:{30,60,144})
    {
        CharacterController test;
        for(int i=0;i<fps*4;++i)
        {
            const float x=(i+1)*2.0f/fps;test.position.x=x*10;
            test.ConformToGround(terrain,{0,2,0},10,1.0f/fps);
            const float expected=x>=2&&x<=6?2+std::min((x-2)*.75f,1.5f):2;
            if(test.position.y<(expected-2)*10-.01f)throw std::runtime_error("Penetra una rampa durante la subida.");
        }
        if(std::abs(test.position.y)>.001f)throw std::runtime_error("No desciende desde la plataforma.");
    }
    std::cout<<"Relieve OK: rampas, tapas invertidas y descensos a 30/60/144 FPS."<<std::endl;
}
void ValidatePasilloGround(const GroundSurface& terrain)
{
    constexpr float x=-114.75f;
    float road=0,curb=0,entry=0;
    if(!terrain.SampleCharacter(x,270,SOURCE_SPAWN.y,road)||
        !terrain.SampleCharacter(x,262,road,curb)||!terrain.SampleCharacter(x,245,road,entry)||
        curb<road+.5f||entry<road+.5f)
        throw std::runtime_error("No detecta la banqueta o el piso elevado de la entrada al Pasillo.");
    for(int fps:{30,60,144})for(int direction:{-1,1})
    {
        CharacterController test;
        const float start=direction<0?270.0f:242.0f,end=direction<0?242.0f:270.0f;
        const float initial=direction<0?road:entry;
        test.position=(glm::vec3(x,initial,start)-SOURCE_SPAWN)*ENVIRONMENT_SCALE;
        float z=start;
        while(std::abs(z-end)>.0001f)
        {
            z=direction<0?std::max(end,z-MovementSettings::RunSpeed/fps):std::min(end,z+MovementSettings::RunSpeed/fps);
            test.position.z=(z-SOURCE_SPAWN.z)*ENVIRONMENT_SCALE;
            test.ConformToGround(terrain,SOURCE_SPAWN,ENVIRONMENT_SCALE,1.0f/fps);
            // En la entrada real el pie debe estar sobre la tapa, no debajo.
            if(z>243&&z<246&&test.position.y<(entry-SOURCE_SPAWN.y)*ENVIRONMENT_SCALE-.01f)
                throw std::runtime_error("La nina atraviesa el piso del Pasillo al caminar.");
        }
        for(int i=0;i<fps;++i)test.ConformToGround(terrain,SOURCE_SPAWN,ENVIRONMENT_SCALE,1.0f/fps);
        const float expected=direction<0?entry:road;
        if(std::abs(test.position.y-(expected-SOURCE_SPAWN.y)*ENVIRONMENT_SCALE)>.01f)
            throw std::runtime_error("Altura final incorrecta al entrar/salir del Pasillo.");
    }
    std::cout<<"Pasillo real OK: calle Y="<<road<<", banqueta Y="<<curb<<", entrada Y="<<entry
        <<"; recorrido en ambos sentidos a 30/60/144 FPS."<<std::endl;
}
void ValidateSceneGrass(const GroundSurface& terrain)
{
    const auto samples=terrain.RaisedSamples(SOURCE_SPAWN.y);
    if(samples.size()<8) throw std::runtime_error("Faltan superficies elevadas de pasto.");
    for(const auto& point:samples)
    {
        float hit=0;
        if(!terrain.SampleCharacter(point.x,point.z,SOURCE_SPAWN.y,hit)||hit<point.y-0.001f)
            throw std::runtime_error("Una jardinera real no admite el raycast desde el piso.");
    }
    std::cout<<"Pasto real OK: "<<samples.size()<<" superficies transitables elevadas."<<std::endl;
}
void SaveFramebuffer(const std::string& path, int width, int height)
{
    std::vector<unsigned char> pixels(static_cast<size_t>(width) * height * 3);
    GLint alignment;
    glGetIntegerv(GL_PACK_ALIGNMENT, &alignment);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_BGR, GL_UNSIGNED_BYTE, pixels.data());
    glPixelStorei(GL_PACK_ALIGNMENT, alignment);
    FILE* file = OpenUtf8File(path.c_str(), L"wb");
    if (!file) throw std::runtime_error("No se pudo guardar la captura: " + path);
    unsigned char header[18]{};
    header[2] = 2; header[12] = width & 255; header[13] = (width >> 8) & 255;
    header[14] = height & 255; header[15] = (height >> 8) & 255; header[16] = 24;
    const bool success = std::fwrite(header, 1, 18, file) == 18 &&
        std::fwrite(pixels.data(), 1, pixels.size(), file) == pixels.size();
    std::fclose(file);
    if (!success) throw std::runtime_error("Captura incompleta.");
}
void SetLighting(const Shader& shader,const MoonSky& moon)
{
    shader.Use();
    glUniform1i(shader.Uniform("texture_diffuse1"), 0);
    glUniform1i(shader.Uniform("torchLightCount"), 0);
    glUniform1i(shader.Uniform("ofrendaLightCount"), 0);
    glUniform4f(shader.Uniform("ofrendaCandleRegion"), 0.0f, 0.0f, 0.0f, 0.0f);
    glUniform3f(shader.Uniform("ofrendaCandleColor"), 0.0f, 0.0f, 0.0f);
    moon.UploadLighting(shader);
}
void SetValidationCamera(const glm::vec3& eye, const glm::vec3& target)
{
    const glm::vec3 direction = glm::normalize(target - eye);
    freeCamera = Camera(eye, glm::vec3(0, 1, 0),
        glm::degrees(std::atan2(direction.z, direction.x)), glm::degrees(std::asin(direction.y)));
    cameraMode = CameraMode::Free;
}
void DrawObject(const Model& object, const Shader& shader, const glm::mat4& model,
    const glm::mat4& view, const glm::mat4& projection, const CharacterController* actor=nullptr)
{
    if(actor)
    {
        const auto extent=glm::vec3(actor->height*2.5f);
        if(!BoundsVisible(ExtractFrustum(projection*view),actor->position-extent,actor->position+extent))
        {++RenderMetrics::culled;return;}
    }
    shader.Use();
    glUniformMatrix4fv(shader.Uniform("view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(shader.Uniform("projection"), 1, GL_FALSE, glm::value_ptr(projection));
    if (actor)
    {
        const auto& bones = actor->Pose();
        glUniformMatrix4fv(shader.Uniform("bones[0]"), static_cast<GLsizei>(bones.size()),
            GL_FALSE, glm::value_ptr(bones[0]));
    }
    object.Draw(shader, projection * view * model, actor==nullptr,model);
}
void RunScene(GLFWwindow* window, bool validate, bool characterOnly, const std::string& capture,bool profiling=false)
{
    if (validate) ValidateControls();
    Shader environmentShader("Shader/modelLoading.vs", "Shader/modelLoading.frag");
    Shader characterShader("Shader/character.vs", "Shader/modelLoading.frag");
    Shader lampShader("Shader/modelLoading_instanced.vs","Shader/modelLoading.frag");
    Shader skyboxShader("Shader/SkyBox.vs", "Shader/SkyBox.frag");
    MoonSky moon(ENVIRONMENT_SCALE);
    SetLighting(environmentShader,moon);
    SetLighting(characterShader,moon);
    SetLighting(lampShader,moon);
    if(validate)moon.Validate(environmentShader,characterShader,lampShader);
    const auto keepLoading = [window]() {
        glfwPollEvents();
        glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glfwSwapBuffers(window);
        return glfwWindowShouldClose(window) == GLFW_FALSE;
    };
    // El modelo y los materiales con ñ se leen como UTF-8, incluso con Assimp antiguo.
    glfwSetWindowTitle(window, "Cargando personaje...");
    Model girl("Models/NinaRig/NinaRig.fbx", keepLoading, { nullptr, true });
    player.Attach(girl.Animator());
    player.Configure(girl.GetMinBounds(), girl.GetMaxBounds(), MovementSettings::CharacterHeight * ENVIRONMENT_SCALE);
    ResetScene();
    if (validate) { ValidateArticulation(*girl.Animator()); ValidateFrozenCharacter(window); ValidateRaisedGround(); ValidateRamps(); }
    glfwSetWindowTitle(window,"Cargando xoloitzcuintle...");
    Model dog("Models/XoloRig/XoloRig.fbx",keepLoading,{nullptr,true});
    if(validate)ValidateCompanion(dog);
    companion.AttachCompanion(dog.Animator());
    companion.Configure(dog.GetMinBounds(),dog.GetMaxBounds(),MovementSettings::CompanionHeight*ENVIRONMENT_SCALE);
    companion.ResetBeside(player,ENVIRONMENT_SCALE);
    Model npcModel("Models/KioskNpcRig/KioskNpcRig.fbx",keepLoading,{nullptr,true,{},false});
    Model axoloteModel("Models/AxoloteEntrada/AxoloteEntrada.fbx",keepLoading,{nullptr,true,{},false});
    Model breadModel("Models/Pan de muerto/PanMuerto.obj",keepLoading);
    Model basketModel("Models/canasta.obj",keepLoading);
    Model boatModel("Models/barquito.obj",keepLoading);
    Model almaModel("Models/EntornoCompleto/Laguna/Almas/alma.obj",keepLoading);
    std::unique_ptr<Model> lagunaDancerModel;
    LagunaDancer lagunaDancer;
    LagunaAlmas lagunaAlmas;
    std::unique_ptr<Model> hombreCuerpoModel;
    std::unique_ptr<Model> hombreBrazoDerModel;
    std::unique_ptr<Model> hombreBrazoIzqModel;
    std::unique_ptr<Model> mujerCuerpoModel;
    std::unique_ptr<Model> mujerBrazoDerModel;
    std::unique_ptr<Model> mujerBrazoIzqModel;
    GuiaAntorchas guiaAntorchas;
    glfwSetWindowTitle(window, "Cargando rig del Xolotl...");
    lagunaDancerModel = std::make_unique<Model>("Models/EntornoCompleto/Laguna/Xolotl/XolotlDanza.xanim",keepLoading);
    const auto skeletalClips=girl.Animator()->ClipCount()+dog.Animator()->ClipCount()+npcModel.Animator()->ClipCount()+axoloteModel.Animator()->ClipCount();
    std::cout<<"Inventario de animaciones: "<<skeletalClips+3<<" tipos = "<<skeletalClips<<" clips esqueleticos + pan que disminuye + agua animada + flecha oscilante."<<std::endl;
    if(validate&&skeletalClips!=14)throw std::runtime_error("El inventario de clips activos cambio; actualizar ANIMACIONES.md.");
    if(validate&&characterOnly)
    {
        GroundSurface testGround;
        const BoatNavigation::Area testArea={{4.976494f,.514682f,-230.460419f},{439.315247f,.514682f,139.177979f},
            {4.976494f,.82025f,-62.891235f},{73.178017f,2.49138f,5.098602f}};
        const auto a=testArea.dockMin,b=testArea.dockMax;
        testGround.Add({a.x,b.y,a.z},{b.x,b.y,a.z},{b.x,b.y,b.z});
        testGround.Add({a.x,b.y,a.z},{b.x,b.y,b.z},{a.x,b.y,b.z});
        const float entryY=1.77513f;
        testGround.Add({a.x-16,entryY,a.z},{a.x+.22f,entryY,a.z},{a.x+.22f,entryY,b.z});
        testGround.Add({a.x-16,entryY,a.z},{a.x+.22f,entryY,b.z},{a.x-16,entryY,b.z});
        BoatNavigation probe(testArea,testGround,SOURCE_SPAWN,ENVIRONMENT_SCALE,false);
        probe.ValidateDockAccess(player,companion);probe.Validate(player,companion);ResetScene();
    }
    GroundSurface ground;
    std::unique_ptr<Model> environment;
    if (!characterOnly)
    {
        glfwSetWindowTitle(window, "Cargando EntornoCompleto.obj...");
        environment = std::make_unique<Model>("Models/EntornoCompleto/EntornoCompleto.obj",
            keepLoading, ModelOptions{ &ground, false, {"TapeteAserrin1.002","tarima_madera.001","Material.025","Material.026","Material.027"} });
        if (validate) { environment->ValidateLayout(); ValidateSceneGrass(ground); ValidatePasilloGround(ground); }
        lagunaDancer.Configure(*environment, *lagunaDancerModel, SOURCE_SPAWN, ENVIRONMENT_SCALE);
        lagunaAlmas.Configure(*environment, almaModel, SOURCE_SPAWN, ENVIRONMENT_SCALE);
        glfwSetWindowTitle(window, "Cargando guias con antorcha...");
        hombreCuerpoModel = std::make_unique<Model>("Models/GuiaAntorcha/HombreGA_Cuerpo.obj", keepLoading);
        hombreBrazoDerModel = std::make_unique<Model>("Models/GuiaAntorcha/HombreGA_BrazoDer.obj", keepLoading);
        hombreBrazoIzqModel = std::make_unique<Model>("Models/GuiaAntorcha/HombreGA_BrazoIzq.obj", keepLoading);
        mujerCuerpoModel = std::make_unique<Model>("Models/GuiaAntorcha/MujerGA_Cuerpo.obj", keepLoading);
        mujerBrazoDerModel = std::make_unique<Model>("Models/GuiaAntorcha/MujerGA_BrazoDer.obj", keepLoading);
        mujerBrazoIzqModel = std::make_unique<Model>("Models/GuiaAntorcha/MujerGA_BrazoIzq.obj", keepLoading);
        guiaAntorchas.Configure(*environment, *hombreCuerpoModel, *mujerCuerpoModel, SOURCE_SPAWN, ENVIRONMENT_SCALE);
    }
    SolidObstacles doors;
    if(environment){doors.Build(*environment,SOURCE_SPAWN,ENVIRONMENT_SCALE);if(validate)doors.Validate(ground,SOURCE_SPAWN,ENVIRONMENT_SCALE);activeDoors=&doors;}
    AxoloteNpc axolote;axolote.ConfigureNpc(axoloteModel,environment.get(),ground,SOURCE_SPAWN,ENVIRONMENT_SCALE);
    activeAxolote=&axolote;if(validate)axolote.Validate();
    KioskNpc npc;npc.ConfigureNpc(npcModel,breadModel,environment.get(),SOURCE_SPAWN,ENVIRONMENT_SCALE);
    if(validate)npc.Validate();
    NpcDialogue dialogue;
    Model lamp("Models/Farola/Farola.obj", keepLoading,{nullptr,false,{"Material.001"}});
    ZocaloLighting plazaLights;
    plazaLights.Build(lamp, environment.get(), ground, SOURCE_SPAWN, ENVIRONMENT_SCALE);
    if(validate)plazaLights.Validate(lamp,ENVIRONMENT_SCALE);
    plazaLights.Upload(environmentShader);
    plazaLights.Upload(characterShader);
    plazaLights.Upload(lampShader);
    std::vector<glm::mat4> lampModels;lampModels.reserve(plazaLights.instances.size());
    for(const auto& lamp:plazaLights.instances)lampModels.push_back(lamp.model);
    std::cout << "Zocalo: 12 farolas (8 interiores y 4 exteriores), focos alineados con su cristal." << std::endl;
    LagunaSpotLighting lagoonLights;
    OfrendaLighting ofrendaLights;
    if (environment)
    {
        lagoonLights.Build(*environment, SOURCE_SPAWN, ENVIRONMENT_SCALE);
        if (validate) lagoonLights.Validate();
        lagoonLights.Upload(environmentShader);
        lagoonLights.Upload(characterShader);
        lagoonLights.Upload(lampShader);

        ofrendaLights.Build(environment.get(), SOURCE_SPAWN, ENVIRONMENT_SCALE);
        if (validate) ofrendaLights.Validate();
        ofrendaLights.Upload(environmentShader, 0.0f);
        ofrendaLights.Upload(characterShader, 0.0f);
        ofrendaLights.Upload(lampShader, 0.0f);
    }
    std::unique_ptr<AlphaBillboards> billboards;
    std::unique_ptr<TreeVegetation> vegetation;
    std::unique_ptr<ZocaloBackdrop> backdrop;
    std::unique_ptr<WaterSurface> water;
    std::unique_ptr<LakeFence> lakeFence;
    std::unique_ptr<BoatNavigation> navigation;
    if (environment)
    {
        billboards=std::make_unique<AlphaBillboards>();
        vegetation=std::make_unique<TreeVegetation>(*billboards,ground,SOURCE_SPAWN,ENVIRONMENT_SCALE);
        backdrop=std::make_unique<ZocaloBackdrop>(*billboards,*environment,ground,SOURCE_SPAWN,ENVIRONMENT_SCALE);
        if(validate) billboards->ValidateAlpha();
        water=std::make_unique<WaterSurface>(*environment,SOURCE_SPAWN,ENVIRONMENT_SCALE);
        water->SetMoonLighting(moon.ToMoon(),moon.Ambient(),moon.Diffuse());
        lakeFence=std::make_unique<LakeFence>(*billboards,*water,SOURCE_SPAWN,ENVIRONMENT_SCALE);
        navigation=std::make_unique<BoatNavigation>(BoatNavigation::Area{water->SourceMin(),water->SourceMax(),water->DockMin(),water->DockMax()},
            ground,SOURCE_SPAWN,ENVIRONMENT_SCALE);
        activeBoat=navigation.get();
        if(validate){navigation->ValidateDockAccess(player,companion);navigation->Validate(player,companion);ResetScene();}
        if(validate)water->ValidateAnimation();
    }
    Skybox skybox;
    UpdateWindowTitle(window);
    firstMouse = true;
    if (!validate) glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    skyboxShader.Use();
    glUniform1i(skyboxShader.Uniform("skybox"), 0);
    const GLint skyView = skyboxShader.Uniform("view");
    const GLint skyProjection = skyboxShader.Uniform("projection");
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    CheckOpenGL("carga de modelos, rig y cubemap");
    const float sceneDiagonal = environment
        ? glm::length(environment->GetMaxBounds() - environment->GetMinBounds()) * ENVIRONMENT_SCALE : 1000.0f;
    std::cout << "Altura personaje: " << MovementSettings::CharacterHeight << "; velocidad: " << MovementSettings::WalkSpeed
        << "/" << MovementSettings::RunSpeed << "; camara: " << MovementSettings::FlySpeed << "/" << MovementSettings::FlyFastSpeed << std::endl;
    std::cout << "WASD: personaje | Shift: correr | C: cambiar camara | R: reiniciar\n"
        << "Camara libre: WASD + E/Q (personaje congelado) | Raton: mirar | Rueda: zoom\n"
        << "Tab: cursor | V: alternar V-Sync (inicia desactivado) | Esc: salir" << std::endl;
    std::cout<<"F: xolo olfatea, gira y avanza; despues retoma el seguimiento."<<std::endl;
    std::cout<<"E: embarcar/desembarcar cerca del bote, muelle u orilla. Navegando: W/S motor, A/D timon; ambos pasajeros inmoviles."<<std::endl;

    double previous = glfwGetTime();
    FrameProfiler profiler(profiling);
    int frame = 0;
    unsigned dialogueNearSamples=0,dialogueFarSamples=0,axoloteDialogueSamples=0;
    glm::vec3 pausedBoat(0);std::vector<glm::mat4> pausedHuman,pausedDog;
    while (!glfwWindowShouldClose(window))
    {
        const double now = glfwGetTime();
        const float dt = validate ? 1.0f / 60.0f : glm::clamp(static_cast<float>(now-previous),0.0f,.25f);
        previous = now;
        profiler.BeginFrame(frame);
        RenderState::Invalidate();
        glfwPollEvents();
        if (validate)
        {
            glm::vec2 input = frame > 0 && frame < 70 ? glm::vec2(0, 1) : glm::vec2(0);
            if (frame == 90) KeyCallback(window, GLFW_KEY_C, 0, GLFW_PRESS, 0);
            if (frame == 110) KeyCallback(window, GLFW_KEY_C, 0, GLFW_PRESS, 0);
            if(environment&&frame==610)
            {
                ResetScene();player.position=navigation->DockSpot();companion.ResetBeside(player,ENVIRONMENT_SCALE);
                KeyCallback(window,GLFW_KEY_E,0,GLFW_PRESS,0);KeyCallback(window,GLFW_KEY_F,0,GLFW_PRESS,0);
                if(!navigation->IsNavigating()||companion.IsSpecial())throw std::runtime_error("E/F no respetan el estado de navegacion.");
                const auto target=navigation->Position()+glm::vec3(0,15,0);
                SetValidationCamera(target+glm::vec3(60,48,-75),target);
            }
            if(environment&&frame==620)KeyCallback(window,GLFW_KEY_C,0,GLFW_PRESS,0);
            if(environment&&frame>=620&&frame<680)input={frame>=650&&frame<670?.65f:0.0f,1.0f};
            if(environment&&frame==680)
            {
                KeyCallback(window,GLFW_KEY_C,0,GLFW_PRESS,0);
                pausedBoat=navigation->Position();pausedHuman=player.Pose();pausedDog=companion.Pose();
            }
            if(environment&&frame>=680&&frame<690)input={1,1}; // Solo se mueve la fly cam.
            if(environment&&frame==690)
            {
                if(navigation->Position()!=pausedBoat||std::memcmp(pausedHuman.data(),player.Pose().data(),pausedHuman.size()*sizeof(glm::mat4))||
                    std::memcmp(pausedDog.data(),companion.Pose().data(),pausedDog.size()*sizeof(glm::mat4)))
                    throw std::runtime_error("C no congela bote y pasajeros.");
                SetValidationCamera(pausedBoat+glm::vec3(65,50,55),pausedBoat+glm::vec3(0,15,0));
            }
            if(environment&&frame==700)KeyCallback(window,GLFW_KEY_C,0,GLFW_PRESS,0);
            if(environment&&frame>=700&&frame<710)input={0,-1};
            if(environment&&frame==710)
            {
                KeyCallback(window,GLFW_KEY_E,0,GLFW_PRESS,0);
                if(!navigation->IsNavigating())throw std::runtime_error("E desembarca en agua profunda.");
            }
            if(environment&&frame==720)
                SetValidationCamera(navigation->Position()+glm::vec3(60,40,-45),navigation->Position()+glm::vec3(0,13,0));
            if(environment&&frame==740)
            {
                ResetScene();player.position=navigation->DockSpot();companion.ResetBeside(player,ENVIRONMENT_SCALE);
                KeyCallback(window,GLFW_KEY_E,0,GLFW_PRESS,0);
            }
            if(environment&&frame==741)
            {
                KeyCallback(window,GLFW_KEY_E,0,GLFW_PRESS,0);
                if(navigation->IsNavigating()||player.IsFrozen()||companion.IsFrozen())throw std::runtime_error("E no devuelve el control en el muelle.");
            }
            if(environment&&frame==750)
            {
                const auto target=(navigation->Position()+player.position)*.5f+glm::vec3(0,12,0);
                SetValidationCamera(target+glm::vec3(40,55,-95),target);
            }
            SimulateFrame(input,frame>=35&&frame<65,0,dt,environment?&ground:nullptr);
        }
        else DoMovement(window,dt,environment?&ground:nullptr);
        if (lagunaDancerModel) lagunaDancer.Update(*lagunaDancerModel, dt);
        lagunaAlmas.Update(dt);
        UpdateFollowCamera(dt);
        npc.UpdateNpc(dt);
        axolote.UpdateGreeting(dt);
        axolote.UpdateProximity(player.position,!navigation||!navigation->IsNavigating());
        guiaAntorchas.Update(dt, player.position, environment ? &ground : nullptr);
        bool dialogueApproachTest=false;glm::vec3 savedDialoguePosition(0);
        if (validate && environment)
        {
            if (frame == 130)
            {
                const glm::vec3 center = (plazaLights.instances[0].base + plazaLights.instances[2].base) * 0.5f;
                SetValidationCamera(center + glm::vec3(0, 2400, -20), center);
            }
            if (frame == 140)
            {
                const auto& instance = plazaLights.instances[6];
                const float h=MovementSettings::LampHeight*ENVIRONMENT_SCALE;
                SetValidationCamera(instance.base+glm::vec3(h*.75f,h*.8f,-h*1.7f),instance.base+glm::vec3(0,h*.5f,0));
            }
            if(frame==350)
                SetValidationCamera(lagunaDancer.ValidationEye(*lagunaDancerModel),
                    lagunaDancer.ValidationTarget(*lagunaDancerModel));
            if(frame==390 || frame==775)
            {
                if(validate) lagunaAlmas.Validate();
                const size_t row = frame == 390 ? 0 : 1;
                SetValidationCamera(lagunaAlmas.ValidationEye(row), lagunaAlmas.ValidationTarget(row));
            }
            if (frame == 150)
            {
                glm::vec3 min, max;
                if (!environment->GetMaterialBounds("Arco_Ofrenda.001", min, max))
                    throw std::runtime_error("Falta el arco del Pasillo.");
                const glm::vec3 target = (glm::vec3(-115.0f, 3.5f, 242.0f) - SOURCE_SPAWN) * ENVIRONMENT_SCALE;
                const glm::vec3 eye = (glm::vec3(-115.0f, 4.2f, 226.0f) - SOURCE_SPAWN) * ENVIRONMENT_SCALE;
                SetValidationCamera(eye, target);
            }
            if (frame == 155)
            {
                const glm::vec3 target(-464.0f, 18.0f, -985.0f);
                const glm::vec3 eye(-464.0f, 140.0f, -680.0f);
                SetValidationCamera(eye, target);
            }
            if(frame==200)
                SetValidationCamera(backdrop->Center()+glm::vec3(1300,1150,-1300),backdrop->Center());
            if(frame==210||frame==220||frame==230)
            {
                const int side=frame==210?2:frame==220?1:0;
                const glm::vec3 target=backdrop->SideTarget(side);
                const auto center=backdrop->Center();
                const auto direction=glm::normalize(glm::vec3(target.x-center.x,0,target.z-center.z));
                const auto right=glm::cross(direction,glm::vec3(0,1,0));
                SetValidationCamera(center+direction*150.0f+right*60.0f+glm::vec3(0,90,0),target);
            }
            if(frame==240)
            {
                const auto base=vegetation->Site(0);
                SetValidationCamera(base+glm::vec3(0,35,-120),base+glm::vec3(0,35,0));
            }
            if(frame==250||frame==260)
                SetValidationCamera(backdrop->PassageEye(frame==250),backdrop->PassageTarget());
            if(frame==270)
                SetValidationCamera(backdrop->Center()+glm::vec3(0,2600,-300),backdrop->Center()+glm::vec3(0,0,-200));
            if(frame==280||frame==290)
                SetValidationCamera(water->Center()+glm::vec3(200,2300,2600),water->Center());
            if(frame==300)
                SetValidationCamera(water->Center()+glm::vec3(-850,220,450),water->Center()+glm::vec3(0,40,0));
            if(frame==330)
                SetValidationCamera(water->DockCenter()+glm::vec3(-500,350,650),water->DockCenter()+glm::vec3(150,0,0));
            if(frame==305||frame==315)
                SetValidationCamera(lakeFence->EntryEye(frame==315),lakeFence->EntryTarget());
            if(frame==325)SetValidationCamera(lakeFence->RailEye(),lakeFence->RailTarget());
            if(frame==335)
            {
                glm::vec3 lo,hi;environment->GetMaterialBounds("Material.004",lo,hi);
                const glm::vec3 center=((lo+hi)*.5f-SOURCE_SPAWN)*ENVIRONMENT_SCALE;
                SetValidationCamera(center+glm::vec3(150,65,-190),center+glm::vec3(0,10,0));
            }
            if(frame==310||frame==320)
            {
                player.SetFrozen(false);player.Reset();
                const float z=frame==310?245.0f:262.0f;
                float y=0;ground.SampleCharacter(-114.75f,z,SOURCE_SPAWN.y,y);
                player.position=(glm::vec3(-114.75f,y,z)-SOURCE_SPAWN)*ENVIRONMENT_SCALE;
                player.ConformToGround(ground,SOURCE_SPAWN,ENVIRONMENT_SCALE,dt);
                SetValidationCamera(player.position+glm::vec3(50,35,50),player.position+glm::vec3(0,player.height*.5f,0));
            }
        }
        if (validate)
        {
            const int actionFrame=environment?340:140;
            if(frame==actionFrame)
            {
                ResetScene();KeyCallback(window,GLFW_KEY_C,0,GLFW_PRESS,0);
                KeyCallback(window,GLFW_KEY_F,0,GLFW_PRESS,0);
                const auto target=(player.position+companion.position)*.5f+glm::vec3(0,player.height*.40f,0);
                SetValidationCamera(target+glm::vec3(60,20,60),target);
            }
            if (environment && frame == 160)
            {
                const auto samples = ground.RaisedSamples(SOURCE_SPAWN.y);
                player.SetFrozen(false); player.Reset();
                player.position = (glm::vec3(samples.front().x, SOURCE_SPAWN.y, samples.front().z)-SOURCE_SPAWN)*ENVIRONMENT_SCALE;
                player.ConformToGround(ground, SOURCE_SPAWN, ENVIRONMENT_SCALE, dt);
                if (player.position.y < (samples.front().y-SOURCE_SPAWN.y)*ENVIRONMENT_SCALE-0.01f)
                    throw std::runtime_error("Penetracion en el bloque real de pasto.");
                SetValidationCamera(player.position+glm::vec3(70,45,-30), player.position+glm::vec3(0,player.height*0.5f,0));
            }
            if (environment && frame == 170)
            {
                player.position.x = player.position.z = 0.0f;
                for (int i=0;i<90;++i) player.ConformToGround(ground, SOURCE_SPAWN, ENVIRONMENT_SCALE, 1.0f/60);
                if (std::abs(player.position.y)>0.02f) throw std::runtime_error("No baja al adoquin real al salir del pasto.");
                SetValidationCamera(player.position+glm::vec3(70,45,-30), player.position+glm::vec3(0,player.height*0.5f,0));
            }
            const int walkFrame = environment ? 180 : 120, runFrame = environment ? 190 : 130;
            if (frame == walkFrame || frame == runFrame)
            {
                player.SetFrozen(false); player.Reset();
                girl.Animator()->Evaluate((frame == runFrame ? 0.70f : 0.80f)*6.28318530718f,1.0f,frame == runFrame ? 1.0f : 0.0f);
                SetValidationCamera(player.position+glm::vec3(player.height*2.0f,player.height*0.60f,0),
                    player.position+glm::vec3(0,player.height*0.55f,0));
            }
            const int npcStart=environment?780:400;
            if(frame==npcStart||frame==npcStart+5||frame==npcStart+10||frame==npcStart+15||frame==npcStart+17)
            {
                const double sample=frame==npcStart?1.0:frame==npcStart+5?4.4:frame==npcStart+10?7.7:frame==npcStart+15?9.9:11.2;
                npc.ResetCycle();npc.UpdateNpc(sample);SetValidationCamera(npc.ViewEye(),npc.ViewTarget());
                if(frame==npcStart+5||frame==npcStart+10)
                {
                    savedDialoguePosition=player.position;player.position=npc.DialogueApproach(ENVIRONMENT_SCALE);
                    dialogueApproachTest=true;
                }
            }
        }
        const int axoloteStart=environment?805:425;
        const int axoloteSample=frame-axoloteStart;
        const bool captureAxolote=validate&&axoloteSample>=0&&axoloteSample<=25&&axoloteSample%5==0;
        bool restoreAxolotePosition=false;glm::vec3 savedAxolotePosition(0);
        if(captureAxolote)
        {
            savedAxolotePosition=player.position;restoreAxolotePosition=true;axolote.ResetInteraction();
            player.position=axoloteSample==25?axolote.position+glm::vec3(80,0,0):axolote.Approach();
            axolote.UpdateProximity(player.position);
            cameraMode=CameraMode::ThirdPerson;
            for(int press=0;press<axoloteSample/5&&axoloteSample<25;++press)KeyCallback(window,GLFW_KEY_G,0,GLFW_PRESS,0);
            if((axoloteSample<20&&(!axolote.DialogueVisible()||axolote.MessageIndex()!=axoloteSample/5-1))||
                (axoloteSample>=20&&axolote.DialogueVisible()))throw std::runtime_error("G o salida del radio no actualiza el dialogo del axolote.");
            if(axolote.DialogueVisible())++axoloteDialogueSamples;
            axolote.UpdateGreeting(axoloteSample==0?.2:axoloteSample==5?2.1:axoloteSample==10?3.1:axoloteSample==15?4.1:axoloteSample==20?6.0:7.0);
            SetValidationCamera(axolote.ViewEye(),axolote.ViewTarget());
        }
        const int moonFrame=environment?800:420;
        if(validate&&frame==moonFrame)SetValidationCamera(moon.Focus()+glm::vec3(0,20*ENVIRONMENT_SCALE,0),moon.Position());
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        if (width <= 0 || height <= 0) { glfwWaitEventsTimeout(0.05); continue; }
        glViewport(0, 0, width, height);
        profiler.BeginRender();
        glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        const glm::vec3 eye = cameraMode == CameraMode::Free ? freeCamera.GetPosition() : followCamera.position;
        const glm::mat4 view = cameraMode == CameraMode::Free ? freeCamera.GetViewMatrix() : followCamera.View();
        const float fov = cameraMode == CameraMode::Free ? freeCamera.GetZoom() : 45.0f;
        const glm::mat4 projection = glm::perspective(glm::radians(fov), static_cast<float>(width) / height,
            0.02f * ENVIRONMENT_SCALE, std::max(moon.FarPlane(eye),std::max(10000.0f,sceneDiagonal*1.5f+glm::length(eye))));
        if (guiaAntorchas.Configured())
        {
            guiaAntorchas.UploadTorchLights(environmentShader, static_cast<float>(now));
            guiaAntorchas.UploadTorchLights(characterShader, static_cast<float>(now));
            guiaAntorchas.UploadTorchLights(lampShader, static_cast<float>(now));
        }
        if (ofrendaLights.Configured())
        {
            ofrendaLights.Upload(environmentShader, static_cast<float>(now));
            ofrendaLights.Upload(characterShader, static_cast<float>(now));
            ofrendaLights.Upload(lampShader, static_cast<float>(now));
        }
        if (environment)
        {
            glm::mat4 model = glm::translate(glm::mat4(1), ENVIRONMENT_TRANSLATION);
            model = glm::scale(model, glm::vec3(ENVIRONMENT_SCALE));
            DrawObject(*environment, environmentShader, model, view, projection);
            lamp.DrawInstances(lampShader,lampModels,projection*view);
            navigation->DrawShore(environmentShader,view,projection);
            DrawObject(boatModel,environmentShader,navigation->ModelMatrix(),view,projection);
            navigation->DrawMarker(environmentShader,view,projection,now);
        }
        DrawObject(axoloteModel,characterShader,axolote.ModelMatrix(),view,projection,&axolote);
        DrawObject(basketModel,environmentShader,npc.BasketMatrix(),view,projection);
        DrawObject(npcModel,characterShader,npc.WorldMatrix(),view,projection,&npc);
        if(npc.BreadVisible())DrawObject(breadModel,environmentShader,npc.BreadMatrix(),view,projection);
        DrawObject(girl, characterShader, player.ModelMatrix(), view, projection, &player);
        DrawObject(dog,characterShader,companion.ModelMatrix(),view,projection,&companion);
        if (lagunaDancerModel && lagunaDancer.Configured())
            DrawObject(*lagunaDancerModel,environmentShader,lagunaDancer.ModelMatrix(*lagunaDancerModel),view,projection);
        if (lagunaAlmas.Configured())
            for (size_t row = 0; row < LagunaAlmas::RowCount; ++row)
                for (size_t i = 0; i < LagunaAlmas::Count; ++i)
                    DrawObject(almaModel, environmentShader, lagunaAlmas.ModelMatrix(almaModel, i, row), view, projection);
        if (guiaAntorchas.Configured() && hombreCuerpoModel && mujerCuerpoModel)
        {
            // Hombre: Torso base, Brazo Derecho articulado oscilando al costado, Brazo Izquierdo estirado al frente con antorcha
            DrawObject(*hombreCuerpoModel, environmentShader, guiaAntorchas.HombreCuerpoMatrix(), view, projection);
            if (hombreBrazoDerModel) DrawObject(*hombreBrazoDerModel, environmentShader, guiaAntorchas.HombreBrazoDerMatrix(), view, projection);
            if (hombreBrazoIzqModel) DrawObject(*hombreBrazoIzqModel, environmentShader, guiaAntorchas.HombreBrazoIzqMatrix(), view, projection);

            // Mujer: Torso base, Brazo Derecho articulado oscilando al costado, Brazo Izquierdo estirado al frente con antorcha
            DrawObject(*mujerCuerpoModel, environmentShader, guiaAntorchas.MujerCuerpoMatrix(), view, projection);
            if (mujerBrazoDerModel) DrawObject(*mujerBrazoDerModel, environmentShader, guiaAntorchas.MujerBrazoDerMatrix(), view, projection);
            if (mujerBrazoIzqModel) DrawObject(*mujerBrazoIzqModel, environmentShader, guiaAntorchas.MujerBrazoIzqMatrix(), view, projection);
        }
        moon.Draw(view,projection,eye);
        if(water)water->Draw(view,projection,eye,now);
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_FALSE);
        skyboxShader.Use();
        const glm::mat4 rotation = glm::mat4(glm::mat3(view));
        glUniformMatrix4fv(skyView, 1, GL_FALSE, glm::value_ptr(rotation));
        glUniformMatrix4fv(skyProjection, 1, GL_FALSE, glm::value_ptr(projection));
        skybox.Draw();
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);
        // Cielo y opacos primero; planos RGBA al final, ordenados de lejos a cerca.
        if(billboards) billboards->Draw(view,projection);
        const bool showDialogue=NpcDialogue::IsNearby(player.position,npc.position,ENVIRONMENT_SCALE);
        dialogue.Draw(showDialogue&&!axolote.DialogueVisible(),width,height);
        axolote.DrawDialogue(width,height);
        profiler.EndRender();
        const int actionFrame=environment?340:140;
        const bool captureAction=frame==actionFrame||frame==actionFrame+55||frame==actionFrame+96||frame==actionFrame+132||frame==actionFrame+180||frame==actionFrame+255;
        const bool captureBoat=environment&&(frame==610||frame==640||frame==670||frame==690||frame==720||frame==750);
        const int npcStart=environment?780:400;
        const bool captureNpc=frame==npcStart||frame==npcStart+5||frame==npcStart+10||frame==npcStart+15||frame==npcStart+17;
        if(validate&&captureNpc)
        {
            if(showDialogue!=dialogueApproachTest)throw std::runtime_error("El dialogo no aparece/desaparece por proximidad real de la nina.");
            if(showDialogue)++dialogueNearSamples;else ++dialogueFarSamples;
        }
        if (validate && (frame==moonFrame || captureAxolote || captureNpc || captureBoat || captureAction || frame == 0 || frame == 45 || frame == 95 || frame == 120 || frame == 130 || frame == 140 || frame == 150 || frame == 155 || frame == 160 || frame == 170 || frame == 180 || frame == 190 || frame == 200 || frame == 210 || frame == 220 || frame == 230 || frame == 240 || frame == 250 || frame == 260 || frame == 270 || frame == 280 || frame == 290 || frame == 300 || frame == 305 || frame == 310 || frame == 315 || frame == 320 || frame == 325 || frame == 330 || frame == 335 || frame == 350 || frame == 390 || frame == 775))
        {
            std::vector<float> depth(static_cast<size_t>(width) * height);
            glReadPixels(0, 0, width, height, GL_DEPTH_COMPONENT, GL_FLOAT, depth.data());
            if (std::none_of(depth.begin(), depth.end(), [](float z) { return z < 1.0f; }))
                throw std::runtime_error("La camara no ve geometria.");
            if (!capture.empty()) SaveFramebuffer(capture + "-" + std::to_string(frame) + ".tga", width, height);
        }
        if(dialogueApproachTest)player.position=savedDialoguePosition;
        if(restoreAxolotePosition)player.position=savedAxolotePosition;
        if (frame == 0 || validate) CheckOpenGL("personaje, animacion, camaras, dialogo, Luna y skybox");
        glfwSwapBuffers(window);
        if (validate && ++frame >= (environment ? 840 : 460))
        {
            if(companion.IsSpecial()||glm::length(companion.position-companion.FollowTarget(player,ENVIRONMENT_SCALE))>.4f*ENVIRONMENT_SCALE)
                throw std::runtime_error("F no devolvio el xolo a su posicion de seguimiento.");
            if(dialogueNearSamples<2||dialogueFarSamples<2)throw std::runtime_error("No se verificaron entrada y salida del dialogo.");
            if(axoloteDialogueSamples<4)throw std::runtime_error("Faltan los tres paneles del axolote en pantalla.");
            std::cout<<"Axolote visual OK: aviso G, mensajes 1/2/3, cierre y salida del trigger."<<std::endl;
            std::cout<<"Dialogo OK: texto visible al acercarse y oculto al alejarse; UI RGBA sin errores."<<std::endl;
            std::cout << "VALIDACION OK: animacion suave, luces, ambas camaras y " << frame
                << " frames sin errores OpenGL." << std::endl;
            break;
        }
        if (!validate) ++frame;
    }
    profiler.Write((capture.empty()?"obj/validation/profile":capture)+"-performance.json");
    activeBoat=nullptr;activeAxolote=nullptr;activeDoors=nullptr;
}
}

int main(int argc, char** argv)
{
    const bool characterOnly = argc > 1 && std::string(argv[1]) == "--validate-character";
    const bool profiling=argc>1&&std::string(argv[1])=="--profile";
    const bool validate = profiling||characterOnly || (argc > 1 && std::string(argv[1]) == "--validate");
    const std::string capture = validate && argc > 2 ? argv[2] : "";
    try
    {
        if (!SetResourceDirectory(!characterOnly)) throw std::runtime_error("No se encontraron los recursos del proyecto; revisa Models/EntornoCompleto/EntornoCompleto.obj.");
    }
    catch (const std::exception& e) { std::cerr << e.what() << std::endl; return EXIT_FAILURE; }
    if (!glfwInit()) return EXIT_FAILURE;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_DEPTH_BITS, 24);
    if (validate) glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Chignahuapan", nullptr, nullptr);
    if (!window) { glfwTerminate(); return EXIT_FAILURE; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(0); // Sin esperas escalonadas 60->30 FPS cuando un frame supera el refresco.
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK)
    {
        glfwDestroyWindow(window); glfwTerminate(); return EXIT_FAILURE;
    }
    while (glGetError() != GL_NO_ERROR) {}
    glfwSetKeyCallback(window, KeyCallback);
    glfwSetCursorPosCallback(window, MouseCallback);
    glfwSetScrollCallback(window, [](GLFWwindow*, double, double y) {
        if (cameraMode == CameraMode::Free) freeCamera.ProcessMouseScroll(static_cast<float>(y));
        else followCamera.Scroll(static_cast<float>(y));
    });
    int result = EXIT_SUCCESS;
    try { RunScene(window, validate, characterOnly, capture,profiling); }
    catch (const std::exception& error)
    {
        std::cerr << "ERROR: " << error.what() << std::endl;
        result = EXIT_FAILURE;
    }
    glfwDestroyWindow(window);
    glfwTerminate();
    return result;
}
