// Базовые include, глобальные параметры, структуры UI и прототипы.
// Этот файл подключается из main2.cpp и рассчитан на сборку в одном translation unit.

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
FastNoiseLite mountainNoise;
FastNoiseLite riverNoise;
FastNoiseLite biomeNoiseA;
FastNoiseLite biomeNoiseB;
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
unsigned int underwaterOverlayTexture = 0;

// ----------------------------------------------------------------------
// Параметры чанков
// ----------------------------------------------------------------------
const int CHUNK_SIZE_X = 16;
const int CHUNK_SIZE_Z = 16;
const int CHUNK_SIZE_Y = 256;
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
const float INVULNERABILITY_DURATION = 0.5f; // 10 тиков (0.5 секунды) неуязвимости после получения урона, как в Minecraft
bool jumpKeyWasPressed = false;
float heldJumpRepeatTimer = 0.0f;
const float HELD_JUMP_REPEAT_DELAY = 0.25f; // Задержка автопрыжка только при зажатом Space (5 тиков)
const float MIN_FALL_DAMAGE_HEIGHT = 3.0f; // Минимальная высота для получения урона (3 блока)
const float MAX_FALL_DAMAGE_HEIGHT = 23.0f; // Высота, с которой урон максимален (23 блока)
// ----------------------------------------------------------------------
// Таблица твёрдости блоков и светимости
// ----------------------------------------------------------------------
bool isSolidBlockFast[256] = {false};
bool blockHasAlpha[256] = {false};
bool blockDrawsSameAlphaFaces[256] = {false};
int blockLightEmission[256] = {0};
int blockOpacity[256] = {0};
int maxBlockLightRadius = 0;

constexpr int WORLD_VERTEX_FLOATS = 13;
constexpr int BLOCK_GRASS = 1;
constexpr int BLOCK_OAK_LEAVES = 7;

struct BiomeColorMap {
    std::vector<unsigned char> pixels;
    int width = 0;
    int height = 0;
    glm::vec3 fallback = glm::vec3(1.0f);
    bool loaded() const { return !pixels.empty() && width > 0 && height > 0; }
};

BiomeColorMap grassColorMap;
BiomeColorMap foliageColorMap;
unsigned int grassSideOverlayTexture = 0;

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
void processPendingLightMeshRebuilds(int maxPerFrame);
void processPendingInteractionMeshRebuilds(int maxPerFrame);
void scheduleSkyLightColumnsAroundAsync(int x, int z);
void processPendingSkyLightColumnUpdates(int maxColumnsPerFrame);
void initReticle();
void evaluateDayNightCycle(float t, glm::vec3& sunDir, float& sunInt, float& amb, glm::vec3& sky);
void checkShaderErrors(unsigned int s, const std::string& t);
void checkProgramErrors(unsigned int p);
bool deleteSelectedWorld();
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
int getBlockAtPoint(const glm::vec3& point);
int getBlockUnderCamera(const glm::vec3& cameraPosition, const glm::vec3& feetPosition);
void renderItemIconFlat(int itemId, int screenX, int screenY, int size, int screenW, int screenH);
void initCloudLayer();
void initCelestialBodies();
void initStarField();
void renderStars(float currentTime, const glm::vec3& sunDir, const glm::mat4& view, const glm::mat4& proj);
void renderSkyHorizonGlow(const glm::vec3& sunDir, const glm::mat4& view, const glm::mat4& proj);
void renderCelestialBodies(float currentTime, const glm::vec3& sunDir);

void initPlayerRenderer();
void renderPlayerModel(const glm::vec3& feetPos, const glm::vec3& lookDir, float currentTime);
void updateGameplayCamera();
void renderCloudLayer(float currentTime);
void renderRainLayer(float currentTime);
void initBiomeColorAssets();
glm::vec3 getBiomeTintColor(int wx, int wz, bool foliage);
bool isBiomeTintedGuiBlockFace(int blockType, int faceIdx, bool& foliage);
glm::vec3 getGuiBiomeTintColor(bool foliage);
void addFaceToVertices(std::vector<float>& verts, 
    glm::vec3 v1, glm::vec3 v2, glm::vec3 v3, glm::vec3 v4,
    glm::vec3 normal, float uOffset);
    struct InventoryItem {
        int blockType;      // Тип блока (1-255)
        int count;          // Количество
    };
    
    // Единый инвентарь игрока: 4x9 (36 слотов).
    // Последние 9 слотов [27..35] используются как хотбар и в HUD, и в меню инвентаря.
    InventoryItem playerInventoryItems[36] = {
        {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
        {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
        {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
        {1, 64}, {2, 64}, {3, 64}, {4, 64}, {5, 8}, {6, 64}, {7, 64}, {8, 32}, {12, 16}
    };
void drawDimOverlay(int screenW, int screenH, float alpha);
void renderInventory(int screenW, int screenH);
void syncInventoryHotbarFromGameHotbar();
unsigned int loadUITexture(const char* path);
unsigned int loadTextureStrip(const char* path, bool forceAlpha = false);
bool loadItemConfig(const std::string& path);
void drawRectangle(float x, float y, float w, float h, unsigned int texture, int screenW, int screenH);
void drawRectangleUV(float x, float y, float w, float h, unsigned int texture, int screenW, int screenH, float u0, float v0, float u1, float v1);
float fitMinecraftTextScale(const std::string& text, float maxWidth, float maxHeight);
void drawMinecraftTextCentered(const std::string& text, float centerX, float centerY, float scale, int screenW, int screenH, const glm::vec4& color);
const char* tr(const char* en, const char* ru, const char* jp);
extern double mouseX, mouseY;
extern bool gameStarted;
// Добавьте эти строки в секцию прототипов функций (примерно строка 63)
struct Slider;
void loadSliderTextures();
void updateSliderPosition(Slider& slider, int screenW, int screenH);
void updateSliderPositions(std::vector<Slider>& sliders, int screenW, int screenH);
std::string formatSliderValue(float value, int decimalPlaces);
bool isMouseOverSlider(const Slider& slider, double mouseX, double mouseY);
void drawSlider(const Slider& slider, int screenW, int screenH);
bool handleSliderClick(Slider& slider, double mouseX, double mouseY);
void handleSliderDrag(Slider& slider, double mouseX, double mouseY);
void releaseSlider(Slider& slider);
void initFOVSlider(std::vector<Slider>& sliders);
bool isWaterBlockAt(float x, float y, float z);
bool isCameraUnderwater();
void renderUnderwaterOverlay(int screenW, int screenH, float currentTime);


// ----------------------------------------------------------------------
// Реализация коллизий (без изменений)
