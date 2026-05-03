#include "include/glad/glad.h"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/constants.hpp>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <cmath>
#include <limits>
#include <fstream>
#include <string>
#include <filesystem>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <array>
#include <random>
#include <cstring>
#include <ctime>
#include <cstdint>
#include <cstdio>
#include <future>
#include <regex>
#include <queue>
#include <SFML/Audio.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "include/nlohmann/json.hpp"
#include "FastNoiseLite.h"

using json = nlohmann::json;
namespace fs = std::filesystem;

// ----------------------------------------------------------------------
// Глобальные объекты шума
// ----------------------------------------------------------------------
FastNoiseLite continentNoise;
FastNoiseLite erosionNoise;
FastNoiseLite mountainNoise;
FastNoiseLite riverNoise;
FastNoiseLite biomeTempNoise;
FastNoiseLite biomeHumidNoise;
FastNoiseLite treeNoise;
FastNoiseLite detailNoise;
FastNoiseLite seaLevelNoise;
FastNoiseLite transitionNoise;

// ----------------------------------------------------------------------
// Окно и камера
// ----------------------------------------------------------------------
const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;
const int BLOCK_UNKNOWN = -1;

glm::vec3 cameraPos   = glm::vec3(0.0f, 200.0f,  0.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp    = glm::vec3(0.0f, 1.0f,  0.0f);

float yaw   = -90.0f;
float pitch =  0.0f;
double lastX = SCR_WIDTH / 2.0;
double lastY = SCR_HEIGHT / 2.0;
bool firstMouse = true;
float deltaTime = 0.0f;
float lastFrame = 0.0f;


enum class CameraMode { FirstPerson, ThirdPersonBack };
CameraMode cameraMode = CameraMode::FirstPerson;
glm::vec3 gameplayRayOrigin = cameraPos;
glm::vec3 gameplayRayDir = cameraFront;
glm::vec3 renderCameraPos = cameraPos;
float thirdPersonDistance = 3.2f;

unsigned int playerTexHead = 0, playerTexBody = 0, playerTexArmL = 0, playerTexArmR = 0, playerTexLegL = 0, playerTexLegR = 0;
unsigned int playerVAO = 0, playerVBO = 0;

// ----------------------------------------------------------------------
// Параметры чанков
// ----------------------------------------------------------------------
const int CHUNK_SIZE_X = 17;
const int CHUNK_SIZE_Z = 17;
const int CHUNK_SIZE_Y = 128;
const int CHUNK_VOLUME = CHUNK_SIZE_X * CHUNK_SIZE_Y * CHUNK_SIZE_Z;

// ----------------------------------------------------------------------
// Физика и коллизии
// ----------------------------------------------------------------------
glm::vec3 playerVelocity = glm::vec3(0.0f);
bool isOnGround = false;
const float GRAVITY = -32.0f;
const float JUMP_POWER = 8.94f;
const float PLAYER_HEIGHT = 1.8f;
const float PLAYER_WIDTH = 0.6f;
const float EYE_HEIGHT = 1.62f;
const float WALK_SPEED = 4.317f;
int playerHealth = 20;
const int MAX_PLAYER_HEALTH = 20;
float fallDistance = 0.0f;
float lastGroundY = 0.0f;
bool wasOnGround = false;
float invulnerabilityTimer = 0.0f;
const float INVULNERABILITY_DURATION = 0.5f; // 0.5 секунды неуязвимости после получения урона
const float MIN_FALL_DAMAGE_HEIGHT = 3.0f; // Минимальная высота для получения урона (3 блока)
const float MAX_FALL_DAMAGE_HEIGHT = 23.0f; // Высота, с которой урон максимален (23 блока)
// ----------------------------------------------------------------------
// Таблица твёрдости блоков и светимости
// ----------------------------------------------------------------------
bool isSolidBlockFast[256] = {false};
int blockLightEmission[256] = {0};
int blockOpacity[256] = {0};
int maxBlockLightRadius = 0;

struct Slider {
    float relX, relY;      // Относительные координаты центра (0-1)
    float relW, relH;      // Относительные размеры
    float absX, absY, absW, absH; // Абсолютные координаты
    const char* label;     // Название параметра
    float minValue;        // Минимальное значение
    float maxValue;        // Максимальное значение
    float* value;          // Указатель на текущее значение
    float step;            // Шаг изменения
    bool isDragging;       // Перетаскивается ли ползунок
    int decimalPlaces;     // Количество знаков после запятой для отображения
};

// Прототипы функций
int getBlockAt(int wx, int wy, int wz);
void setBlockAt(int wx, int wy, int wz, int type);
int getBlockAtForMesh(int wx, int wy, int wz);
void integratePendingChunkData(int maxPerFrame);
void buildChunkMeshesNearCamera(int maxPerFrame);
void rebuildChunkMeshesImmediatelyAround(int cx, int cz, int lx, int lz);
void initReticle();
void evaluateDayNightCycle(float t, glm::vec3& sunDir, float& sunInt, float& amb, glm::vec3& sky);
bool isWorldButtonEnabled(int buttonIndex);
void updateMainMenuOptions(GLFWwindow* window);
void renderMainMenuOptions(int screenW, int screenH);
void handleMainMenuOptionsClick(GLFWwindow* window, int button);
void updateOptionsLayout(int screenW, int screenH);
float calculateFallDamage(float distance);
void applyDamage(int damage);
bool isPlayerInWater();
void scanAmbientSounds();
void playRandomAmbientSound();
void renderSingleBlockModel(int blockType);
void renderItemIconFlat(int itemId, int screenX, int screenY, int size, int screenW, int screenH);
void initCloudLayer();

void initPlayerRenderer();
void renderPlayerModel(const glm::vec3& feetPos, const glm::vec3& lookDir, float currentTime);

void renderGame(int screenW, int screenH, float currentTime);
void processInputInGame(GLFWwindow* window, float deltaTime);

void updatePauseMenu(GLFWwindow* window);
void renderPauseMenu(int screenW, int screenH);
void handlePauseMenuClick(GLFWwindow* window, int button);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
bool areChunksReady();





void renderSingleBlockModel(int blockType) {
    static std::unordered_map<int, unsigned int> singleBlockVAO;
    static std::unordered_map<int, unsigned int> singleBlockVBO;
    static std::unordered_map<int, size_t> singleBlockVertexCount;
    
    if (singleBlockVAO.find(blockType) == singleBlockVAO.end()) {
        std::vector<float> vertices;
        
        // Получаем количество частей текстуры
        int textureParts = 3;
        auto btIt = blockTypes.find(blockType);
        if (btIt != blockTypes.end()) {
            textureParts = btIt->second.textureParts;
        }
        
        float size = 1.0f;
        
        // Функция добавления грани с правильными UV
        auto addFace = [&](const float* faceVerts, int faceIdx) {
            glm::vec3 normal;
            switch(faceIdx) {
                case 0: normal = glm::vec3(-1,0,0); break;
                case 1: normal = glm::vec3(1,0,0); break;
                case 2: normal = glm::vec3(0,-1,0); break;
                case 3: normal = glm::vec3(0,1,0); break;
                case 4: normal = glm::vec3(0,0,1); break;
                case 5: normal = glm::vec3(0,0,-1); break;
            }
            
            float uScale = 1.0f / textureParts;
            float uOff;
            
            if (textureParts == 3) {
                if (faceIdx == 3) uOff = 0.0f;           // Верх
                else if (faceIdx == 2) uOff = 2.0f * uScale; // Низ
                else uOff = uScale;                        // Бок
            }
            else if (textureParts == 4) {
                if (faceIdx == 3) uOff = 0.0f;           // Верх
                else if (faceIdx == 2) uOff = 3.0f * uScale; // Низ
                else if (faceIdx == 0 || faceIdx == 1)    // Лево/право
                    uOff = 1.0f * uScale;                  // Бок1
                else                                        // Перед/зад
                    uOff = 2.0f * uScale;                  // Бок2
            }
            else if (textureParts == 6) {
                switch(faceIdx) {
                    case 3: uOff = 0.0f; break;            // Верх
                    case 4: uOff = 1.0f * uScale; break;   // Перед
                    case 1: uOff = 2.0f * uScale; break;   // Право
                    case 5: uOff = 3.0f * uScale; break;   // Зад
                    case 0: uOff = 4.0f * uScale; break;   // Лево
                    case 2: uOff = 5.0f * uScale; break;   // Низ
                }
            }
            
            float uvs[] = { 0,1, 1,1, 1,0, 0,0 };
            int indices[] = {0,1,2, 0,2,3};
            
            for (int idx : indices) {
                vertices.push_back(faceVerts[idx*3]);
                vertices.push_back(faceVerts[idx*3+1]);
                vertices.push_back(faceVerts[idx*3+2]);
                
                float u = uvs[idx*2];
                float v = uvs[idx*2+1];
                
                if (textureParts == 3 && faceIdx != 2 && faceIdx != 3) {
                    v = 1.0f - v;
                }
                
                vertices.push_back(uOff + u * uScale);
                vertices.push_back(v);
                
                vertices.push_back(normal.x);
                vertices.push_back(normal.y);
                vertices.push_back(normal.z);
                
                vertices.push_back(1.0f); // Яркость
                vertices.push_back(0.0f); // Блочный свет
            }
        };
        
        // LEFT FACE
        float leftFace[] = {
            -size/2, -size/2, -size/2,
            -size/2, -size/2,  size/2,
            -size/2,  size/2,  size/2,
            -size/2,  size/2, -size/2
        };
        addFace(leftFace, 0);
        
        // RIGHT FACE
        float rightFace[] = {
             size/2, -size/2,  size/2,
             size/2, -size/2, -size/2,
             size/2,  size/2, -size/2,
             size/2,  size/2,  size/2
        };
        addFace(rightFace, 1);
        
        // BOTTOM FACE
        float bottomFace[] = {
            -size/2, -size/2, -size/2,
             size/2, -size/2, -size/2,
             size/2, -size/2,  size/2,
            -size/2, -size/2,  size/2
        };
        addFace(bottomFace, 2);
        
        // TOP FACE
        float topFace[] = {
            -size/2,  size/2,  size/2,
             size/2,  size/2,  size/2,
             size/2,  size/2, -size/2,
            -size/2,  size/2, -size/2
        };
        addFace(topFace, 3);
        
        // FRONT FACE
        float frontFace[] = {
            -size/2, -size/2,  size/2,
             size/2, -size/2,  size/2,
             size/2,  size/2,  size/2,
            -size/2,  size/2,  size/2
        };
        addFace(frontFace, 4);
        
        // BACK FACE
        float backFace[] = {
             size/2, -size/2, -size/2,
            -size/2, -size/2, -size/2,
            -size/2,  size/2, -size/2,
             size/2,  size/2, -size/2
        };
        addFace(backFace, 5);
        
        unsigned int vao, vbo;
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
        
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 10*sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 10*sizeof(float), (void*)(3*sizeof(float)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 10*sizeof(float), (void*)(5*sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, 10*sizeof(float), (void*)(8*sizeof(float)));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, 10*sizeof(float), (void*)(9*sizeof(float)));
        glEnableVertexAttribArray(4);
        
        singleBlockVAO[blockType] = vao;
        singleBlockVBO[blockType] = vbo;
        singleBlockVertexCount[blockType] = vertices.size() / 10;
    }
    
    auto it = blockTypes.find(blockType);
    if (it != blockTypes.end()) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, it->second.textureID);
        glBindVertexArray(singleBlockVAO[blockType]);
        glDrawArrays(GL_TRIANGLES, 0, singleBlockVertexCount[blockType]);
    }
}

