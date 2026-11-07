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
#include "TreeVegetation.h"

namespace
{
constexpr int WIDTH = 1280, HEIGHT = 720;
constexpr float ENVIRONMENT_SCALE = 10.0f;
const glm::vec3 SOURCE_SPAWN(-114.0f, 2.005836f, 290.0f);
const glm::vec3 ENVIRONMENT_TRANSLATION = -SOURCE_SPAWN * ENVIRONMENT_SCALE;
Camera freeCamera(glm::vec3(30.0f, 30.0f, -60.0f), glm::vec3(0, 1, 0), 115.0f, -12.0f);
CharacterController player;
FollowCamera followCamera;
enum class CameraMode { Free, ThirdPerson };
CameraMode cameraMode = CameraMode::ThirdPerson;
bool firstMouse = true;
double lastX = 0.0, lastY = 0.0;

void UpdateWindowTitle(GLFWwindow* window)
{
    glfwSetWindowTitle(window, cameraMode == CameraMode::ThirdPerson
        ? "Chignahuapan | Tercera persona | C: camara libre"
        : "Chignahuapan | Camara libre | C: tercera persona");
}
void ResetScene()
{
    cameraMode = CameraMode::ThirdPerson;
    player.SetFrozen(false);
    player.Reset();
    followCamera.Reset();
    followCamera.Update(player, 0.0f, ENVIRONMENT_SCALE, true);
    freeCamera = Camera(glm::vec3(30.0f, 30.0f, -60.0f), glm::vec3(0, 1, 0), 115.0f, -12.0f);
    firstMouse = true;
}
bool SetResourceDirectory()
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
        if (fs::exists(candidate / "Models/EntornoCompleto/EntornoCompleto.obj") &&
            fs::exists(candidate / "Shader/character.vs"))
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
    if (key == GLFW_KEY_C)
    {
        cameraMode = cameraMode == CameraMode::ThirdPerson ? CameraMode::Free : CameraMode::ThirdPerson;
        player.SetFrozen(cameraMode == CameraMode::Free);
        followCamera.Update(player, 0.0f, ENVIRONMENT_SCALE, true);
        firstMouse = true;
        UpdateWindowTitle(window);
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
        else ResetScene();
    }
}
void ApplyMovement(glm::vec2 input, bool sprint, float vertical, float dt)
{
    if (cameraMode == CameraMode::Free)
    {
        // Exclusividad estricta: no se actualiza movimiento, pose ni reloj de la niña.
        freeCamera.SetMovementSpeed(ENVIRONMENT_SCALE * (sprint ? 12.0f : 5.0f));
        if (input.y > 0) freeCamera.ProcessFlyKeyboard(FORWARD, dt);
        if (input.y < 0) freeCamera.ProcessFlyKeyboard(BACKWARD, dt);
        if (input.x < 0) freeCamera.ProcessFlyKeyboard(LEFT, dt);
        if (input.x > 0) freeCamera.ProcessFlyKeyboard(RIGHT, dt);
        if (vertical != 0) freeCamera.ProcessVertical(vertical, dt);
        return;
    }
    player.Update(input, followCamera.Forward(), sprint, dt, ENVIRONMENT_SCALE);
}
void DoMovement(GLFWwindow* window, float dt)
{
    const auto down = [window](int key) {
        return glfwGetWindowAttrib(window, GLFW_FOCUSED) && glfwGetKey(window, key) == GLFW_PRESS;
    };
    const glm::vec2 input(static_cast<float>(down(GLFW_KEY_D) - down(GLFW_KEY_A)),
        static_cast<float>(down(GLFW_KEY_W) - down(GLFW_KEY_S)));
    ApplyMovement(input, down(GLFW_KEY_LEFT_SHIFT),
        static_cast<float>(down(GLFW_KEY_E) - down(GLFW_KEY_Q)), dt);
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
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);
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
void SetLighting(const Shader& shader)
{
    shader.Use();
    glUniform1i(glGetUniformLocation(shader.Program, "texture_diffuse1"), 0);
    glUniform3f(glGetUniformLocation(shader.Program, "lightDirection"), -0.2f, -1.0f, -0.3f);
    glUniform3f(glGetUniformLocation(shader.Program, "ambientLight"), 0.38f, 0.40f, 0.45f);
    glUniform3f(glGetUniformLocation(shader.Program, "diffuseLight"), 0.26f, 0.25f, 0.24f);
}
void SetValidationCamera(const glm::vec3& eye, const glm::vec3& target)
{
    const glm::vec3 direction = glm::normalize(target - eye);
    freeCamera = Camera(eye, glm::vec3(0, 1, 0),
        glm::degrees(std::atan2(direction.z, direction.x)), glm::degrees(std::asin(direction.y)));
    cameraMode = CameraMode::Free;
}
void DrawObject(const Model& object, const Shader& shader, const glm::mat4& model,
    const glm::mat4& view, const glm::mat4& projection, bool character)
{
    shader.Use();
    const glm::mat3 normal = glm::transpose(glm::inverse(glm::mat3(model)));
    glUniformMatrix4fv(glGetUniformLocation(shader.Program, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix3fv(glGetUniformLocation(shader.Program, "normalMatrix"), 1, GL_FALSE, glm::value_ptr(normal));
    glUniformMatrix4fv(glGetUniformLocation(shader.Program, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(shader.Program, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
    if (character)
    {
        const auto& bones = player.Pose();
        glUniformMatrix4fv(glGetUniformLocation(shader.Program, "bones[0]"), static_cast<GLsizei>(bones.size()),
            GL_FALSE, glm::value_ptr(bones[0]));
    }
    object.Draw(shader, projection * view * model, !character);
}
void RunScene(GLFWwindow* window, bool validate, bool characterOnly, const std::string& capture)
{
    if (validate) ValidateControls();
    Shader environmentShader("Shader/modelLoading.vs", "Shader/modelLoading.frag");
    Shader characterShader("Shader/character.vs", "Shader/modelLoading.frag");
    Shader skyboxShader("Shader/SkyBox.vs", "Shader/SkyBox.frag");
    SetLighting(environmentShader);
    SetLighting(characterShader);
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
    player.Configure(girl.GetMinBounds(), girl.GetMaxBounds(), 1.35f * ENVIRONMENT_SCALE);
    ResetScene();
    if (validate) ValidateFrozenCharacter(window);
    GroundSurface ground;
    std::unique_ptr<Model> environment;
    if (!characterOnly)
    {
        glfwSetWindowTitle(window, "Cargando EntornoCompleto.obj...");
        environment = std::make_unique<Model>("Models/EntornoCompleto/EntornoCompleto.obj",
            keepLoading, ModelOptions{ &ground, false });
    }
    Model lamp("Models/Farola/Farola.obj", keepLoading);
    ZocaloLighting plazaLights;
    plazaLights.Build(lamp, environment.get(), ground, SOURCE_SPAWN, ENVIRONMENT_SCALE);
    plazaLights.Upload(environmentShader);
    plazaLights.Upload(characterShader);
    std::cout << "Zocalo: 8 farolas, 8 focos alineados con su cristal." << std::endl;
    std::unique_ptr<TreeVegetation> vegetation;
    if (environment) vegetation = std::make_unique<TreeVegetation>(ground, SOURCE_SPAWN, ENVIRONMENT_SCALE);
    Skybox skybox;
    UpdateWindowTitle(window);
    firstMouse = true;
    if (!validate) glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    skyboxShader.Use();
    glUniform1i(glGetUniformLocation(skyboxShader.Program, "skybox"), 0);
    const GLint skyView = glGetUniformLocation(skyboxShader.Program, "view");
    const GLint skyProjection = glGetUniformLocation(skyboxShader.Program, "projection");
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    CheckOpenGL("carga de modelos, rig y cubemap");
    const float sceneDiagonal = environment
        ? glm::length(environment->GetMaxBounds() - environment->GetMinBounds()) * ENVIRONMENT_SCALE : 1000.0f;
    std::cout << "WASD: personaje | Shift: correr | C: cambiar camara | R: reiniciar\n"
        << "Camara libre: WASD + E/Q (personaje congelado) | Raton: mirar | Rueda: zoom\n"
        << "Tab: cursor | Esc: salir" << std::endl;

    double previous = glfwGetTime();
    int frame = 0;
    while (!glfwWindowShouldClose(window))
    {
        const double now = glfwGetTime();
        const float dt = validate ? 1.0f / 60.0f : std::min(static_cast<float>(now - previous), 0.1f);
        previous = now;
        glfwPollEvents();
        if (validate)
        {
            const glm::vec2 input = frame > 0 && frame < 70 ? glm::vec2(0, 1) : glm::vec2(0);
            if (frame == 90) KeyCallback(window, GLFW_KEY_C, 0, GLFW_PRESS, 0);
            if (frame == 110) KeyCallback(window, GLFW_KEY_C, 0, GLFW_PRESS, 0);
            ApplyMovement(input, frame >= 35 && frame < 65, 0, dt);
        }
        else DoMovement(window, dt);
        if (environment && cameraMode == CameraMode::ThirdPerson)
        {
            const glm::vec3 source = player.position / ENVIRONMENT_SCALE + SOURCE_SPAWN;
            float floor = 0.0f;
            if (ground.Sample(source.x, source.z, source.y, floor))
                player.position.y = glm::mix(player.position.y,
                    (floor - SOURCE_SPAWN.y) * ENVIRONMENT_SCALE, 1.0f - std::exp(-20.0f * dt));
        }
        followCamera.Update(player, dt, ENVIRONMENT_SCALE);
        if (validate && environment)
        {
            if (frame == 130)
            {
                const glm::vec3 center = (plazaLights.instances[0].base + plazaLights.instances[2].base) * 0.5f;
                SetValidationCamera(center + glm::vec3(0, 1450, -20), center);
            }
            if (frame == 140)
            {
                const auto& instance = plazaLights.instances[6];
                SetValidationCamera(instance.base + glm::vec3(30, 25, -65), instance.base + glm::vec3(0, 27, 0));
            }
            if (frame == 150)
            {
                glm::vec3 min, max;
                if (!environment->GetMaterialBounds("Arco_Ofrenda.001", min, max))
                    throw std::runtime_error("Falta el arco del Pasillo.");
                const glm::vec3 center = ((min + max) * 0.5f - SOURCE_SPAWN) * ENVIRONMENT_SCALE;
                SetValidationCamera(center + glm::vec3(160, 25, 160), center);
            }
        }
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        if (width <= 0 || height <= 0) { glfwWaitEventsTimeout(0.05); continue; }
        glViewport(0, 0, width, height);
        glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        const glm::vec3 eye = cameraMode == CameraMode::Free ? freeCamera.GetPosition() : followCamera.position;
        const glm::mat4 view = cameraMode == CameraMode::Free ? freeCamera.GetViewMatrix() : followCamera.View();
        const float fov = cameraMode == CameraMode::Free ? freeCamera.GetZoom() : 45.0f;
        const glm::mat4 projection = glm::perspective(glm::radians(fov), static_cast<float>(width) / height,
            0.02f * ENVIRONMENT_SCALE, std::max(10000.0f, sceneDiagonal * 1.5f + glm::length(eye)));
        if (environment)
        {
            glm::mat4 model = glm::translate(glm::mat4(1), ENVIRONMENT_TRANSLATION);
            model = glm::scale(model, glm::vec3(ENVIRONMENT_SCALE));
            DrawObject(*environment, environmentShader, model, view, projection, false);
            for (const auto& instance : plazaLights.instances)
                DrawObject(lamp, environmentShader, instance.model, view, projection, false);
        }
        if (vegetation) vegetation->Draw(environmentShader, view, projection);
        DrawObject(girl, characterShader, player.ModelMatrix(), view, projection, true);
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_FALSE);
        skyboxShader.Use();
        const glm::mat4 rotation = glm::mat4(glm::mat3(view));
        glUniformMatrix4fv(skyView, 1, GL_FALSE, glm::value_ptr(rotation));
        glUniformMatrix4fv(skyProjection, 1, GL_FALSE, glm::value_ptr(projection));
        skybox.Draw();
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);
        if (validate && (frame == 0 || frame == 45 || frame == 95 || frame == 130 || frame == 140 || frame == 150))
        {
            std::vector<float> depth(static_cast<size_t>(width) * height);
            glReadPixels(0, 0, width, height, GL_DEPTH_COMPONENT, GL_FLOAT, depth.data());
            if (std::none_of(depth.begin(), depth.end(), [](float z) { return z < 1.0f; }))
                throw std::runtime_error("La camara no ve geometria.");
            if (!capture.empty()) SaveFramebuffer(capture + "-" + std::to_string(frame) + ".tga", width, height);
        }
        if (frame == 0 || validate) CheckOpenGL("personaje, animacion, camaras y skybox");
        glfwSwapBuffers(window);
        if (validate && ++frame >= (environment ? 160 : 130))
        {
            std::cout << "VALIDACION OK: animacion suave, luces, ambas camaras y " << frame
                << " frames sin errores OpenGL." << std::endl;
            break;
        }
        if (!validate) ++frame;
    }
}
}

int main(int argc, char** argv)
{
    const bool characterOnly = argc > 1 && std::string(argv[1]) == "--validate-character";
    const bool validate = characterOnly || (argc > 1 && std::string(argv[1]) == "--validate");
    const std::string capture = validate && argc > 2 ? argv[2] : "";
    try
    {
        if (!SetResourceDirectory()) throw std::runtime_error("No se encontraron los recursos del proyecto.");
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
    glfwSwapInterval(validate ? 0 : 1);
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
    try { RunScene(window, validate, characterOnly, capture); }
    catch (const std::exception& error)
    {
        std::cerr << "ERROR: " << error.what() << std::endl;
        result = EXIT_FAILURE;
    }
    glfwDestroyWindow(window);
    glfwTerminate();
    return result;
}