void renderItemIconFlat(int itemId, int screenX, int screenY, int size, int screenW, int screenH) {
    auto it = itemTypes.find(itemId);
    if (it == itemTypes.end() || it->second.textureID == 0) return;
    drawRectangle(screenX, screenY, static_cast<float>(size), static_cast<float>(size), it->second.textureID, screenW, screenH);
}

void addFaceToVerticesUV(std::vector<float>& verts, 
    glm::vec3 v1, glm::vec3 v2, glm::vec3 v3, glm::vec3 v4,
    glm::vec3 normal, float uOffset, float uScale) {
    
    float uvs[] = {
        0.0f, 1.0f,  // v1
        1.0f, 1.0f,  // v2
        1.0f, 0.0f,  // v3
        0.0f, 0.0f   // v4
    };

    glm::vec3 vertices[] = {v1, v2, v3, v4};
    int indices[] = {0, 1, 2, 0, 2, 3};

    for (int idx : indices) {
        glm::vec3 vert = vertices[idx];
        verts.push_back(vert.x);
        verts.push_back(vert.y);
        verts.push_back(vert.z);

        float u = uvs[idx * 2];
        float v = uvs[idx * 2 + 1];
        // Применяем смещение и масштаб для выбора нужной текстуры
        verts.push_back(uOffset + u * uScale);
        verts.push_back(v);

        verts.push_back(normal.x);
        verts.push_back(normal.y);
        verts.push_back(normal.z);

        verts.push_back(1.0f); // Яркость
        verts.push_back(0.0f); // Блочный свет для GUI-модели
    }
}

void render3DItemInGUI(int blockType, float screenX, float screenY, 
    float width, float height, float currentTime) {
if (blockType == 0 || blockTypes.find(blockType) == blockTypes.end()) 
return;

// Сохраняем текущие настройки OpenGL
GLboolean depthTest = glIsEnabled(GL_DEPTH_TEST);
GLboolean cullFace = glIsEnabled(GL_CULL_FACE);
GLboolean blend = glIsEnabled(GL_BLEND);
GLint viewport[4];
glGetIntegerv(GL_VIEWPORT, viewport);

// Временно меняем параметры для рендеринга в GUI
glEnable(GL_DEPTH_TEST);
glEnable(GL_CULL_FACE);
glDisable(GL_BLEND);

// Устанавливаем viewport на область предмета
glViewport((GLint)screenX, (GLint)screenY, (GLsizei)width, (GLsizei)height);

// Создаём проекцию для предмета (перспективная, но с фиксированным углом)
glm::mat4 proj = glm::perspective(glm::radians(45.0f), width / height, 0.1f, 10.0f);

// Камера: смотрим на предмет с определённого угла
glm::vec3 itemPos = glm::vec3(0.0f, 0.0f, 0.0f);
glm::vec3 cameraPosItem = glm::vec3(1.5f, 1.2f, 2.0f);  // Сбоку-сверху
glm::vec3 cameraTarget = itemPos;
glm::mat4 view = glm::lookAt(cameraPosItem, cameraTarget, glm::vec3(0.0f, 1.0f, 0.0f));

// Анимация вращения (медленно крутится)
float rotationAngle = currentTime * 30.0f;  // 30 градусов в секунду
glm::mat4 model = glm::rotate(glm::mat4(1.0f), glm::radians(rotationAngle), glm::vec3(0.0f, 1.0f, 0.0f));
model = glm::scale(model, glm::vec3(0.8f));  // Немного уменьшаем

glUseProgram(shaderProgram);
glUniformMatrix4fv(u_modelLoc, 1, GL_FALSE, glm::value_ptr(model));
glUniformMatrix4fv(u_viewLoc, 1, GL_FALSE, glm::value_ptr(view));
glUniformMatrix4fv(u_projLoc, 1, GL_FALSE, glm::value_ptr(proj));

// Временно отключаем динамическое освещение для GUI
glUniform1f(u_sunIntensity_location, 0.8f);
glUniform1f(u_ambientBase_location, 0.5f);
glUniform1i(u_isWater_location, 0);

// Рендерим блок как отдельную модель
// Для простоты используем тот же подход, что и для чанков, но для одного блока
renderSingleBlockModel(blockType);

// Восстанавливаем настройки
glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);

if (depthTest) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
if (cullFace) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
if (blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
}



// ----------------------------------------------------------------------
// Реализация функций состояний
// ----------------------------------------------------------------------

void startNewGame() {
    refreshWorldMenuEntries();
    currentWorldFolderName = makeUniqueWorldFolderName();
    currentWorldDisplayName = makeDefaultWorldDisplayName();
    currentWorldSeed = generateWorldSeed();
    selectedWorldFolderName = currentWorldFolderName;

    gameStarted = true;
    movementEnabled = false;
    playerPlaced = false;
    loadingTimer = 0.0f;
    isLoadingGame = false;

    resetPlayerStateForNewWorld();
    fs::create_directories(getCurrentChunksPath());
    saveCurrentWorldMetadata();
    initWorldNoise();
    workerThread = std::thread(workerFunction);
    updateChunksAroundCamera(cameraPos, false);
    initReticle();
    startMusic();
    glfwSetInputMode(glfwGetCurrentContext(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    currentState = GameState::LOADING_GAME;
}

void loadGame() {
    if (worldListState.selectedIndex < 0 || worldListState.selectedIndex >= static_cast<int>(availableWorlds.size())) {
        return;
    }

    const WorldSummary& selectedWorld = availableWorlds[worldListState.selectedIndex];
    if (!loadWorldMetadata(selectedWorld.folderName, true)) {
        return;
    }

    selectedWorldFolderName = selectedWorld.folderName;
    gameStarted = true;
    movementEnabled = false;
    playerPlaced = false;
    loadingTimer = 0.0f;
    isLoadingGame = true;
    initWorldNoise();
    workerThread = std::thread(workerFunction);
    updateChunksAroundCamera(cameraPos, true);
    initReticle();
    startMusic();
    glfwSetInputMode(glfwGetCurrentContext(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    currentState = GameState::LOADING_GAME;
}

void exitToMenu(GLFWwindow* window, int& sw, int& sh) {
    if (gameStarted) {
        saveAllChunks();
    }

    if (ambientSound) {
        delete ambientSound;
        ambientSound = nullptr;
    }
    ambientSoundPlaying = false;
    ambientSoundBuffers.clear();
    gameStarted = false;
    movementEnabled = false;
    playerPlaced = false;
    loadingTimer = 0.0f;
    gamePaused = false;
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    loadedChunks.clear();
    waterChunksCache.clear();
    waterChunksCacheValid = false;
    {
        std::lock_guard<std::mutex> lock(chunkMutex);
        pendingData.clear();
        pendingLoad.clear();
        pendingGen.clear();
    }
    lastChunkForMesh = nullptr;
    lastChunkCoordsForMesh = glm::ivec2(0,0);
    workerRunning = false;
    if (workerThread.joinable()) workerThread.join();
    workerRunning = true;
    glfwGetWindowSize(window, &sw, &sh);
    updateButtonPositions(sw, sh);
    updatePhotoPosition(sw, sh);
    stopMusic();
    playerVelocity = glm::vec3(0.0f);
    isOnGround = false;
    cameraPos = glm::vec3(0.0f, 200.0f, 0.0f);
    yaw = -90.0f;
    pitch = 0.0f;
    cameraFront = glm::vec3(0,0,-1);
    firstMouse = true;
    selectedWorldFolderName.clear();
    currentState = GameState::MAIN_MENU;
    lastChunkForMesh = nullptr;
    lastChunkCoordsForMesh = glm::ivec2(0,0);
    refreshWorldMenuEntries();
}

bool areChunksReady() {
    int ccx = (int)std::floor(cameraPos.x/CHUNK_SIZE_X);
    int ccz = (int)std::floor(cameraPos.z/CHUNK_SIZE_Z);
    for (int dx=-5; dx<=5; ++dx)
        for (int dz=-5; dz<=5; ++dz) {
            auto it = loadedChunks.find({ccx+dx, ccz+dz});
            if (it == loadedChunks.end() || !it->second.data || !it->second.meshReady)
                return false;
        }
    return true;
}

bool loadItemConfig(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return false;

    json data = json::parse(f);
    itemTypes.clear();

    for (auto& item : data["items"]) {
        ItemType it;
        it.id = item["id"];
        it.name = item.value("name", "");
        it.displayName = item.value("displayName", it.name);
        it.maxStack = item.value("maxStack", 64);

        const std::string textureField = item.value("texture", "None");
        it.isBlock = (textureField == "None");

        if (!it.isBlock) {
            std::string texturePath = textureField;
            if (texturePath.rfind("textures/", 0) != 0) {
                texturePath = "textures/items/" + texturePath;
            }
            it.textureID = loadUITexture(texturePath.c_str());
        }

        itemTypes[it.id] = std::move(it);
    }
    return true;
}

void updateMainMenu(GLFWwindow* window) {
    // Ничего не делаем в главном меню (все через коллбэки)
}

void renderMainMenu(int screenW, int screenH) {
    glClearColor(0,0,0,1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (menuBackgroundLightTexture || menuBackgroundTexture)
        drawTiledBackground(menuBackgroundLightTexture != 0 ? menuBackgroundLightTexture : menuBackgroundTexture, screenW, screenH);
    if (menuPhotoTexture)
        drawRectangle(photoAbsX, photoAbsY, photoAbsW, photoAbsH, menuPhotoTexture, screenW, screenH);
    
    for (int i = 0; i < MAIN_MENU_BUTTON_COUNT; ++i) {
        unsigned int tex = menuButtonTexture;
        const bool hovered = isMouseOverButton(buttons[i], mouseX, mouseY);
        if (menuButtonHighlightTexture && hovered) {
            tex = menuButtonHighlightTexture;
        }
        drawRectangle(buttons[i].absX, buttons[i].absY, buttons[i].absW, buttons[i].absH, tex, screenW, screenH);

        // Не рисуем текст для кнопки с текстурой (индекс 5)
        if (i != 5) {
            const float textScale = fitMinecraftButtonTextScale(buttons[i].label, buttons[i].absW, buttons[i].absH);
            drawMinecraftTextCentered(
                buttons[i].label,
                buttons[i].absX + buttons[i].absW * 0.5f,
                buttons[i].absY + buttons[i].absH * 0.52f,
                textScale,
                screenW,
                screenH,
                getMenuTextColor(hovered)
            );
        }
    }
    
    // Кнопка языка с текстурой
    if (languageButtonTexture) {
        const bool langHovered = isMouseOverButton(buttons[5], mouseX, mouseY);
        
        // Рисуем текстуру кнопки (используем highlight текстуру при наведении)
        unsigned int texToDraw = languageButtonTexture;
        if (langHovered && menuButtonHighlightTexture) {
            // Рисуем рамку подсветки СВЕРХУ, а не вместо
            drawRectangle(buttons[5].absX, buttons[5].absY, buttons[5].absW, buttons[5].absH, 
                         menuButtonHighlightTexture, screenW, screenH);
        }
        // Рисуем саму иконку поверх
        drawRectangle(buttons[5].absX, buttons[5].absY, buttons[5].absW, buttons[5].absH, 
                     texToDraw, screenW, screenH);
    }
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}

void updateWorldSelectMenu(GLFWwindow* window) {
    // Экран статичный, логика через клики
}

void updateMood(float deltaTime) {
    static float moodTimer = 0.0f;
    moodTimer += deltaTime;
    
    // Проверяем каждые 15 секунд
    if (moodTimer >= 15.0f) {
        moodTimer = 0.0f;
        
        // Получаем позицию игрока
        int playerX = static_cast<int>(floor(cameraPos.x));
        int playerY = static_cast<int>(floor(cameraPos.y));
        int playerZ = static_cast<int>(floor(cameraPos.z));
        
        // 1. Проверяем освещенность на уровне головы игрока
        uint8_t blockLight = getBlockLightAt(playerX, playerY, playerZ);
        uint8_t skyLight = getSkyLightAt(playerX, playerY, playerZ);
        int totalLight = std::max(blockLight, skyLight);
        
        // 2. Проверяем, есть ли блок НАД игроком (пещера/подземелье)
        bool blockAbove = false;
        // Проверяем блоки над головой (на 1-3 блока выше)
        for (int offset = 1; offset <= 4; ++offset) {
            int blockAboveId = getBlockAt(playerX, playerY + offset, playerZ);
            if (blockAboveId != 0 && blockAboveId != 5) { // Не воздух и не вода
                blockAbove = true;
                break;
            }
        }
        
        // 3. Проверяем глубину (насколько глубоко под землёй)
        // Ищем высоту поверхности (первый непрозрачный блок СВЕРХУ ВНИЗ)
        int surfaceY = CHUNK_SIZE_Y - 1;
        for (int y = CHUNK_SIZE_Y - 1; y >= 0; --y) {
            int blockId = getBlockAt(playerX, y, playerZ);
            if (blockId != 0 && blockId != 5) { // Нашли землю/камень
                surfaceY = y;
                break;
            }
        }
        
        // Глубина: насколько игрок ниже поверхности
        // Если игрок НАД поверхностью, глубина будет отрицательной
        int depth = surfaceY - playerY;
        
        // Для отладки - раскомментируйте, чтобы видеть значения:
        std::cout << "Mood check - Light: " << totalLight 
                  << ", Block above: " << (blockAbove ? "YES" : "NO")
                  << ", SurfaceY: " << surfaceY 
                  << ", PlayerY: " << playerY
                  << ", Depth: " << depth << std::endl;
        
        // === ИСПРАВЛЕННЫЕ УСЛОВИЯ ===
        // Настроение повышается КОГДА:
        // 1. Игрок ПОД землёй (depth > 0) - в пещере
        // 2. Освещённость НИЗКАЯ (<= 7) - темно
        // 3. Есть блок НАД головой - небо не видно
        
        bool inCave = (depth > 5);           // Глубоко под землёй (минимум 5 блоков)
        bool isDark = (totalLight <= 7);      // Темно
        bool isUnderground = blockAbove;      // Есть блок над головой
        
        // Все три условия должны выполняться для ПОВЫШЕНИЯ настроения (страх/напряжение в пещере)
        bool conditionsMet = inCave && isDark && isUnderground;
        
        if (conditionsMet) {
            float oldMood = currentMood;
            currentMood += 10.0f;
            if (currentMood > 100.0f) currentMood = 100.0f;
            
            std::cout << "Mood increased (in cave!): " << oldMood << "% -> " << currentMood << "%" << std::endl;
            
            // Если настроение достигло 100%, проигрываем звук и сбрасываем
            if (currentMood >= 100.0f) {
                playRandomAmbientSound();
                currentMood = 0.0f;
                std::cout << "Mood reached 100%! Playing ambient sound." << std::endl;
            }
        } else {
            // НАОБОРОТ: на поверхности настроение медленно снижается
            if (currentMood > 0.0f) {
                float oldMood = currentMood;
                // На поверхности снижаем быстрее (10% вместо 5%)
                float decreaseAmount = 10.0f;
                currentMood -= decreaseAmount;
                if (currentMood < 0.0f) currentMood = 0.0f;
                if (oldMood != currentMood) {
                    std::cout << "Mood decreased (on surface): " << oldMood << "% -> " << currentMood << "%" << std::endl;
                }
            }
        }
    }
    
    // Обновляем состояние звука (если звук закончился)
    if (ambientSoundPlaying && ambientSound && ambientSound->getStatus() == sf::Sound::Status::Stopped) {
        ambientSoundPlaying = false;
        std::cout << "Ambient sound finished playing" << std::endl;
    }
}

void playRandomAmbientSound() {
    if (ambientSoundBuffers.empty()) {
        std::cout << "No ambient sounds available!" << std::endl;
        return;
    }
    
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, static_cast<int>(ambientSoundBuffers.size()) - 1);
    int index = dis(gen);
    
    // Удаляем старый звук, если он существует
    if (ambientSound) {
        delete ambientSound;
        ambientSound = nullptr;
    }
    
    // Создаём новый звук с буфером
    ambientSound = new sf::Sound(ambientSoundBuffers[index]);
    ambientSound->setVolume(70.0f); // Громкость ambient звуков (0-100)
    ambientSound->play();
    ambientSoundPlaying = true;
    
    std::cout << "Playing ambient sound " << index << " (Mood reached 100%)" << std::endl;
}

void renderMainMenuOptions(int screenW, int screenH) {
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Тот же фон, что и в главном меню
    if (menuBackgroundLightTexture || menuBackgroundTexture)
        drawTiledBackground(menuBackgroundLightTexture != 0 ? menuBackgroundLightTexture : menuBackgroundTexture, screenW, screenH);
    drawMinecraftTextCentered("Options", screenW * 0.5f, screenH * 0.08f, 3.12f, screenW, screenH, glm::vec4(1.0f));
    for (const auto& slider : optionsSliders) {
        drawSlider(slider, screenW, screenH);
    }

    auto drawOptButton = [&](const Button& btn) {
        const bool hovered = isMouseOverButton(btn, mouseX, mouseY);
        unsigned int tex = (menuButtonHighlightTexture && hovered) ? menuButtonHighlightTexture : menuButtonTexture;
        drawRectangle(btn.absX, btn.absY, btn.absW, btn.absH, tex, screenW, screenH);
        drawMinecraftTextCentered(btn.label, btn.absX + btn.absW * 0.5f, btn.absY + btn.absH * 0.52f,
            fitMinecraftButtonTextScale(btn.label, btn.absW, btn.absH), screenW, screenH, getMenuTextColor(hovered));
    };
    drawOptButton(optionsDifficultyButton);
    for (int i = 0; i < OPTIONS_ROW1_BUTTON_COUNT; ++i) drawOptButton(optionsRow1Buttons[i]);
    for (int i = 0; i < OPTIONS_ROW2_BUTTON_COUNT; ++i) drawOptButton(optionsRow2Buttons[i]);
    drawOptButton(optionsDoneButton);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}

void renderWorldSelectMenu(int screenW, int screenH) {
    glClearColor(0,0,0,1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const unsigned int darkTexture = menuBackgroundDarkTexture != 0 ? menuBackgroundDarkTexture : menuBackgroundTexture;
    const unsigned int lightTexture = menuBackgroundLightTexture != 0 ? menuBackgroundLightTexture : menuBackgroundTexture;

    const float topStripHeight = 120.0f;
    const float bottomStripHeight = 192.0f;
    const float listTop = topStripHeight;
    const float listBottom = screenH - bottomStripHeight;
    const float listHeight = std::max(64.0f, listBottom - listTop);
    worldListState.x = screenW * 0.275f;
    worldListState.y = listTop + 12.0f;
    worldListState.w = screenW * 0.45f;
    worldListState.h = listHeight - 8.0f;
    worldListState.rowHeight = 138.0f;
    {
        const float contentHeight = worldMenuEntries.size() * worldListState.rowHeight;
        const float verticalOffset = contentHeight < worldListState.h ? (worldListState.h - contentHeight) * 0.5f : 0.0f;
        worldListState.hoveredIndex = isInsideRect(static_cast<float>(mouseX), static_cast<float>(mouseY), worldListState.x, worldListState.y, worldListState.w, worldListState.h)
            ? static_cast<int>((static_cast<float>(mouseY) - worldListState.y - verticalOffset + worldListState.scroll) / worldListState.rowHeight)
            : -1;
    }
    if (worldListState.hoveredIndex < 0 || worldListState.hoveredIndex >= static_cast<int>(worldMenuEntries.size())) worldListState.hoveredIndex = -1;

    // 1. СНАЧАЛА рисуем тёмный фон (весь экран)
    if (darkTexture) {
        drawTiledBackground(darkTexture, screenW, screenH);
    }
    
    // 2. ПОТОМ рисуем список миров (поверх тёмного фона)
    
    renderMenuList(worldListState, worldMenuEntries, screenW, screenH);

    // 3. ЗАТЕМ рисуем светлые полосы сверху и снизу (поверх списка)
    if (lightTexture) {
        drawTiledBackground(lightTexture, screenW, screenH, 0.0f, 0.0f, static_cast<float>(screenW), topStripHeight);
        drawTiledBackground(lightTexture, screenW, screenH, 0.0f, screenH - bottomStripHeight, static_cast<float>(screenW), bottomStripHeight);
    }
    drawMinecraftTextCentered(tr("Select World", "Выбор мира", "ワールド選択"), screenW * 0.5f, 60.0f, 3.0f, screenW, screenH, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    // 4. НАКОНЕЦ рисуем кнопки (поверх светлых полос)
    for (int i = 0; i < WORLD_SELECT_BUTTON_COUNT; ++i) {
        bool enabled = isWorldButtonEnabled(i);
        unsigned int tex = menuButtonTexture;
        const bool hovered = isMouseOverButton(worldButtons[i], mouseX, mouseY);
        
        // Логика выбора текстуры
        if (!enabled) {
            tex = menuButtonDisabledTexture != 0 ? menuButtonDisabledTexture : menuButtonTexture;
        } else if (menuButtonHighlightTexture && hovered) {
            tex = menuButtonHighlightTexture;
        }
        
        drawRectangle(worldButtons[i].absX, worldButtons[i].absY, worldButtons[i].absW, worldButtons[i].absH, tex, screenW, screenH);
        
        // Цвет текста
        glm::vec4 textColor;
        if (!enabled) {
            textColor = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
        } else {
            textColor = getMenuTextColor(hovered);
        }
        
        drawMinecraftTextCentered(
            worldButtons[i].label,
            worldButtons[i].absX + worldButtons[i].absW * 0.5f,
            worldButtons[i].absY + worldButtons[i].absH * 0.52f,
            fitMinecraftButtonTextScale(worldButtons[i].label, worldButtons[i].absW, worldButtons[i].absH),
            screenW,
            screenH,
            textColor
        );
    }

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}

void renderLanguageMenu(int screenW, int screenH) {
    glClearColor(0,0,0,1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const unsigned int darkTexture = menuBackgroundDarkTexture != 0 ? menuBackgroundDarkTexture : menuBackgroundTexture;
    const unsigned int lightTexture = menuBackgroundLightTexture != 0 ? menuBackgroundLightTexture : menuBackgroundTexture;
    const float topStripHeight = 120.0f;
    const float bottomStripHeight = 192.0f;
    const float listTop = topStripHeight;
    const float listBottom = screenH - bottomStripHeight;
    languageListState.x = screenW * 0.29f;
    languageListState.y = listTop + 12.0f;
    languageListState.w = screenW * 0.42f;
    languageListState.h = std::max(64.0f, listBottom - listTop - 8.0f);
    languageListState.rowHeight = 78.0f;
    {
        const float contentHeight = languageMenuEntries.size() * languageListState.rowHeight;
        const float verticalOffset = contentHeight < languageListState.h ? (languageListState.h - contentHeight) * 0.5f : 0.0f;
        languageListState.hoveredIndex = isInsideRect(static_cast<float>(mouseX), static_cast<float>(mouseY), languageListState.x, languageListState.y, languageListState.w, languageListState.h)
            ? static_cast<int>((static_cast<float>(mouseY) - languageListState.y - verticalOffset + languageListState.scroll) / languageListState.rowHeight)
            : -1;
    }
    if (languageListState.hoveredIndex < 0 || languageListState.hoveredIndex >= static_cast<int>(languageMenuEntries.size())) languageListState.hoveredIndex = -1;

    if (darkTexture) {
        drawTiledBackground(darkTexture, screenW, screenH);
    }
    if (lightTexture) {
        drawTiledBackground(lightTexture, screenW, screenH, 0.0f, 0.0f, static_cast<float>(screenW), topStripHeight);
        drawTiledBackground(lightTexture, screenW, screenH, 0.0f, screenH - bottomStripHeight, static_cast<float>(screenW), bottomStripHeight);
    }

    drawMinecraftTextCentered(tr("Language", "Язык", "言語"), screenW * 0.5f, 60.0f, 3.0f, screenW, screenH, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    renderMenuList(languageListState, languageMenuEntries, screenW, screenH);

    const int i = 0;
    unsigned int tex = menuButtonTexture;
    const bool hovered = isMouseOverButton(languageButtons[i], mouseX, mouseY);
    if (menuButtonHighlightTexture && hovered) {
        tex = menuButtonHighlightTexture;
    }

    drawRectangle(languageButtons[i].absX, languageButtons[i].absY, languageButtons[i].absW, languageButtons[i].absH, tex, screenW, screenH);
    drawMinecraftTextCentered(
        languageButtons[i].label,
        languageButtons[i].absX + languageButtons[i].absW * 0.5f,
        languageButtons[i].absY + languageButtons[i].absH * 0.52f,
        fitMinecraftButtonTextScale(languageButtons[i].label, languageButtons[i].absW, languageButtons[i].absH),
        screenW,
        screenH,
        getMenuTextColor(hovered)
    );

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}

void handleLanguageMenuClick(GLFWwindow* window, int button) {
    if (button != GLFW_MOUSE_BUTTON_LEFT) return;

    double mx, my;
    glfwGetCursorPos(window, &mx, &my);
    int appliedIndex = -1;
    if (handleMenuListClick(languageListState, languageMenuEntries, mx, my, glfwGetTime(), appliedIndex)) {
        if (appliedIndex >= 0) {
            soundManager.playUISound("ui");
            applyLanguageSelection(appliedIndex);
        }
        return;
    }
    if (mx >= languageButtons[0].absX && mx <= languageButtons[0].absX + languageButtons[0].absW &&
        my >= languageButtons[0].absY && my <= languageButtons[0].absY + languageButtons[0].absH) {
        soundManager.playUISound("ui");
        if (languageListState.selectedIndex >= 0) applyLanguageSelection(languageListState.selectedIndex);
        currentState = GameState::MAIN_MENU;
    }
}

void updateMainMenuOptions(GLFWwindow* window) {
    static bool escWasPressed = false;
    int w = 0, h = 0;
    glfwGetWindowSize(window, &w, &h);
    updateOptionsLayout(w, h);
    
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        if (!escWasPressed) {
            currentState = GameState::MAIN_MENU; // Возврат в главное меню
        }
        escWasPressed = true;
    } else {
        escWasPressed = false;
    }
}

void updateOptionsLayout(int screenW, int screenH) {
    const float yOffset = screenH * 0.10f;
    const float topY = screenH * 0.15f;
    const float buttonW = OPTIONS_BUTTON_W;
    const float buttonH = OPTIONS_BUTTON_H;
    const float colGap = 44.0f * OPTIONS_UI_SCALE;
    const float rowGap = 34.0f * OPTIONS_UI_SCALE;
    const float leftX = screenW * 0.5f - colGap * 0.5f - buttonW;
    const float rightX = screenW * 0.5f + colGap * 0.5f;

    // FOV в левом верхнем блоке
    if (!optionsSliders.empty()) {
        optionsSliders[0].relX = (leftX + buttonW * 0.5f) / screenW;
        optionsSliders[0].relY = (topY + yOffset) / screenH;
        optionsSliders[0].relW = buttonW / screenW;
        optionsSliders[0].relH = buttonH / screenH;
        updateSliderPosition(optionsSliders[0], screenW, screenH);
    }

    // Difficulty справа от FOV
    optionsDifficultyButton.label = tr("Difficulty: Hard", "Сложность: Сложно", "難易度: ハード");
    optionsDifficultyButton.absW = buttonW;
    optionsDifficultyButton.absH = buttonH;
    optionsDifficultyButton.absX = rightX;
    optionsDifficultyButton.absY = topY - buttonH * 0.5f + yOffset;

    // Структура как на скриншоте
    // Одиночная кнопка справа под Difficulty
    optionsRow2Buttons[4].label = "Super Secret Settings...";
    optionsRow2Buttons[4].absX = rightX;
    optionsRow2Buttons[4].absY = optionsDifficultyButton.absY + buttonH + rowGap;
    optionsRow2Buttons[4].absW = buttonW;
    optionsRow2Buttons[4].absH = buttonH;

    // Две колонки ниже
    const float startRowsY = optionsRow2Buttons[4].absY + buttonH + rowGap;
    const char* leftLabels[4] = { "Music & Sounds...", "Video Settings...", "Language...", "Resource Packs..." };
    const char* rightLabels[4] = { "Broadcast Settings...", "Controls...", "Multiplayer Settings...", "Snooper Settings..." };
    for (int i = 0; i < 4; ++i) {
        optionsRow1Buttons[i].absW = buttonW;
        optionsRow1Buttons[i].absH = buttonH;
        optionsRow2Buttons[i].absW = buttonW;
        optionsRow2Buttons[i].absH = buttonH;

        optionsRow1Buttons[i].absX = leftX;
        optionsRow1Buttons[i].absY = startRowsY + i * (buttonH + rowGap * 0.55f);
        optionsRow1Buttons[i].label = leftLabels[i];

        optionsRow2Buttons[i].absX = rightX;
        optionsRow2Buttons[i].absY = startRowsY + i * (buttonH + rowGap * 0.55f);
        optionsRow2Buttons[i].label = rightLabels[i];
    }

    optionsDoneButton.label = tr("Done", "Готово", "完了");
    optionsDoneButton.absW = 560.0f * OPTIONS_UI_SCALE;
    optionsDoneButton.absH = 50.0f * OPTIONS_UI_SCALE;
    optionsDoneButton.absX = (screenW - optionsDoneButton.absW) * 0.5f;
    optionsDoneButton.absY = optionsRow1Buttons[3].absY + buttonH + 46.0f * OPTIONS_UI_SCALE;
}

void handleWorldSelectMenuClick(GLFWwindow* window, int button) {
    if (button != GLFW_MOUSE_BUTTON_LEFT) return;

    double mx, my;
    glfwGetCursorPos(window, &mx, &my);
    int appliedIndex = -1;
    if (handleMenuListClick(worldListState, worldMenuEntries, mx, my, glfwGetTime(), appliedIndex)) {
        if (appliedIndex >= 0 && appliedIndex < static_cast<int>(availableWorlds.size())) {
            soundManager.playUISound("ui");
            loadGame();
        }
        return;
    }
    for (int i = 0; i < WORLD_SELECT_BUTTON_COUNT; ++i) {
        if (mx < worldButtons[i].absX || mx > worldButtons[i].absX + worldButtons[i].absW ||
            my < worldButtons[i].absY || my > worldButtons[i].absY + worldButtons[i].absH) {
            continue;
        }

        // ДОБАВЛЕНО: проверяем, доступна ли кнопка
        if (!isWorldButtonEnabled(i)) {
            return; // Игнорируем клик по заблокированной кнопке
        }

        worldButtons[i].clicked = true;
        soundManager.playUISound("ui");
        if (i == 0) {
            if (worldListState.selectedIndex >= 0 && worldListState.selectedIndex < static_cast<int>(availableWorlds.size())) {
                loadGame();
            }
        } else if (i == 3) {
            startNewGame();
        } else if (i == 5) {
            currentState = GameState::MAIN_MENU;
        } else {
            // Остальные кнопки пока заглушки
        }
        worldButtons[i].clicked = false;
        break;
    }
}

void handleMainMenuClick(GLFWwindow* window, int button) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        int w, h;
        glfwGetWindowSize(window, &w, &h);
        double mx, my;
        glfwGetCursorPos(window, &mx, &my);
        for (int i = 0; i < MAIN_MENU_BUTTON_COUNT; ++i) {
            if (mx >= buttons[i].absX && mx <= buttons[i].absX + buttons[i].absW &&
                my >= buttons[i].absY && my <= buttons[i].absY + buttons[i].absH) {
                buttons[i].clicked = true;
                soundManager.playUISound("ui");
                if (i == 0) { // Singleplayer
                    refreshWorldMenuEntries();
                    currentState = GameState::WORLD_SELECT_MENU;
                } else if (i == 4) { // Exit
                    glfwSetWindowShouldClose(window, GLFW_TRUE);
                } else if (i == 1) { // Multiplayer
                    // Пока заглушка
                } else if (i == 2) { // Mods and resource packs
                    // Пока заглушка
                } else if (i == 3) { // Options
                    currentState = GameState::MAIN_MENU_OPTIONS;
                } else if (i == 5) {
                    refreshLanguageMenuEntries();
                    currentState = GameState::LANGUAGE_MENU;
                }
                
                buttons[i].clicked = false;
            }
        }
    }
}

void updateGame(GLFWwindow* window, float deltaTime) {
    if (invulnerabilityTimer > 0.0f) {
        invulnerabilityTimer -= deltaTime;
    }
    
    if (currentState == GameState::LOADING_GAME) {
        loadingTimer += deltaTime;
        updateChunksAroundCamera(cameraPos, isLoadingGame);
        buildChunkMeshesNearCamera(8); // быстрее строим меши во время загрузки
        if (areChunksReady()) {
            if (!playerPlaced) {
                placePlayerOnGround();
                playerPlaced = true;
            }
            movementEnabled = true;
            currentState = GameState::IN_GAME;
        }
        updateMusic();
    } 
    else if (currentState == GameState::IN_GAME) {
        updateChunksAroundCamera(cameraPos, isLoadingGame);
        buildChunkMeshesNearCamera(2);
        processInputInGame(window, deltaTime);
        updateMusic();
        updateMood(deltaTime);
    }
    else if (currentState == GameState::CREATIVE_INVENTORY) {
        // В инвентаре мир продолжает обновляться (чанки, музыка, настроение)
        // Но игрок не двигается (движение отключено в processInputInGame)
        updateChunksAroundCamera(cameraPos, isLoadingGame);
        buildChunkMeshesNearCamera(2);
        processInputInGame(window, deltaTime);
        updateMusic();
        updateMood(deltaTime);
    }
}

void processInputInGame(GLFWwindow* window, float deltaTime) {
    static bool escWasPressed = false;
    static bool eWasPressed = false;

    static bool f5WasPressed = false;
    bool f5Pressed = glfwGetKey(window, GLFW_KEY_F5) == GLFW_PRESS;
    if (f5Pressed && !f5WasPressed) {
        cameraMode = (cameraMode == CameraMode::FirstPerson) ? CameraMode::ThirdPersonBack : CameraMode::FirstPerson;
    }
    f5WasPressed = f5Pressed;

    
    // Обработка ESC
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        if (!escWasPressed) {
            if (currentState == GameState::CREATIVE_INVENTORY) {
                // Выход из инвентаря
                currentState = GameState::IN_GAME;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                glfwGetCursorPos(window, &lastX, &lastY);
                firstMouse = true;
            } else if (currentState == GameState::IN_GAME) {
                gamePaused = true;
                currentState = GameState::PAUSE_MENU;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }
        }
        escWasPressed = true;
    } else {
        escWasPressed = false;
    }
    
    // Обработка клавиши E (инвентарь)
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
        if (!eWasPressed) {
            if (currentState == GameState::IN_GAME) {
                // Открываем инвентарь
                currentState = GameState::CREATIVE_INVENTORY;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            } else if (currentState == GameState::CREATIVE_INVENTORY) {
                // Закрываем инвентарь
                currentState = GameState::IN_GAME;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                glfwGetCursorPos(window, &lastX, &lastY);
                firstMouse = true;
            }
        }
        eWasPressed = true;
    } else {
        eWasPressed = false;
    }
    
    // =========================================================
    // ФИЗИКА РАБОТАЕТ И В ИНВЕНТАРЕ, НО УПРАВЛЕНИЕ БЛОКИРУЕТСЯ
    // =========================================================
    const bool inventoryOpen = (currentState == GameState::CREATIVE_INVENTORY);
    if (currentState != GameState::IN_GAME && !inventoryOpen) return;
    if (!movementEnabled) return;

    // Определяем состояние воды
    glm::vec3 feetPosCheck = cameraPos - glm::vec3(0.0f, EYE_HEIGHT, 0.0f);
    bool feetInWater = getBlockAt(floor(feetPosCheck.x), floor(feetPosCheck.y + 0.1f), floor(feetPosCheck.z)) == 5;
    bool headInWater = getBlockAt(floor(cameraPos.x), floor(cameraPos.y + 0.2f), floor(cameraPos.z)) == 5;
    bool waistInWater = getBlockAt(floor(cameraPos.x), floor(cameraPos.y - 0.5f), floor(cameraPos.z)) == 5;
    
    bool inWater = feetInWater || waistInWater || headInWater;
    bool fullySubmerged = headInWater && waistInWater;
    bool atWaterSurface = waistInWater && !headInWater;
    static bool wasInWaterLastFrame = false;

    // В воде гравитация и скорость слабее, но не "ватные" как раньше.
    float currentGravity = inWater ? GRAVITY * 0.12f : GRAVITY;
    float moveSpeed = inWater ? WALK_SPEED * 0.62f : WALK_SPEED;

    glm::vec3 moveDir(0.0f);
    if (!inventoryOpen) {
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) moveDir += cameraFront;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) moveDir -= cameraFront;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) moveDir -= glm::normalize(glm::cross(cameraFront, cameraUp));
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) moveDir += glm::normalize(glm::cross(cameraFront, cameraUp));
    }
    
    bool moving = glm::length(moveDir) > 0.1f;
    if (moving) moveDir = glm::normalize(moveDir);
    glm::vec3 desiredMove = moveDir * moveSpeed * deltaTime;
    
    // Прыжок/всплытие и погружение
    const bool wantsSwimUp = !inventoryOpen && glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
    const bool wantsDiveDown = !inventoryOpen && glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;
    if (wantsSwimUp) {
        if (inWater) {
            const float swimImpulse = fullySubmerged ? 3.8f : 2.0f;
            playerVelocity.y = std::min(playerVelocity.y + swimImpulse * deltaTime * 7.5f, fullySubmerged ? 2.6f : 1.2f);
        } else if (isOnGround) {
            playerVelocity.y = JUMP_POWER;
            isOnGround = false;
        }
    }
    
    playerVelocity.y += currentGravity * deltaTime;

    if (inWater) {
        // Вода гасит вертикальную скорость, но оставляет ощущение инерции.
        playerVelocity.y *= 0.86f;

        if (fullySubmerged) {
            // Под водой игрок слабо всплывает, но может уверенно погружаться через Shift.
            if (wantsDiveDown && !wantsSwimUp) {
                playerVelocity.y = std::max(playerVelocity.y - 8.5f * deltaTime, -2.3f);
            } else {
                playerVelocity.y = std::min(playerVelocity.y + 0.55f * deltaTime, 1.5f);
            }
        } else if (atWaterSurface) {
            // На поверхности держим голову у кромки воды, но не позволяем левитировать над ней.
            if (wantsSwimUp) {
                playerVelocity.y = std::min(playerVelocity.y, 0.9f);
            } else if (wantsDiveDown) {
                playerVelocity.y = std::max(playerVelocity.y - 7.0f * deltaTime, -1.6f);
            } else {
                playerVelocity.y = std::min(playerVelocity.y, 0.0f);
                if (playerVelocity.y > -0.55f) {
                    playerVelocity.y -= 1.6f * deltaTime;
                }
            }

            if (!feetInWater) {
                playerVelocity.y = std::min(playerVelocity.y, -0.25f);
            }
        }

        playerVelocity.y = std::clamp(playerVelocity.y, -3.2f, 2.6f);
    } else if (wasInWaterLastFrame && playerVelocity.y > 0.0f) {
        // При выходе из воды резко гасим остаточный подъём, чтобы игрок не "парил" над поверхностью.
        playerVelocity.y *= 0.28f;
    }

    wasInWaterLastFrame = inWater;
    
    glm::vec3 delta = desiredMove;
    delta.y = playerVelocity.y * deltaTime;
    
    glm::vec3 feetPos = cameraPos - glm::vec3(0.0f, EYE_HEIGHT, 0.0f);
    glm::vec3 actualDelta = applyCollision(feetPos, delta);
    feetPos += actualDelta;
    cameraPos = feetPos + glm::vec3(0.0f, EYE_HEIGHT, 0.0f);
    
    if (feetPos.y < 0.0f) {
        feetPos.y = 0.0f;
        cameraPos = feetPos + glm::vec3(0, EYE_HEIGHT, 0);
        playerVelocity.y = 0;
        isOnGround = true;
    }

    // Звук шагов
    static bool wasMoving = false;
    
    if (isOnGround && moving) {
        if (!wasMoving) {
            stepSoundTimer = STEP_SOUND_INTERVAL;
        }
        
        stepSoundTimer += deltaTime;
        if (stepSoundTimer >= STEP_SOUND_INTERVAL) {
            stepSoundTimer -= STEP_SOUND_INTERVAL;
            int blockBelow = getBlockAt(floor(cameraPos.x), floor(feetPos.y - 0.1f), floor(cameraPos.z));
            if (blockBelow != 0 && blockBelow != 5) {
                soundManager.play(blockBelow, "step", gameStarted);
            }
        }
    } else {
        stepSoundTimer = 0.0f;
    }
    
    wasMoving = moving;

    // Выбор слота хотбара
    if (!inventoryOpen) {
        for (int i = 0; i < 9; ++i)
            if (glfwGetKey(window, GLFW_KEY_1 + i) == GLFW_PRESS)
                currentHotbarSlot = i;
        if (glfwGetKey(window, GLFW_KEY_9) == GLFW_PRESS)
            currentBlockType = 9;
        for (int i = 1; i <= 9; ++i)
            if (glfwGetKey(window, GLFW_KEY_0 + i) == GLFW_PRESS && blockTypes.count(i))
                currentBlockType = i;
    }
}


void updateGameplayCamera() {
    gameplayRayOrigin = cameraPos;
    gameplayRayDir = cameraFront;
    renderCameraPos = cameraPos;
    if (cameraMode == CameraMode::ThirdPersonBack) {
        glm::vec3 flatFront = glm::vec3(cameraFront.x, 0.0f, cameraFront.z);
        if (glm::length(flatFront) < 0.001f) flatFront = glm::vec3(0,0,-1);
        flatFront = glm::normalize(flatFront);
        renderCameraPos = cameraPos - flatFront * thirdPersonDistance + glm::vec3(0.0f, 0.2f, 0.0f);
    }
}

void initPlayerRenderer() {
    playerTexHead = loadTextureStrip("textures/entity/player/head.png", true);
    playerTexBody = loadTextureStrip("textures/entity/player/body.png", true);
    playerTexArmL = loadTextureStrip("textures/entity/player/arm_left.png", true);
    playerTexArmR = loadTextureStrip("textures/entity/player/arm.right.png", true);
    playerTexLegL = loadTextureStrip("textures/entity/player/leg_left.png", true);
    playerTexLegR = loadTextureStrip("textures/entity/player/leg_right.png", true);
    glGenVertexArrays(1, &playerVAO);
    glGenBuffers(1, &playerVBO);
}

void renderPlayerModel(const glm::vec3& feetPos, const glm::vec3& lookDir, float currentTime) {
    (void)currentTime;
    (void)lookDir;
    if (!playerVAO) return;

    auto drawPart=[&](glm::vec3 c, glm::vec3 sz, unsigned int tex){
        float x0=c.x-sz.x*0.5f,x1=c.x+sz.x*0.5f,y0=c.y,y1=c.y+sz.y,z0=c.z-sz.z*0.5f,z1=c.z+sz.z*0.5f;
        float v[]={
            x0,y0,z1,0,0, 0,0,1,1,0,  x1,y0,z1,1,0, 0,0,1,1,0,  x1,y1,z1,1,1, 0,0,1,1,0,  x1,y1,z1,1,1, 0,0,1,1,0,  x0,y1,z1,0,1, 0,0,1,1,0,  x0,y0,z1,0,0, 0,0,1,1,0,
            x1,y0,z0,0,0, 0,0,-1,1,0, x0,y0,z0,1,0, 0,0,-1,1,0, x0,y1,z0,1,1, 0,0,-1,1,0, x0,y1,z0,1,1, 0,0,-1,1,0, x1,y1,z0,0,1, 0,0,-1,1,0, x1,y0,z0,0,0, 0,0,-1,1,0,
            x0,y0,z0,0,0,-1,0,0,1,0,  x0,y0,z1,1,0,-1,0,0,1,0,  x0,y1,z1,1,1,-1,0,0,1,0,  x0,y1,z1,1,1,-1,0,0,1,0,  x0,y1,z0,0,1,-1,0,0,1,0,  x0,y0,z0,0,0,-1,0,0,1,0,
            x1,y0,z1,0,0,1,0,0,1,0,   x1,y0,z0,1,0,1,0,0,1,0,   x1,y1,z0,1,1,1,0,0,1,0,   x1,y1,z0,1,1,1,0,0,1,0,   x1,y1,z1,0,1,1,0,0,1,0,   x1,y0,z1,0,0,1,0,0,1,0,
            x0,y1,z1,0,0,0,1,0,1,0,   x1,y1,z1,1,0,0,1,0,1,0,   x1,y1,z0,1,1,0,1,0,1,0,   x1,y1,z0,1,1,0,1,0,1,0,   x0,y1,z0,0,1,0,1,0,1,0,   x0,y1,z1,0,0,0,1,0,1,0,
            x0,y0,z0,0,0,0,-1,0,1,0,  x1,y0,z0,1,0,0,-1,0,1,0,  x1,y0,z1,1,1,0,-1,0,1,0,  x1,y0,z1,1,1,0,-1,0,1,0,  x0,y0,z1,0,1,0,-1,0,1,0,  x0,y0,z0,0,0,0,-1,0,1,0
        };
        glBindTexture(GL_TEXTURE_2D, tex);
        glBindVertexArray(playerVAO);
        glBindBuffer(GL_ARRAY_BUFFER, playerVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,10*sizeof(float),(void*)0); glEnableVertexAttribArray(0);
        glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,10*sizeof(float),(void*)(3*sizeof(float))); glEnableVertexAttribArray(1);
        glVertexAttribPointer(2,3,GL_FLOAT,GL_FALSE,10*sizeof(float),(void*)(5*sizeof(float))); glEnableVertexAttribArray(2);
        glVertexAttribPointer(3,1,GL_FLOAT,GL_FALSE,10*sizeof(float),(void*)(8*sizeof(float))); glEnableVertexAttribArray(3);
        glVertexAttribPointer(4,1,GL_FLOAT,GL_FALSE,10*sizeof(float),(void*)(9*sizeof(float))); glEnableVertexAttribArray(4);
        glDrawArrays(GL_TRIANGLES,0,36);
    };

    float y=feetPos.y;
    drawPart(glm::vec3(feetPos.x, y+1.32f, feetPos.z), glm::vec3(0.6f,0.48f,0.6f), playerTexHead); // 8x8x8
    drawPart(glm::vec3(feetPos.x, y+0.72f, feetPos.z), glm::vec3(0.6f,0.72f,0.3f), playerTexBody); // 8x12x4
    drawPart(glm::vec3(feetPos.x-0.42f, y+0.72f, feetPos.z), glm::vec3(0.3f,0.72f,0.3f), playerTexArmL); //4x12x4
    drawPart(glm::vec3(feetPos.x+0.42f, y+0.72f, feetPos.z), glm::vec3(0.3f,0.72f,0.3f), playerTexArmR);
    drawPart(glm::vec3(feetPos.x-0.15f, y, feetPos.z), glm::vec3(0.3f,0.72f,0.3f), playerTexLegL);
    drawPart(glm::vec3(feetPos.x+0.15f, y, feetPos.z), glm::vec3(0.3f,0.72f,0.3f), playerTexLegR);
}

void renderGame(int screenW, int screenH, float currentTime) {
    glm::vec3 sunDir, skyColor;
    float sunIntensity, ambientBase;
    evaluateDayNightCycle(currentTime, sunDir, sunIntensity, ambientBase, skyColor);

    glClearColor(skyColor.r, skyColor.g, skyColor.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glm::mat4 model(1.0f);
    updateGameplayCamera();
    glm::mat4 view = glm::lookAt(renderCameraPos, renderCameraPos + cameraFront, cameraUp);
    
    // ИСПОЛЬЗУЕМ currentFOV ВМЕСТО ФИКСИРОВАННОГО ЗНАЧЕНИЯ 65.0f
    glm::mat4 proj = glm::perspective(glm::radians(currentFOV), 
                                      (float)screenW / screenH, 
                                      0.1f, 
                                      1000.0f);

    glUseProgram(shaderProgram);
    glUniformMatrix4fv(u_modelLoc, 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(u_viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(u_projLoc, 1, GL_FALSE, glm::value_ptr(proj));
    glUniform1f(u_time_location, currentTime);
    glUniform3fv(u_sunDir_location, 1, glm::value_ptr(sunDir));
    glUniform1f(u_sunIntensity_location, sunIntensity);
    glUniform1f(u_ambientBase_location, ambientBase);
    renderCloudLayer(currentTime);

    // Рендер всех чанков
    for (auto& p : loadedChunks)
        p.second.render();

    // Рендер воды (с сортировкой)
    updateWaterChunksCache();
    if (glm::distance(renderCameraPos, lastCameraPosForWaterSort) > 0.5f) {
        std::sort(waterChunksCache.begin(), waterChunksCache.end(), [&](Chunk* a, Chunk* b) {
            glm::vec3 ca(a->pos.x * CHUNK_SIZE_X + CHUNK_SIZE_X / 2, 30, a->pos.y * CHUNK_SIZE_Z + CHUNK_SIZE_Z / 2);
            glm::vec3 cb(b->pos.x * CHUNK_SIZE_X + CHUNK_SIZE_X / 2, 30, b->pos.y * CHUNK_SIZE_Z + CHUNK_SIZE_Z / 2);
            return glm::distance(renderCameraPos, ca) > glm::distance(renderCameraPos, cb);
        });
        lastCameraPosForWaterSort = renderCameraPos;
    }
    for (Chunk* ch : waterChunksCache)
        ch->renderWater();

    if (cameraMode == CameraMode::ThirdPersonBack) {
        glm::vec3 feetPos = gameplayRayOrigin - glm::vec3(0.0f, EYE_HEIGHT, 0.0f);
        renderPlayerModel(feetPos, cameraFront, currentTime);
    }

    // =========================================================
    // HUD И ИНВЕНТАРЬ
    // =========================================================
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Хотбар и сердца всегда остаются видимыми; инвентарь рисуется поверх них.
    drawHUD(screenW, screenH, currentTime);

    if (currentState == GameState::CREATIVE_INVENTORY) {
        drawDimOverlay(screenW, screenH, 0.6f);
        renderInventory(screenW, screenH);
    }
    
    // Рисуем прицел только в обычной игре.
    if (currentState != GameState::CREATIVE_INVENTORY && currentState != GameState::PAUSE_MENU) {
        glUseProgram(reticleProgram);
        glBindVertexArray(reticleVAO);
        glDrawArrays(GL_POINTS, 0, 1);
    }
    
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

void initCloudLayer() {
    cloudTexture = loadTextureStrip("textures/environment/clouds.png", true);

    const float y = 120.0f;
    const float halfSize = 2048.0f;
    const float uvScale = 8.0f;
    const float vtx[] = {
        -halfSize, y, -halfSize,   0.0f,     0.0f,    0.0f, -1.0f, 0.0f, 0.92f, 0.0f,
         halfSize, y, -halfSize,   uvScale,  0.0f,    0.0f, -1.0f, 0.0f, 0.92f, 0.0f,
         halfSize, y,  halfSize,   uvScale,  uvScale, 0.0f, -1.0f, 0.0f, 0.92f, 0.0f,
         halfSize, y,  halfSize,   uvScale,  uvScale, 0.0f, -1.0f, 0.0f, 0.92f, 0.0f,
        -halfSize, y,  halfSize,   0.0f,     uvScale, 0.0f, -1.0f, 0.0f, 0.92f, 0.0f,
        -halfSize, y, -halfSize,   0.0f,     0.0f,    0.0f, -1.0f, 0.0f, 0.92f, 0.0f
    };

    glGenVertexArrays(1, &cloudVAO);
    glGenBuffers(1, &cloudVBO);
    glBindVertexArray(cloudVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cloudVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vtx), vtx, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 10 * sizeof(float), (void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 10 * sizeof(float), (void*)(3 * sizeof(float))); glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 10 * sizeof(float), (void*)(5 * sizeof(float))); glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, 10 * sizeof(float), (void*)(8 * sizeof(float))); glEnableVertexAttribArray(3);
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, 10 * sizeof(float), (void*)(9 * sizeof(float))); glEnableVertexAttribArray(4);
    glBindVertexArray(0);
}

void renderCloudLayer(float currentTime) {
    if (!cloudVAO || !cloudTexture) return;
    const float cloudSpeed = 0.45f;
    glm::mat4 cloudModel = glm::translate(glm::mat4(1.0f), glm::vec3(currentTime * cloudSpeed, 0.0f, 0.0f));
    glUniformMatrix4fv(u_modelLoc, 1, GL_FALSE, glm::value_ptr(cloudModel));
    glUniform1i(u_isWater_location, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, cloudTexture);
    glBindVertexArray(cloudVAO);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDisable(GL_BLEND);
    glBindVertexArray(0);
    glUniformMatrix4fv(u_modelLoc, 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));
}

void updatePauseMenu(GLFWwindow* window) {
    static bool escWasPressed = false;
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        if (!escWasPressed) {
            gamePaused = false;
            currentState = GameState::IN_GAME;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            glfwGetCursorPos(window, &lastX, &lastY);
            firstMouse = true;
        }
        escWasPressed = true;
    } else {
        escWasPressed = false;
    }
}

void handleMainMenuOptionsClick(GLFWwindow* window, int button) {
    if (button != GLFW_MOUSE_BUTTON_LEFT) return;

    double mx, my;
    glfwGetCursorPos(window, &mx, &my);

    if (isMouseOverButton(optionsDoneButton, mx, my)) {
        soundManager.playUISound("ui");
        currentState = GameState::MAIN_MENU;
        return;
    }

    for (int i = 0; i < OPTIONS_ROW1_BUTTON_COUNT; ++i) {
        if (isMouseOverButton(optionsRow1Buttons[i], mx, my)) {
            if (std::string(optionsRow1Buttons[i].label) == "Language...") {
                soundManager.playUISound("ui");
                refreshLanguageMenuEntries();
                currentState = GameState::LANGUAGE_MENU;
            }
            return;
        }
    }
}

void renderPauseMenu(int screenW, int screenH) {
    GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
    GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
    GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);
    GLint prevBlendSrc, prevBlendDst;
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &prevBlendSrc);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &prevBlendDst);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(dimShaderProgram);
    glUniform4f(glGetUniformLocation(dimShaderProgram, "uColor"), 0.0f, 0.0f, 0.0f, 0.5f);
    glBindVertexArray(dimVAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    glUseProgram(uiShaderProgram);
    glm::mat4 proj = glm::ortho(0.0f, (float)screenW, (float)screenH, 0.0f);
    glUniformMatrix4fv(glGetUniformLocation(uiShaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(proj));
    glUniform1i(glGetUniformLocation(uiShaderProgram, "uUseTexture"), 1);
    glUniform4f(glGetUniformLocation(uiShaderProgram, "uColor"), 1.0f, 1.0f, 1.0f, 1.0f);
    glBindVertexArray(uiVAO);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(glGetUniformLocation(uiShaderProgram, "uTexture"), 0);

    // Кнопка Resume
    unsigned int texResume = menuButtonTexture;
    const bool resumeHovered = isMouseOverButton(pauseResumeButton, mouseX, mouseY);
    if (menuButtonHighlightTexture && resumeHovered) {
        texResume = menuButtonHighlightTexture;
    }
    glm::mat4 modelResume = glm::translate(glm::mat4(1.0f), glm::vec3(pauseResumeButton.absX, pauseResumeButton.absY, 0.0f));
    modelResume = glm::scale(modelResume, glm::vec3(pauseResumeButton.absW, pauseResumeButton.absH, 1.0f));
    glUniformMatrix4fv(glGetUniformLocation(uiShaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(modelResume));
    glBindTexture(GL_TEXTURE_2D, texResume);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    drawMinecraftTextCentered(
        pauseResumeButton.label,
        pauseResumeButton.absX + pauseResumeButton.absW * 0.5f,
        pauseResumeButton.absY + pauseResumeButton.absH * 0.52f,
        fitMinecraftButtonTextScale(pauseResumeButton.label, pauseResumeButton.absW, pauseResumeButton.absH),
        screenW,
        screenH,
        getMenuTextColor(resumeHovered)
    );

    // Кнопка Exit to Menu
    unsigned int texExit = menuButtonTexture;
    const bool exitHovered = isMouseOverButton(pauseExitButton, mouseX, mouseY);
    if (menuButtonHighlightTexture && exitHovered) {
        texExit = menuButtonHighlightTexture;
    }
    glm::mat4 modelExit = glm::translate(glm::mat4(1.0f), glm::vec3(pauseExitButton.absX, pauseExitButton.absY, 0.0f));
    modelExit = glm::scale(modelExit, glm::vec3(pauseExitButton.absW, pauseExitButton.absH, 1.0f));
    glUniformMatrix4fv(glGetUniformLocation(uiShaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(modelExit));
    glBindTexture(GL_TEXTURE_2D, texExit);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    drawMinecraftTextCentered(
        pauseExitButton.label,
        pauseExitButton.absX + pauseExitButton.absW * 0.5f,
        pauseExitButton.absY + pauseExitButton.absH * 0.52f,
        fitMinecraftButtonTextScale(pauseExitButton.label, pauseExitButton.absW, pauseExitButton.absH),
        screenW,
        screenH,
        getMenuTextColor(exitHovered)
    );

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    if (blendWasEnabled) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    glBlendFunc(prevBlendSrc, prevBlendDst);
    if (cullWasEnabled) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    if (depthWasEnabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
}

void handlePauseMenuClick(GLFWwindow* window, int button) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        int w, h;
        glfwGetWindowSize(window, &w, &h);
        double mx, my;
        glfwGetCursorPos(window, &mx, &my);
        if (mx >= pauseResumeButton.absX && mx <= pauseResumeButton.absX + pauseResumeButton.absW &&
            my >= pauseResumeButton.absY && my <= pauseResumeButton.absY + pauseResumeButton.absH) {
            soundManager.playUISound("ui");
            gamePaused = false;
            currentState = GameState::IN_GAME;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            glfwGetCursorPos(window, &lastX, &lastY);
            firstMouse = true;
        } else if (mx >= pauseExitButton.absX && mx <= pauseExitButton.absX + pauseExitButton.absW &&
                   my >= pauseExitButton.absY && my <= pauseExitButton.absY + pauseExitButton.absH) {
            soundManager.playUISound("ui");
            exitToMenu(window, currentScreenW, currentScreenH);
        }
    }
}

// ----------------------------------------------------------------------
// Коллбэки GLFW
// ----------------------------------------------------------------------
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (currentState == GameState::MAIN_MENU) {
        if (action != GLFW_PRESS) return;
        handleMainMenuClick(window, button);
    } else if (currentState == GameState::WORLD_SELECT_MENU) {
        if (action != GLFW_PRESS) return;
        handleWorldSelectMenuClick(window, button);
    }else if (currentState == GameState::MAIN_MENU_OPTIONS) {
        if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
            double mx, my;
            glfwGetCursorPos(window, &mx, &my);
            for (auto& slider : optionsSliders) {
                if (handleSliderClick(slider, mx, my)) return;
            }
        } else if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
            for (auto& slider : optionsSliders) releaseSlider(slider);
        }
        if (action == GLFW_PRESS) handleMainMenuOptionsClick(window, button);
    } else if (currentState == GameState::PAUSE_MENU) {
        if (action != GLFW_PRESS) return;
        handlePauseMenuClick(window, button);
    }else if (currentState == GameState::CREATIVE_INVENTORY) {
        // Заглушка
        return;
    } else if (currentState == GameState::LANGUAGE_MENU) {
        if (action != GLFW_PRESS) return;
        handleLanguageMenuClick(window, button);
    } else if (currentState == GameState::IN_GAME) {
        if (action != GLFW_PRESS) return;
        if (!movementEnabled) return;
        glm::vec3 rayDir = gameplayRayDir;
        int hx, hy, hz, face;
        if (rayCast(gameplayRayOrigin, rayDir, hx, hy, hz, face, 10.0f)) {
            if (button == GLFW_MOUSE_BUTTON_LEFT) {
                int blockType = getBlockAt(hx, hy, hz);
                if (blockType == 5) return;
                setBlockAt(hx, hy, hz, 0);
                soundManager.play(blockType, "hit", gameStarted);
            } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
                int nx = hx, ny = hy, nz = hz;
                switch (face) {
                    case 0: nx = hx + 1; break;
                    case 1: nx = hx - 1; break;
                    case 2: ny = hy + 1; break;
                    case 3: ny = hy - 1; break;
                    case 4: nz = hz + 1; break;
                    case 5: nz = hz - 1; break;
                }
                if (getBlockAt(nx, ny, nz) != 0 && getBlockAt(nx, ny, nz) != 5) return;
                setBlockAt(nx, ny, nz, currentBlockType);
                soundManager.play(currentBlockType, "place", gameStarted);
            }
        }
    }
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    if (currentState == GameState::CREATIVE_INVENTORY) {
        inventoryScrollRow = std::max(0, inventoryScrollRow - static_cast<int>(yoffset));
        return;
    }
    
    if (currentState == GameState::WORLD_SELECT_MENU &&
        isInsideRect(static_cast<float>(mouseX), static_cast<float>(mouseY), worldListState.x, worldListState.y, worldListState.w, worldListState.h)) {
        scrollMenuList(worldListState, worldMenuEntries, yoffset);
    } else if (currentState == GameState::LANGUAGE_MENU &&
               isInsideRect(static_cast<float>(mouseX), static_cast<float>(mouseY), languageListState.x, languageListState.y, languageListState.w, languageListState.h)) {
        scrollMenuList(languageListState, languageMenuEntries, yoffset);
    }
}

void cursor_pos_callback(GLFWwindow* window, double x, double y) {
    mouseX = x;
    mouseY = y;
    if (currentState == GameState::MAIN_MENU_OPTIONS) {
        for (auto& slider : optionsSliders) {
            handleSliderDrag(slider, x, y);
        }
    }
    if (currentState != GameState::IN_GAME || gamePaused) return;
    if (firstMouse) {
        lastX = x;
        lastY = y;
        firstMouse = false;
    }
    float xoff = x - lastX;
    float yoff = lastY - y;
    lastX = x;
    lastY = y;
    xoff *= 0.1f;
    yoff *= 0.1f;
    yaw += xoff;
    pitch += yoff;
    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;
    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(front);
}

// ----------------------------------------------------------------------
// Шейдеры и инициализация
// ----------------------------------------------------------------------
const char *vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in vec3 aNormal;
layout (location = 3) in float aLight;
layout (location = 4) in float aBlockLight;
out vec2 TexCoord; out vec3 FragPos; out vec3 Normal; out float LightLevel; out float BlockLightLevel;
uniform mat4 model; uniform mat4 view; uniform mat4 projection;
void main() {
    vec4 worldPos = model * vec4(aPos, 1.0);
    gl_Position = projection * view * worldPos;
    TexCoord = aTexCoord; FragPos = worldPos.xyz;
    Normal = mat3(transpose(inverse(model))) * aNormal;
    LightLevel = aLight;
    BlockLightLevel = aBlockLight;
}
)";
const char *fragmentShaderSource = R"(
#version 330 core
in vec2 TexCoord; out vec4 FragColor;
uniform sampler2D ourTexture; uniform float u_time; uniform int u_isWater;
uniform vec3 u_sunDir; uniform float u_sunIntensity; uniform float u_ambientBase;
in vec3 FragPos; in vec3 Normal; in float LightLevel; in float BlockLightLevel;
void main() {
    vec2 uv = TexCoord;
    if (u_isWater == 1) {
        float frames = 32.0, speed = 0.7;
        float frame = fract(u_time * speed) * frames;
        uv.y = uv.y / frames + floor(frame) / frames;
    }
    vec4 color = texture(ourTexture, uv); if (u_isWater==1) color.a = 0.7;
    vec3 n = normalize(Normal);
    float vertexLight = clamp(LightLevel, 0.0, 1.0);
    float blockLightOnly = clamp(BlockLightLevel, 0.0, 1.0);

    // Minecraft-подобное постоянное затенение граней: без "солнца сбоку".
    float faceShade = 0.86;
    if (n.y > 0.5) faceShade = 1.0;
    else if (n.y < -0.5) faceShade = 0.72;

    float ambientFactor = mix(0.10, 1.0, pow(vertexLight, 1.15));
    float dayFactor = mix(0.55, 1.0, clamp(u_sunIntensity, 0.0, 1.0));
    float sunLighting = u_ambientBase * ambientFactor * faceShade * dayFactor;

    // Блочный свет (факелы/лампы) не должен полностью гаснуть ночью.
    // Оставляем мягкую кривую, чтобы в темноте источники света выглядели ярко,
    // а днём не пересвечивали поверхность.
    float emissiveLighting = pow(blockLightOnly, 1.35) * 0.95;
    float lighting = max(sunLighting, emissiveLighting);

    if (u_isWater==1) lighting = max(lighting, 0.12);
    FragColor = vec4(color.rgb * lighting, color.a);
}
)";
const char *reticleVertexSource = R"(#version 330 core
void main() { gl_PointSize = 10.0; gl_Position = vec4(0.0, 0.0, 0.0, 1.0); }
)";
const char *reticleFragmentSource = R"(#version 330 core
out vec4 FragColor;
void main() { vec2 c = gl_PointCoord; if (length(c-0.5)>0.4) discard; FragColor=vec4(1.0); }
)";
void checkShaderErrors(unsigned int s, const std::string& t) { int ok; glGetShaderiv(s,GL_COMPILE_STATUS,&ok); if(!ok){char l[512]; glGetShaderInfoLog(s,512,NULL,l); std::cerr<<t<<" error:\n"<<l<<std::endl;}}
void checkProgramErrors(unsigned int p) { int ok; glGetProgramiv(p,GL_LINK_STATUS,&ok); if(!ok){char l[512]; glGetProgramInfoLog(p,512,NULL,l); std::cerr<<"Link error:\n"<<l<<std::endl;}}
void evaluateDayNightCycle(float t, glm::vec3& sunDir, float& sunInt, float& amb, glm::vec3& sky) {
    float cycle = fmod(t, FULL_CYCLE_SECONDS) / FULL_CYCLE_SECONDS;
    float angle = cycle * glm::two_pi<float>() - glm::half_pi<float>();
    sunDir = glm::normalize(glm::vec3(cos(angle), sin(angle), 0.35f));
    float daylight = glm::clamp(sunDir.y * 1.15f + 0.15f, 0.0f, 1.0f);
    daylight = daylight * daylight * (3.0f - 2.0f*daylight);
    sunInt = 0.05f + daylight * 0.95f;
    amb = 0.02f + daylight * 0.36f;
    sky = glm::mix(glm::vec3(0.01,0.015,0.045), glm::vec3(0.53,0.81,0.92), daylight);
}
void initReticle() {
    unsigned int vs = glCreateShader(GL_VERTEX_SHADER), fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(vs,1,&reticleVertexSource,NULL); glCompileShader(vs);
    glShaderSource(fs,1,&reticleFragmentSource,NULL); glCompileShader(fs);
    reticleProgram = glCreateProgram(); glAttachShader(reticleProgram,vs); glAttachShader(reticleProgram,fs);
    glLinkProgram(reticleProgram); checkProgramErrors(reticleProgram);
    glDeleteShader(vs); glDeleteShader(fs); glGenVertexArrays(1,&reticleVAO); glBindVertexArray(reticleVAO); glBindVertexArray(0);
}

// ----------------------------------------------------------------------
// main
// ----------------------------------------------------------------------
int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_ALPHA_BITS,8);
    GLFWmonitor* mon = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(mon);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(mode->width, mode->height, "Voxel Builder", NULL, NULL);
    if (!window) { glfwTerminate(); return -1; }
    glfwSetWindowPos(window,0,0);
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, [](GLFWwindow*,int w,int h){ glViewport(0,0,w,h); });
    glfwSetCursorPosCallback(window, cursor_pos_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    glfwSetWindowCloseCallback(window, [](GLFWwindow*){ if(gameStarted) saveAllChunks(); });

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return -1;
    glEnable(GL_CULL_FACE); glCullFace(GL_BACK); glFrontFace(GL_CCW); glEnable(GL_DEPTH_TEST);

    unsigned int vs = glCreateShader(GL_VERTEX_SHADER), fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(vs,1,&vertexShaderSource,NULL); glCompileShader(vs); checkShaderErrors(vs,"vertex");
    glShaderSource(fs,1,&fragmentShaderSource,NULL); glCompileShader(fs); checkShaderErrors(fs,"fragment");
    shaderProgram = glCreateProgram(); glAttachShader(shaderProgram,vs); glAttachShader(shaderProgram,fs);
    glLinkProgram(shaderProgram); checkProgramErrors(shaderProgram);
    glDeleteShader(vs); glDeleteShader(fs); glUseProgram(shaderProgram);
    glUniform1i(glGetUniformLocation(shaderProgram,"ourTexture"),0);
    u_time_location = glGetUniformLocation(shaderProgram,"u_time");
    u_isWater_location = glGetUniformLocation(shaderProgram,"u_isWater");
    u_sunDir_location = glGetUniformLocation(shaderProgram,"u_sunDir");
    u_sunIntensity_location = glGetUniformLocation(shaderProgram,"u_sunIntensity");
    u_ambientBase_location = glGetUniformLocation(shaderProgram,"u_ambientBase");
    u_modelLoc = glGetUniformLocation(shaderProgram,"model");
    u_viewLoc = glGetUniformLocation(shaderProgram,"view");
    u_projLoc = glGetUniformLocation(shaderProgram,"projection");

    if (!loadBlockConfig("blocks.json")) return -1;
    if (!loadItemConfig("items.json")) return -1;
    initUI(); loadMenuTextures(); loadHUDTextures(); initLanguageMenu();
    loadSliderTextures();
    initFOVSlider(optionsSliders);
    initCloudLayer();
    initPlayerRenderer();
    if (fs::exists("sounds/hurtflesh1.ogg")) {
        soundManager.loadPlayerSound("hurt", "sounds/hurtflesh1.ogg");
    }
    if (fs::exists("sounds/hurtflesh2.ogg")) {
        soundManager.loadPlayerSound("hurt", "sounds/hurtflesh2.ogg");
    }
    if (fs::exists("sounds/ui/click.ogg")) {
        soundManager.loadPlayerSound("ui", "sounds/ui/click.ogg");
    }
    unsigned int dimVS = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(dimVS, 1, &dimVertexShaderSrc, NULL); glCompileShader(dimVS);
    unsigned int dimFS = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(dimFS, 1, &dimFragmentShaderSrc, NULL); glCompileShader(dimFS);
    dimShaderProgram = glCreateProgram();
    glAttachShader(dimShaderProgram, dimVS);
    glAttachShader(dimShaderProgram, dimFS);
    glLinkProgram(dimShaderProgram);
    glDeleteShader(dimVS);
    glDeleteShader(dimFS);

    int screenW = mode->width, screenH = mode->height;
    currentScreenW = screenW; currentScreenH = screenH;
    updateButtonPositions(screenW, screenH);
    updatePhotoPosition(screenW, screenH);
    scanMusicFolder();
    scanAmbientSounds();
    currentState = GameState::MAIN_MENU;

    // Переменные для отслеживания нажатий клавиш
    static bool escWasPressedInMenu = false;
    
    while (!glfwWindowShouldClose(window)) {
        float now = glfwGetTime();
        if (currentState != GameState::PAUSE_MENU) {
            deltaTime = now - lastFrame;
            lastFrame = now;
        } else {
            deltaTime = 0.0f;
        }
        if (deltaTime > 0.1f) deltaTime = 0.1f;

        glfwPollEvents();

        // Обработка Escape в меню выбора мира
        if (currentState == GameState::WORLD_SELECT_MENU) {
            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                if (!escWasPressedInMenu) {
                    worldListState.selectedIndex = -1;
                    worldListState.lastClickedIndex = -1;
                }
                escWasPressedInMenu = true;
            } else {
                escWasPressedInMenu = false;
            }
        }

        // Обработка Escape в меню языка
        if (currentState == GameState::LANGUAGE_MENU) {
            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                if (!escWasPressedInMenu) {
                    currentState = GameState::MAIN_MENU;
                }
                escWasPressedInMenu = true;
            } else {
                escWasPressedInMenu = false;
            }
        }

        switch (currentState) {
            case GameState::MAIN_MENU:
                updateMainMenu(window);
                renderMainMenu(screenW, screenH);
                break;
            case GameState::WORLD_SELECT_MENU:
                updateWorldSelectMenu(window);
                renderWorldSelectMenu(screenW, screenH);
                break;
            case GameState::LANGUAGE_MENU:
                renderLanguageMenu(screenW, screenH);
                break;
            case GameState::LOADING_GAME:
            case GameState::IN_GAME:
            case GameState::CREATIVE_INVENTORY:
                updateGame(window, deltaTime);
                renderGame(screenW, screenH, now);
                if (currentState == GameState::PAUSE_MENU)
                    break;
                break;
            case GameState::MAIN_MENU_OPTIONS:
                updateMainMenuOptions(window);
                renderMainMenuOptions(screenW, screenH);
                break;
            case GameState::PAUSE_MENU:
                renderGame(screenW, screenH, now);
                renderPauseMenu(screenW, screenH);
                updatePauseMenu(window);
                break;
        }

        glfwSwapBuffers(window);
    }

    if (gameStarted) {
        saveAllChunks();
        workerRunning = false;
        if (workerThread.joinable()) workerThread.join();
    }
    stopMusic();
        // Очистка ambient звука при выходе
    if (ambientSound) {
        delete ambientSound;
        ambientSound = nullptr;
    }
    glDeleteTextures(1, &menuBackgroundTexture);
    glDeleteTextures(1, &menuBackgroundLightTexture);
    glDeleteTextures(1, &menuBackgroundDarkTexture);
    glDeleteTextures(1, &menuButtonTexture);
    glDeleteTextures(1, &menuButtonHighlightTexture);
    glDeleteTextures(1, &menuPhotoTexture);
    glDeleteTextures(1, &menuButtonDisabledTexture);
    glDeleteTextures(1, &hotbarTexture);
    glDeleteTextures(1, &heartFullTexture);
    glDeleteTextures(1, &heartHalfTexture);
    glDeleteTextures(1, &hotbarSelTexture);
    glDeleteTextures(1, &heartContTexture);
    glDeleteTextures(1, &inventoryTexture);
    glDeleteTextures(1, &inventoryScrollerTexture);
    glDeleteTextures(1, &inventoryScrollerDisabledTexture);
    glDeleteTextures(1, &minecraftAsciiTexture);
    for (auto& page : minecraftFontPages) {
        if (page.second != 0) {
            glDeleteTextures(1, &page.second);
        }
    }
    glDeleteVertexArrays(1, &uiVAO);
    glDeleteVertexArrays(1, &fontVAO);
    glDeleteBuffers(1, &uiVBO);
    glDeleteBuffers(1, &uiEBO);
    glDeleteBuffers(1, &fontVBO);
    glDeleteBuffers(1, &fontEBO);
    glDeleteProgram(uiShaderProgram);
    glDeleteVertexArrays(1, &dimVAO);
    glDeleteProgram(dimShaderProgram);
    for (auto& p : blockTypes) glDeleteTextures(1, &p.second.textureID);
    for (auto& p : itemTypes) {
        if (!p.second.isBlock && p.second.textureID != 0) {
            glDeleteTextures(1, &p.second.textureID);
        }
    }
    if (cloudTexture) glDeleteTextures(1, &cloudTexture);
    if (cloudVAO) glDeleteVertexArrays(1, &cloudVAO);
    if (cloudVBO) glDeleteBuffers(1, &cloudVBO);
    glDeleteProgram(shaderProgram);
    glDeleteVertexArrays(1, &reticleVAO);
    glDeleteProgram(reticleProgram);
    glfwTerminate();
    return 0;
}
