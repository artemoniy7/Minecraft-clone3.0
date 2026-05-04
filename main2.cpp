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
void updateGameplayCamera();
void renderCloudLayer(float currentTime);
void renderRainLayer(float currentTime);
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
        {1, 64}, {2, 64}, {3, 64}, {4, 64}, {5, 8}, {6, 64}, {7, 64}, {8, 32}, {9, 16}
    };
void drawDimOverlay(int screenW, int screenH, float alpha);
void renderInventory(int screenW, int screenH);
void syncInventoryHotbarFromGameHotbar();
unsigned int loadUITexture(const char* path);
unsigned int loadTextureStrip(const char* path, bool forceAlpha = false);
bool loadItemConfig(const std::string& path);
void drawRectangle(float x, float y, float w, float h, unsigned int texture, int screenW, int screenH);
float fitMinecraftTextScale(const std::string& text, float maxWidth, float maxHeight);
void drawMinecraftTextCentered(const std::string& text, float centerX, float centerY, float scale, int screenW, int screenH, const glm::vec4& color);
const char* tr(const char* en, const char* ru, const char* jp);
extern double mouseX, mouseY;
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


// ----------------------------------------------------------------------
// Реализация коллизий (без изменений)
// ----------------------------------------------------------------------
bool isSolidBlock(int blockId) {
    return blockId != 0 && blockId != 5;
}

inline bool isSolid(int blockId) {
    return isSolidBlockFast[blockId & 0xFF];
}

bool checkPlayerCollision(const glm::vec3& feetPos) {
    float halfWidth = PLAYER_WIDTH * 0.5f;
    glm::vec3 minCorner = feetPos + glm::vec3(-halfWidth, 0.0f, -halfWidth);
    glm::vec3 maxCorner = feetPos + glm::vec3( halfWidth, PLAYER_HEIGHT,  halfWidth);
    
    int minX = static_cast<int>(std::ceil(minCorner.x - 0.5f));
    int maxX = static_cast<int>(std::floor(maxCorner.x + 0.5f));
    int minY = static_cast<int>(std::ceil(minCorner.y - 0.5f));
    int maxY = static_cast<int>(std::floor(maxCorner.y + 0.5f));
    int minZ = static_cast<int>(std::ceil(minCorner.z - 0.5f));
    int maxZ = static_cast<int>(std::floor(maxCorner.z + 0.5f));
    
    for (int x = minX; x <= maxX; ++x) {
        for (int y = minY; y <= maxY; ++y) {
            for (int z = minZ; z <= maxZ; ++z) {
                int blockId = getBlockAt(x, y, z);
                if (isSolid(blockId)) {
                    glm::vec3 blockMin(x - 0.5f, y - 0.5f, z - 0.5f);
                    glm::vec3 blockMax(x + 0.5f, y + 0.5f, z + 0.5f);
                    if (minCorner.x <= blockMax.x && maxCorner.x >= blockMin.x &&
                        minCorner.y <= blockMax.y && maxCorner.y >= blockMin.y &&
                        minCorner.z <= blockMax.z && maxCorner.z >= blockMin.z) {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

bool isOnGroundCheck(const glm::vec3& feetPos) {
    glm::vec3 below = feetPos;
    below.y -= 0.05f;
    return checkPlayerCollision(below);
}

glm::vec3 applyCollision(const glm::vec3& oldFeetPos, const glm::vec3& delta) {
    glm::vec3 newFeetPos = oldFeetPos;
    glm::vec3 actualDelta(0.0f);
    constexpr float COLLISION_STEP = 0.01f;
    
    // Обработка X
    newFeetPos.x += delta.x;
    if (checkPlayerCollision(newFeetPos)) {
        newFeetPos.x = oldFeetPos.x;
        const float direction = (delta.x > 0.0f) ? 1.0f : -1.0f;
        for (float t = COLLISION_STEP; t <= std::abs(delta.x); t += COLLISION_STEP) {
            newFeetPos.x = oldFeetPos.x + direction * t;
            if (checkPlayerCollision(newFeetPos)) {
                newFeetPos.x -= direction * COLLISION_STEP;
                break;
            }
        }
    }
    actualDelta.x = newFeetPos.x - oldFeetPos.x;
    
    // Обработка Z
    newFeetPos.z += delta.z;
    if (checkPlayerCollision(newFeetPos)) {
        newFeetPos.z = oldFeetPos.z;
        const float direction = (delta.z > 0.0f) ? 1.0f : -1.0f;
        for (float t = COLLISION_STEP; t <= std::abs(delta.z); t += COLLISION_STEP) {
            newFeetPos.z = oldFeetPos.z + direction * t;
            if (checkPlayerCollision(newFeetPos)) {
                newFeetPos.z -= direction * COLLISION_STEP;
                break;
            }
        }
    }
    actualDelta.z = newFeetPos.z - oldFeetPos.z;
    
    // Проверяем, были ли мы в воздухе до обработки Y
    bool wasFalling = playerVelocity.y < 0.0f && !wasOnGround;
    
    // Проверяем, находимся ли мы в воде (по голове или ногам)
    bool inWater = isPlayerInWater();
    
    // Обработка Y
    newFeetPos.y += delta.y;
    if (checkPlayerCollision(newFeetPos)) {
        // Урон от падения — ТОЛЬКО если не в воде
        if (wasFalling && fallDistance > MIN_FALL_DAMAGE_HEIGHT && !inWater) {
            float damage = calculateFallDamage(fallDistance);
            if (damage > 0) {
                applyDamage(static_cast<int>(damage));
            }
        }
        
        // Обнуляем вертикальную скорость при столкновении
        if (delta.y > 0.0f) playerVelocity.y = 0.0f;
        else if (delta.y < 0.0f) playerVelocity.y = 0.0f;
        
        // Сброс расстояния падения при приземлении
        fallDistance = 0.0f;
        
        // Точное размещение на поверхности
        newFeetPos.y = oldFeetPos.y;
        const float direction = (delta.y > 0.0f) ? 1.0f : -1.0f;
        for (float t = COLLISION_STEP; t <= std::abs(delta.y); t += COLLISION_STEP) {
            newFeetPos.y = oldFeetPos.y + direction * t;
            if (checkPlayerCollision(newFeetPos)) {
                newFeetPos.y -= direction * COLLISION_STEP;
                break;
            }
        }
    }
    actualDelta.y = newFeetPos.y - oldFeetPos.y;
    
    // Обновление состояния "на земле"
    bool wasOnGroundPrevious = wasOnGround;
    isOnGround = isOnGroundCheck(newFeetPos);
    
    // Отслеживание дистанции падения
    if (!isOnGround && wasOnGroundPrevious) {
        // Начало падения
        lastGroundY = newFeetPos.y;
        fallDistance = 0.0f;
    } else if (!isOnGround && !wasOnGroundPrevious) {
        // В воздухе - обновляем дистанцию падения (но не в воде)
        if (!inWater) {
            fallDistance = lastGroundY - newFeetPos.y;
        }
    } else if (isOnGround) {
        // На земле - сбрасываем
        fallDistance = 0.0f;
        lastGroundY = newFeetPos.y;
    }
    
    wasOnGround = isOnGround;
    return actualDelta;
}

void placePlayerOnGround() {
    glm::vec3 feetPos = cameraPos - glm::vec3(0.0f, EYE_HEIGHT, 0.0f);
    while (feetPos.y > -10.0f && !checkPlayerCollision(feetPos)) {
        feetPos.y -= 0.1f;
    }
    while (feetPos.y < CHUNK_SIZE_Y && checkPlayerCollision(feetPos)) {
        feetPos.y += 0.05f;
    }
    feetPos.y -= 0.05f;
    cameraPos = feetPos + glm::vec3(0.0f, EYE_HEIGHT, 0.0f);
    playerVelocity = glm::vec3(0.0f);
    isOnGround = true;
    wasOnGround = true; // ДОБАВЬТЕ ЭТУ СТРОКУ
    fallDistance = 0.0f; // ДОБАВЬТЕ ЭТУ СТРОКУ
    lastGroundY = feetPos.y; // ДОБАВЬТЕ ЭТУ СТРОКУ
}

// ----------------------------------------------------------------------
// Типы блоков
// ----------------------------------------------------------------------
struct BlockType {
    int id;
    std::string name;
    unsigned int textureID;
    int lightEmission = 0;
    int textureParts = 3;  // Количество частей в текстурной полосе (3, 4 или 6)
};
std::unordered_map<int, BlockType> blockTypes;
int currentBlockType = 1;

struct ItemType {
    int id = 0;
    std::string name;
    std::string displayName;
    int maxStack = 64;
    bool isBlock = true;
    unsigned int textureID = 0;
};
std::unordered_map<int, ItemType> itemTypes;

// ----------------------------------------------------------------------
// Звуки (без изменений)
// ----------------------------------------------------------------------
struct SoundSet {
    std::vector<sf::SoundBuffer> buffers;
    bool empty() const { return buffers.empty(); }
    const sf::SoundBuffer& getRandom() const {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, buffers.size() - 1);
        return buffers[dis(gen)];
    }
};

class SoundManager {
public:
    SoundManager() {
        volumes["hit"] = 100.0f;
        volumes["place"] = 80.0f;
        volumes["step"] = 60.0f;
        volumes["interact"] = 100.0f;
        volumes["hurt"] = 100.0f;
        volumes["ui"] = 90.0f;
    }

    void loadSoundsForBlock(int blockId, const std::string& basePath, const std::string& pattern) {
        std::string fullPattern = basePath + pattern;
        std::regex re;
        std::string regexStr = std::regex_replace(pattern, std::regex("\\*"), ".*");
        re = std::regex(regexStr, std::regex::icase);

        for (const auto& entry : fs::directory_iterator(basePath)) {
            if (entry.is_regular_file()) {
                std::string filename = entry.path().filename().string();
                if (std::regex_match(filename, re)) {
                    sf::SoundBuffer buffer;
                    if (buffer.loadFromFile(entry.path().string())) {
                        soundSets[blockId][getCategoryFromPattern(pattern)].buffers.push_back(std::move(buffer));
                    } else {
                        std::cerr << "Failed to load sound: " << entry.path() << std::endl;
                    }
                }
            }
        }
    }

    void play(int blockId, const std::string& category, bool gameStarted) {
        if (!gameStarted) return;
        auto it = soundSets.find(blockId);
        if (it == soundSets.end()) return;
        auto catIt = it->second.find(category);
        if (catIt == it->second.end() || catIt->second.empty()) return;
    
        static std::vector<std::unique_ptr<sf::Sound>> activeSounds;
        activeSounds.erase(std::remove_if(activeSounds.begin(), activeSounds.end(),
            [](const std::unique_ptr<sf::Sound>& s) { return s->getStatus() == sf::Sound::Status::Stopped; }), activeSounds.end());
        if (activeSounds.size() > 32) return;
    
        auto sound = std::make_unique<sf::Sound>(catIt->second.getRandom());
        sound->setVolume(volumes[category]);
        sound->play();
        activeSounds.push_back(std::move(sound));
    }

    void setVolume(const std::string& category, float vol) {
        volumes[category] = vol;
    }
    void loadPlayerSound(const std::string& category, const std::string& filePath) {
        sf::SoundBuffer buffer;
        if (buffer.loadFromFile(filePath)) {
            playerSounds[category].buffers.push_back(std::move(buffer));
        } else {
            std::cerr << "Failed to load player sound: " << filePath << std::endl;
        }
    }

    // ДОБАВЬТЕ ЭТОТ МЕТОД для воспроизведения звуков игрока
    void playPlayerSound(const std::string& category, bool gameStarted) {
        if (!gameStarted) return;
        auto it = playerSounds.find(category);
        if (it == playerSounds.end() || it->second.empty()) return;
    
        static std::vector<std::unique_ptr<sf::Sound>> activeSounds;
        activeSounds.erase(std::remove_if(activeSounds.begin(), activeSounds.end(),
            [](const std::unique_ptr<sf::Sound>& s) { return s->getStatus() == sf::Sound::Status::Stopped; }), activeSounds.end());
        if (activeSounds.size() > 32) return;
    
        auto sound = std::make_unique<sf::Sound>(it->second.getRandom());
        sound->setVolume(volumes[category]);
        sound->play();
        activeSounds.push_back(std::move(sound));
    }

    void playUISound(const std::string& category) {
        auto it = playerSounds.find(category);
        if (it == playerSounds.end() || it->second.empty()) return;

        static std::vector<std::unique_ptr<sf::Sound>> activeSounds;
        activeSounds.erase(std::remove_if(activeSounds.begin(), activeSounds.end(),
            [](const std::unique_ptr<sf::Sound>& s) { return s->getStatus() == sf::Sound::Status::Stopped; }), activeSounds.end());
        if (activeSounds.size() > 32) return;

        auto sound = std::make_unique<sf::Sound>(it->second.getRandom());
        sound->setVolume(volumes[category]);
        sound->play();
        activeSounds.push_back(std::move(sound));
    }
private:
    std::unordered_map<int, std::unordered_map<std::string, SoundSet>> soundSets;
    std::unordered_map<std::string, SoundSet> playerSounds;
    std::unordered_map<std::string, float> volumes;

    std::string getCategoryFromPattern(const std::string& pattern) {
        if (pattern.find("hit") != std::string::npos) return "hit";
        if (pattern.find("place") != std::string::npos) return "place";
        if (pattern.find("step") != std::string::npos) return "step";
        if (pattern.find("interact") != std::string::npos) return "interact";
        return "hit";
    }
};

SoundManager soundManager;

// ----------------------------------------------------------------------
// Сохранение/загрузка чанков (расширено для освещения)
// ----------------------------------------------------------------------
const std::string SAVES_ROOT_DIR = "saves";

struct WorldSummary {
    std::string folderName;
    std::string displayName;
    std::string gameMode;
    fs::file_time_type lastPlayed{};
};

std::string currentWorldFolderName;
std::string currentWorldDisplayName;
std::string selectedWorldFolderName;
int currentWorldSeed = 0;
std::vector<WorldSummary> availableWorlds;

fs::path getSavesRootPath() {
    return fs::path(SAVES_ROOT_DIR);
}

fs::path getWorldPath(const std::string& folderName) {
    return getSavesRootPath() / folderName;
}

fs::path getCurrentWorldPath() {
    return currentWorldFolderName.empty() ? fs::path() : getWorldPath(currentWorldFolderName);
}

fs::path getChunksPath(const std::string& folderName) {
    return getWorldPath(folderName) / "chunks";
}

fs::path getCurrentChunksPath() {
    return currentWorldFolderName.empty() ? fs::path() : getCurrentWorldPath() / "chunks";
}

fs::path getWorldMetaPath(const std::string& folderName) {
    return getWorldPath(folderName) / "world.json";
}

fs::path getCurrentWorldMetaPath() {
    return currentWorldFolderName.empty() ? fs::path() : getCurrentWorldPath() / "world.json";
}

struct ChunkHeader {
    char magic[4];
    uint32_t version;
    int32_t cx;
    int32_t cz;
    uint32_t timestamp;
    uint32_t blockCount;
    uint32_t reserved;
};

struct PackedBlock {
    uint16_t x;
    uint16_t y;
    uint16_t z;
    uint8_t id;
    uint8_t skyLight;
    uint8_t blockLight;
};

struct hash_ivec2 {
    size_t operator()(const glm::ivec2& v) const {
        return ((v.x * 73856093) ^ (v.y * 19349663));
    }
};

struct ChunkData {
    glm::ivec2 pos;
    std::vector<int> blocks;
    std::vector<uint8_t> skyLight;
    std::vector<uint8_t> blockLight;
    bool valid = false;
    bool lightDirty = false;
    ChunkData(int cx, int cz) : pos(cx, cz) {
        blocks.resize(CHUNK_VOLUME, 0);
        skyLight.resize(CHUNK_VOLUME, 0);
        blockLight.resize(CHUNK_VOLUME, 0);
    }
};

int generateWorldSeed() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(1, 999999);
    return dis(gen);
}

void resetPlayerStateForNewWorld() {
    cameraPos = glm::vec3(0.0f, 200.0f, 0.0f);
    yaw = -90.0f;
    pitch = 0.0f;
    cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
    playerVelocity = glm::vec3(0.0f);
    isOnGround = false;
    wasOnGround = false;
    fallDistance = 0.0f;
    lastGroundY = 0.0f;
    invulnerabilityTimer = 0.0f;
    playerHealth = MAX_PLAYER_HEALTH;
    firstMouse = true;
}

bool saveCurrentWorldMetadata() {
    if (currentWorldFolderName.empty()) return false;

    fs::path worldPath = getCurrentWorldPath();
    fs::create_directories(worldPath);
    fs::create_directories(getCurrentChunksPath());

    json metadata;
    metadata["name"] = currentWorldDisplayName.empty() ? "New World" : currentWorldDisplayName;
    metadata["folder"] = currentWorldFolderName;
    metadata["seed"] = currentWorldSeed;
    metadata["gameMode"] = "survival";
    metadata["lastPlayed"] = static_cast<int64_t>(std::time(nullptr));
    metadata["player"] = {
        {"x", cameraPos.x},
        {"y", cameraPos.y},
        {"z", cameraPos.z},
        {"yaw", yaw},
        {"pitch", pitch},
        {"health", playerHealth}
    };

    std::ofstream out(getCurrentWorldMetaPath());
    if (!out.is_open()) return false;
    out << metadata.dump(2);
    return true;
}

bool loadWorldMetadata(const std::string& folderName, bool applyPlayerState) {
    fs::path metadataPath = getWorldMetaPath(folderName);
    if (!fs::exists(metadataPath)) return false;

    std::ifstream in(metadataPath);
    if (!in.is_open()) return false;

    json metadata = json::parse(in, nullptr, false);
    if (metadata.is_discarded()) return false;

    currentWorldFolderName = folderName;
    currentWorldDisplayName = metadata.value("name", folderName);
    currentWorldSeed = metadata.value("seed", generateWorldSeed());

    if (applyPlayerState && metadata.contains("player")) {
        const auto& player = metadata["player"];
        cameraPos.x = player.value("x", 0.0f);
        cameraPos.y = player.value("y", 200.0f);
        cameraPos.z = player.value("z", 0.0f);
        yaw = player.value("yaw", -90.0f);
        pitch = player.value("pitch", 0.0f);
        cameraFront = glm::normalize(glm::vec3(
            cos(glm::radians(yaw)) * cos(glm::radians(pitch)),
            sin(glm::radians(pitch)),
            sin(glm::radians(yaw)) * cos(glm::radians(pitch))
        ));
        playerHealth = player.value("health", MAX_PLAYER_HEALTH);
        playerVelocity = glm::vec3(0.0f);
        isOnGround = false;
        wasOnGround = false;
        fallDistance = 0.0f;
        invulnerabilityTimer = 0.0f;
        firstMouse = true;
    }

    return true;
}

std::string makeUniqueWorldFolderName() {
    fs::create_directories(getSavesRootPath());
    int index = 1;
    while (true) {
        std::string folderName = "world_" + std::to_string(index);
        if (!fs::exists(getWorldPath(folderName))) return folderName;
        ++index;
    }
}

std::string makeDefaultWorldDisplayName() {
    int index = 1;
    while (true) {
        std::string displayName = (index == 1) ? "New World" : "New World (" + std::to_string(index) + ")";
        bool found = false;
        for (const auto& world : availableWorlds) {
            if (world.displayName == displayName) {
                found = true;
                break;
            }
        }
        if (!found) return displayName;
        ++index;
    }
}

void saveChunkToFile(const fs::path& chunksPath, const glm::ivec2& pos, const std::vector<int>& blocks,
                     const std::vector<uint8_t>& skyLight, const std::vector<uint8_t>& blockLight) {
    if (chunksPath.empty()) return;
    if (!fs::exists(chunksPath)) fs::create_directories(chunksPath);

    fs::path filename = chunksPath / ("chunk_" + std::to_string(pos.x) + "_" + std::to_string(pos.y) + ".bin");
    std::ofstream f(filename, std::ios::binary);
    if (!f.is_open()) return;
    
    std::vector<PackedBlock> packedBlocks;
    packedBlocks.reserve(CHUNK_VOLUME / 4);
    for (int x = 0; x < CHUNK_SIZE_X; ++x)
        for (int y = 0; y < CHUNK_SIZE_Y; ++y)
            for (int z = 0; z < CHUNK_SIZE_Z; ++z) {
                int idx = (x * CHUNK_SIZE_Y + y) * CHUNK_SIZE_Z + z;
                int id = blocks[idx];
                if (id != 0 || skyLight[idx] != 0 || blockLight[idx] != 0) {
                    PackedBlock pb;
                    pb.x = static_cast<uint16_t>(x);
                    pb.y = static_cast<uint16_t>(y);
                    pb.z = static_cast<uint16_t>(z);
                    pb.id = static_cast<uint8_t>(id);
                    pb.skyLight = skyLight[idx];
                    pb.blockLight = blockLight[idx];
                    packedBlocks.push_back(pb);
                }
            }
    
    ChunkHeader header;
    header.magic[0] = 'V'; header.magic[1] = 'O'; header.magic[2] = 'X'; header.magic[3] = 'C';
    header.version = 3; // версия с блочным светом
    header.cx = pos.x;
    header.cz = pos.y;
    header.timestamp = static_cast<uint32_t>(std::time(nullptr));
    header.blockCount = static_cast<uint32_t>(packedBlocks.size());
    header.reserved = 0;
    
    f.write(reinterpret_cast<const char*>(&header), sizeof(header));
    if (!packedBlocks.empty())
        f.write(reinterpret_cast<const char*>(packedBlocks.data()), packedBlocks.size() * sizeof(PackedBlock));
    f.close();
}

std::shared_ptr<ChunkData> loadChunkFromFile(int cx, int cz) {
    auto data = std::make_shared<ChunkData>(cx, cz);
    fs::path filename = getCurrentChunksPath() / ("chunk_" + std::to_string(cx) + "_" + std::to_string(cz) + ".bin");
    std::ifstream f(filename, std::ios::binary);
    if (!f.is_open()) return nullptr;
    
    ChunkHeader header;
    f.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (header.magic[0] != 'V' || header.magic[1] != 'O' || header.magic[2] != 'X' || header.magic[3] != 'C') return nullptr;
    if (header.version != 3) return nullptr; // только версия 3 поддерживает блочный свет
    
    std::fill(data->blocks.begin(), data->blocks.end(), 0);
    std::fill(data->skyLight.begin(), data->skyLight.end(), 0);
    std::fill(data->blockLight.begin(), data->blockLight.end(), 0);
    if (header.blockCount > 0) {
        std::vector<PackedBlock> packedBlocks(header.blockCount);
        f.read(reinterpret_cast<char*>(packedBlocks.data()), header.blockCount * sizeof(PackedBlock));
        for (const auto& pb : packedBlocks) {
            if (pb.x < CHUNK_SIZE_X && pb.y < CHUNK_SIZE_Y && pb.z < CHUNK_SIZE_Z) {
                int idx = (pb.x * CHUNK_SIZE_Y + pb.y) * CHUNK_SIZE_Z + pb.z;
                data->blocks[idx] = pb.id;
                data->skyLight[idx] = pb.skyLight;
                data->blockLight[idx] = pb.blockLight;
            }
        }
    }
    data->valid = true;
    return data;
}

void deleteOldWorld() {
    fs::path currentWorldPath = getCurrentWorldPath();
    if (!currentWorldPath.empty() && fs::exists(currentWorldPath)) {
        fs::remove_all(currentWorldPath);
    }
}

void initWorldNoise() {
    if (currentWorldSeed == 0) {
        currentWorldSeed = generateWorldSeed();
    }
    int seed = currentWorldSeed;
    
    continentNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    continentNoise.SetFrequency(0.0008f);
    continentNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    continentNoise.SetFractalOctaves(6);
    continentNoise.SetFractalLacunarity(2.0f);
    continentNoise.SetFractalGain(0.5f);
    continentNoise.SetSeed(seed);

    erosionNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    erosionNoise.SetFrequency(0.008f);
    erosionNoise.SetFractalOctaves(4);
    erosionNoise.SetFractalGain(0.5f);
    erosionNoise.SetSeed(seed + 1);

    mountainNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    mountainNoise.SetFrequency(0.004f);
    mountainNoise.SetFractalOctaves(7);
    mountainNoise.SetFractalGain(0.6f);
    mountainNoise.SetFractalLacunarity(2.2f);
    mountainNoise.SetSeed(seed + 2);

    riverNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    riverNoise.SetFrequency(0.02f);
    riverNoise.SetFractalOctaves(2);
    riverNoise.SetSeed(seed + 3);

    biomeTempNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    biomeTempNoise.SetFrequency(0.0006f);
    biomeTempNoise.SetFractalOctaves(3);
    biomeTempNoise.SetSeed(seed + 4);
    
    biomeHumidNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    biomeHumidNoise.SetFrequency(0.0006f);
    biomeHumidNoise.SetFractalOctaves(3);
    biomeHumidNoise.SetSeed(seed + 5);

    detailNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    detailNoise.SetFrequency(0.04f);
    detailNoise.SetFractalOctaves(3);
    detailNoise.SetSeed(seed + 6);

    treeNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    treeNoise.SetFrequency(0.08f);
    treeNoise.SetSeed(seed + 7);

    seaLevelNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    seaLevelNoise.SetFrequency(0.0003f);
    seaLevelNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    seaLevelNoise.SetFractalOctaves(4);
    seaLevelNoise.SetFractalLacunarity(2.0f);
    seaLevelNoise.SetFractalGain(0.5f);
    seaLevelNoise.SetSeed(seed + 8);

    transitionNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    transitionNoise.SetFrequency(0.002f);
    transitionNoise.SetFractalOctaves(2);
    transitionNoise.SetSeed(seed + 9);
}

// ----------------------------------------------------------------------
// НОВАЯ СИСТЕМА ОСВЕЩЕНИЯ (BFS) - ИСПРАВЛЕННАЯ (ДОБАВЛЕНО ОБНОВЛЕНИЕ НЕБЕСНОГО СВЕТА)
// ----------------------------------------------------------------------
const int MAX_LIGHT = 15;

// Проверка, является ли блок НЕПРОЗРАЧНЫМ (полностью блокирует свет)
bool isOpaque(int blockId) {
    if (blockId == 0) return false;  // воздух
    if (blockId == 5) return false;  // вода
    if (blockId == 7) return false;  // листва
    return true;
}

// Ослабление света при прохождении через блок
int getLightOpacity(int blockId) {
    if (blockId == 0) return 0;
    if (blockId == 5) return 1;
    if (blockId == 7) return 1;
    return MAX_LIGHT + 1;  // твёрдые блоки полностью блокируют
}

// Глобальный доступ к свету
uint8_t getSkyLightAt(int wx, int wy, int wz);
uint8_t getBlockLightAt(int wx, int wy, int wz);
void setSkyLightAt(int wx, int wy, int wz, uint8_t value);
void setBlockLightAt(int wx, int wy, int wz, uint8_t value);

struct LightNode {
    int x, y, z;
    int sourceX, sourceY, sourceZ;
    int radius;
    uint8_t light;
};

struct LightRegion {
    int minX, maxX;
    int minY, maxY;
    int minZ, maxZ;
};

uint8_t computeSphericalBlockLight(int x, int y, int z, int sourceX, int sourceY, int sourceZ, int radius) {
    if (radius <= 0) return 0;

    float dx = static_cast<float>(x - sourceX);
    float dy = static_cast<float>(y - sourceY);
    float dz = static_cast<float>(z - sourceZ);
    float distance = std::sqrt(dx * dx + dy * dy + dz * dz);

    if (distance > static_cast<float>(radius)) return 0;

    float factor = (static_cast<float>(radius) + 1.0f - distance) / (static_cast<float>(radius) + 1.0f);
    factor = glm::clamp(factor, 0.0f, 1.0f);
    return static_cast<uint8_t>(std::round(factor * MAX_LIGHT));
}

bool isInsideLightRegion(const LightRegion& region, int x, int y, int z) {
    return x >= region.minX && x <= region.maxX &&
           y >= region.minY && y <= region.maxY &&
           z >= region.minZ && z <= region.maxZ;
}

void rebuildSkyLightColumnBase(int x, int z) {
    bool blockedAbove = false;
    for (int y = CHUNK_SIZE_Y - 1; y >= 0; --y) {
        int blockId = getBlockAt(x, y, z);
        if (blockedAbove || isOpaque(blockId)) {
            setSkyLightAt(x, y, z, 0);
            if (isOpaque(blockId)) {
                blockedAbove = true;
            }
        } else {
            setSkyLightAt(x, y, z, static_cast<uint8_t>(MAX_LIGHT));
        }
    }
}

void smoothSkyLightColumn(int x, int z) {
    bool blockedAbove = false;

    for (int y = CHUNK_SIZE_Y - 1; y >= 0; --y) {
        int blockId = getBlockAt(x, y, z);
        if (isOpaque(blockId)) {
            blockedAbove = true;
            setSkyLightAt(x, y, z, 0);
            continue;
        }

        if (blockedAbove) {
            continue;
        }

        uint8_t currentLight = getSkyLightAt(x, y, z);
        uint8_t smoothedLight = currentLight;

        uint8_t neighborLight = getSkyLightAt(x + 1, y, z);
        if (neighborLight > 0) {
            smoothedLight = std::max(smoothedLight, static_cast<uint8_t>(neighborLight - 1));
        }

        neighborLight = getSkyLightAt(x - 1, y, z);
        if (neighborLight > 0) {
            smoothedLight = std::max(smoothedLight, static_cast<uint8_t>(neighborLight - 1));
        }

        neighborLight = getSkyLightAt(x, y, z + 1);
        if (neighborLight > 0) {
            smoothedLight = std::max(smoothedLight, static_cast<uint8_t>(neighborLight - 1));
        }

        neighborLight = getSkyLightAt(x, y, z - 1);
        if (neighborLight > 0) {
            smoothedLight = std::max(smoothedLight, static_cast<uint8_t>(neighborLight - 1));
        }

        setSkyLightAt(x, y, z, smoothedLight);
    }
}

void updateSkyLightColumnsAround(int x, int z) {
    const int offsets[5][2] = {
        {0, 0},
        {1, 0},
        {-1, 0},
        {0, 1},
        {0, -1}
    };

    for (int i = 0; i < 5; ++i) {
        rebuildSkyLightColumnBase(x + offsets[i][0], z + offsets[i][1]);
    }

    for (int i = 0; i < 5; ++i) {
        smoothSkyLightColumn(x + offsets[i][0], z + offsets[i][1]);
    }
}

// Глобальная BFS для блочного света
void propagateBlockLightGlobal(std::queue<LightNode>& queue) {
    const int dirs[6][3] = {
        {1,0,0}, {-1,0,0},
        {0,1,0}, {0,-1,0},
        {0,0,1}, {0,0,-1}
    };

    const int MAX_ITERATIONS = 50000;
    int iterations = 0;

    while (!queue.empty() && iterations < MAX_ITERATIONS) {
        iterations++;
        LightNode node = queue.front();
        queue.pop();

        if (node.light <= 1) continue;
        uint8_t propagatedLight = static_cast<uint8_t>(node.light - 1);

        for (int i = 0; i < 6; ++i) {
            int nx = node.x + dirs[i][0];
            int ny = node.y + dirs[i][1];
            int nz = node.z + dirs[i][2];
            
            if (ny < 0 || ny >= CHUNK_SIZE_Y) continue;

            int blockId = getBlockAt(nx, ny, nz);
            if (isOpaque(blockId)) continue;

            uint8_t current = getBlockLightAt(nx, ny, nz);
            uint8_t newLight = propagatedLight;

            if (newLight > current && newLight > 0) {
                setBlockLightAt(nx, ny, nz, newLight);
                queue.push({nx, ny, nz, node.sourceX, node.sourceY, node.sourceZ, node.radius, newLight});
            }
        }
    }
}

// Добавление блочного света
void addBlockLightGlobal(int x, int y, int z, uint8_t light) {
    if (light == 0) return;
    std::queue<LightNode> queue;
    uint8_t sourceLight = std::min<uint8_t>(light, static_cast<uint8_t>(MAX_LIGHT));
    setBlockLightAt(x, y, z, sourceLight);
    queue.push({x, y, z, x, y, z, light, sourceLight});
    propagateBlockLightGlobal(queue);
}

void propagateBlockLightInRegion(std::queue<LightNode>& queue, const LightRegion& region) {
    const int dirs[6][3] = {
        {1,0,0}, {-1,0,0},
        {0,1,0}, {0,-1,0},
        {0,0,1}, {0,0,-1}
    };

    const int MAX_ITERATIONS = 50000;
    int iterations = 0;

    while (!queue.empty() && iterations < MAX_ITERATIONS) {
        iterations++;
        LightNode node = queue.front();
        queue.pop();

        if (node.light <= 1) continue;
        uint8_t propagatedLight = static_cast<uint8_t>(node.light - 1);

        for (int i = 0; i < 6; ++i) {
            int nx = node.x + dirs[i][0];
            int ny = node.y + dirs[i][1];
            int nz = node.z + dirs[i][2];

            if (!isInsideLightRegion(region, nx, ny, nz)) continue;
            if (ny < 0 || ny >= CHUNK_SIZE_Y) continue;

            int blockId = getBlockAt(nx, ny, nz);
            if (isOpaque(blockId)) continue;

            uint8_t current = getBlockLightAt(nx, ny, nz);
            uint8_t newLight = propagatedLight;

            if (newLight > current && newLight > 0) {
                setBlockLightAt(nx, ny, nz, newLight);
                queue.push({nx, ny, nz, node.sourceX, node.sourceY, node.sourceZ, node.radius, newLight});
            }
        }
    }
}

void addBlockLightInRegion(int x, int y, int z, uint8_t radius, const LightRegion& region) {
    if (radius == 0) return;

    std::queue<LightNode> queue;
    if (isInsideLightRegion(region, x, y, z)) {
        uint8_t current = getBlockLightAt(x, y, z);
        uint8_t sourceLight = std::min<uint8_t>(radius, static_cast<uint8_t>(MAX_LIGHT));
        if (sourceLight > current) {
            setBlockLightAt(x, y, z, sourceLight);
        }
    }

    uint8_t sourceLight = std::min<uint8_t>(radius, static_cast<uint8_t>(MAX_LIGHT));
    queue.push({x, y, z, x, y, z, radius, sourceLight});
    propagateBlockLightInRegion(queue, region);
}

void rebuildBlockLightRegion(const LightRegion& rawRegion);

// Обновление небесного света при изменении блока
void updateSkyLightAt(int x, int y, int z) {
    updateSkyLightColumnsAround(x, z);
}

// Обработчик изменения блока - ТЕПЕРЬ ОБНОВЛЯЕТ И НЕБЕСНЫЙ СВЕТ ТОЖЕ
void onBlockChangedGlobal(int x, int y, int z) {
    updateSkyLightAt(x, y, z);

    LightRegion region;
    region.minX = x - maxBlockLightRadius;
    region.maxX = x + maxBlockLightRadius;
    region.minY = std::max(0, y - maxBlockLightRadius);
    region.maxY = std::min(CHUNK_SIZE_Y - 1, y + maxBlockLightRadius);
    region.minZ = z - maxBlockLightRadius;
    region.maxZ = z + maxBlockLightRadius;
    rebuildBlockLightRegion(region);
}

// Небесный свет: BFS после вертикального прохода (для генерации чанков)
void floodSkyLight(std::queue<LightNode>& queue) {
    while (!queue.empty()) {
        queue.pop();
    }
}

void smoothSkyLightHorizontally(ChunkData& chunk) {
    const std::vector<uint8_t> baseSkyLight = chunk.skyLight;

    for (int x = 0; x < CHUNK_SIZE_X; ++x) {
        for (int z = 0; z < CHUNK_SIZE_Z; ++z) {
            bool blockedAbove = false;

            for (int y = CHUNK_SIZE_Y - 1; y >= 0; --y) {
                int idx = (x * CHUNK_SIZE_Y + y) * CHUNK_SIZE_Z + z;

                if (isOpaque(chunk.blocks[idx])) {
                    blockedAbove = true;
                    chunk.skyLight[idx] = 0;
                    continue;
                }

                if (blockedAbove) {
                    chunk.skyLight[idx] = baseSkyLight[idx];
                    continue;
                }

                uint8_t currentLight = baseSkyLight[idx];
                uint8_t smoothedLight = currentLight;

                if (x + 1 < CHUNK_SIZE_X) {
                    int nidx = ((x + 1) * CHUNK_SIZE_Y + y) * CHUNK_SIZE_Z + z;
                    uint8_t neighborLight = baseSkyLight[nidx];
                    if (neighborLight > 0) {
                        smoothedLight = std::max(smoothedLight, static_cast<uint8_t>(neighborLight - 1));
                    }
                }

                if (x - 1 >= 0) {
                    int nidx = ((x - 1) * CHUNK_SIZE_Y + y) * CHUNK_SIZE_Z + z;
                    uint8_t neighborLight = baseSkyLight[nidx];
                    if (neighborLight > 0) {
                        smoothedLight = std::max(smoothedLight, static_cast<uint8_t>(neighborLight - 1));
                    }
                }

                if (z + 1 < CHUNK_SIZE_Z) {
                    int nidx = (x * CHUNK_SIZE_Y + y) * CHUNK_SIZE_Z + (z + 1);
                    uint8_t neighborLight = baseSkyLight[nidx];
                    if (neighborLight > 0) {
                        smoothedLight = std::max(smoothedLight, static_cast<uint8_t>(neighborLight - 1));
                    }
                }

                if (z - 1 >= 0) {
                    int nidx = (x * CHUNK_SIZE_Y + y) * CHUNK_SIZE_Z + (z - 1);
                    uint8_t neighborLight = baseSkyLight[nidx];
                    if (neighborLight > 0) {
                        smoothedLight = std::max(smoothedLight, static_cast<uint8_t>(neighborLight - 1));
                    }
                }

                chunk.skyLight[idx] = smoothedLight;
            }
        }
    }
}

// ----------------------------------------------------------------------
// Генерация мира с освещением
// ----------------------------------------------------------------------
float getHeightAt(int wx, int wz, float& outBiomeTemp, float& outBiomeHumid, float& outWaterLevel) {
    outBiomeTemp = biomeTempNoise.GetNoise((float)wx, (float)wz);
    outBiomeHumid = biomeHumidNoise.GetNoise((float)wx, (float)wz);

    float continent = continentNoise.GetNoise((float)wx, (float)wz);
    float seaNoise = seaLevelNoise.GetNoise((float)wx, (float)wz);
    outWaterLevel = 62.0f + seaNoise * 8.0f;
    outWaterLevel = glm::clamp(outWaterLevel, 45.0f, 75.0f);

    float t = (continent + 1.0f) * 0.5f;
    t = glm::smoothstep(0.0f, 1.0f, t);
    
    float minHeightOffset = -12.0f;
    float maxHeightOffset = 40.0f;
    float heightOffset = glm::mix(minHeightOffset, maxHeightOffset, t);
    
    float mountain = 0.0f;
    if (continent > 0.35f) {
        float mountainFactor = (continent - 0.35f) / 0.65f;
        mountain = mountainNoise.GetNoise((float)wx, (float)wz) * 18.0f * mountainFactor;
    }

    float erosion = erosionNoise.GetNoise((float)wx, (float)wz) * 4.0f;
    float detail = detailNoise.GetNoise((float)wx, (float)wz) * 1.5f;

    float river = riverNoise.GetNoise((float)wx, (float)wz);
    float riverFactor = 0.0f;
    if (std::abs(river) < 0.15f) {
        float dist = 1.0f - (std::abs(river) / 0.15f);
        riverFactor = -6.0f * dist * dist;
    }

    float height = outWaterLevel + heightOffset + mountain + erosion + detail + riverFactor;

    float minH = 1.0f;
    float maxH = CHUNK_SIZE_Y - 8.0f;
    if (height < minH) height = minH + (height - minH) * 0.5f;
    if (height > maxH) height = maxH - (maxH - height) * 0.5f;
    height = glm::clamp(height, minH, maxH);

    return height;
}

int getBiome(float temp, float humid, float height, float waterLevel) {
    if (height <= waterLevel) {
        float depth = waterLevel - height;
        if (depth < 2.0f) return 6;
        if (depth < 8.0f) return 4;
        return 5;
    }
    if (height - waterLevel < 3.0f) return 6;

    float t = (temp + 1.0f) * 0.5f;
    float h = (humid + 1.0f) * 0.5f;

    if (t < 0.2f) return 7;
    if (t > 0.7f && h > 0.5f) return 1;
    if (h < 0.3f) return 8;
    if (height > waterLevel + 25.0f) return 2;
    return 0;
}

bool isTreeNearby(int lx, int lz, int surfaceY, const std::vector<int>& blocks) {
    for (int dx = -3; dx <= 3; ++dx)
        for (int dz = -3; dz <= 3; ++dz) {
            int x = lx + dx, z = lz + dz;
            if (x < 0 || x >= CHUNK_SIZE_X || z < 0 || z >= CHUNK_SIZE_Z) continue;
            for (int y = std::max(0, surfaceY - 1); y <= std::min(CHUNK_SIZE_Y - 1, surfaceY + 7); ++y) {
                if (y >= CHUNK_SIZE_Y) continue;
                int blockId = blocks[(x * CHUNK_SIZE_Y + y) * CHUNK_SIZE_Z + z];
                if (blockId == 6 || blockId == 7) return true;
            }
        }
    return false;
}

void addTree(int cx, int cz, int lx, int lz, int surfaceY, std::vector<int>& blocks) {
    int worldX = cx * CHUNK_SIZE_X + lx, worldZ = cz * CHUNK_SIZE_Z + lz;
    float treeRand = treeNoise.GetNoise((float)worldX, (float)worldZ);
    if (treeRand < 0.65f) return;

    // Дерево должно целиком помещаться в чанке, иначе на границе крона обрежется.
    constexpr int TREE_EDGE_MARGIN = 2;
    if (lx < TREE_EDGE_MARGIN || lx >= CHUNK_SIZE_X - TREE_EDGE_MARGIN ||
        lz < TREE_EDGE_MARGIN || lz >= CHUNK_SIZE_Z - TREE_EDGE_MARGIN) {
        return;
    }

    if (isTreeNearby(lx, lz, surfaceY, blocks)) return;

    const int trunkHeight = 6;
    const int topY = surfaceY + trunkHeight;

    auto setBlock = [&](int x, int y, int z, int type) {
        if (x < 0 || x >= CHUNK_SIZE_X || z < 0 || z >= CHUNK_SIZE_Z || y < 0 || y >= CHUNK_SIZE_Y) return;
        int idx = (x * CHUNK_SIZE_Y + y) * CHUNK_SIZE_Z + z;
        if (blocks[idx] == 0) blocks[idx] = type;
    };

    if (topY + 2 >= CHUNK_SIZE_Y) return;

    auto getBlock = [&](int x, int y, int z) -> int {
        if (x < 0 || x >= CHUNK_SIZE_X || z < 0 || z >= CHUNK_SIZE_Z || y < 0 || y >= CHUNK_SIZE_Y) return BLOCK_UNKNOWN;
        return blocks[(x * CHUNK_SIZE_Y + y) * CHUNK_SIZE_Z + z];
    };

    // Дереву нужна относительно ровная площадка, иначе оно уходит в склон.
    for (int dx = -2; dx <= 2; ++dx) {
        for (int dz = -2; dz <= 2; ++dz) {
            int groundBlock = getBlock(lx + dx, surfaceY, lz + dz);
            int aboveBlock = getBlock(lx + dx, surfaceY + 1, lz + dz);
            if (groundBlock == BLOCK_UNKNOWN || aboveBlock == BLOCK_UNKNOWN) return;
            if (std::abs(dx) <= 1 && std::abs(dz) <= 1 && groundBlock == 0) return;
            if (aboveBlock != 0) return;
        }
    }

    // Проверяем, что весь объём ствола и кроны свободен.
    for (int y = surfaceY + 1; y <= topY + 1; ++y) {
        int radius = 0;
        if (y == topY || y == topY - 1) radius = 1;
        else if (y == topY - 2 || y == topY - 3) radius = 2;

        for (int dx = -radius; dx <= radius; ++dx) {
            for (int dz = -radius; dz <= radius; ++dz) {
                if (dx == 0 && dz == 0 && y <= topY) continue;
                if (getBlock(lx + dx, y, lz + dz) != 0) return;
            }
        }
    }

    for (int h = 1; h <= trunkHeight; ++h) {
        int y = surfaceY + h;
        int idx = (lx * CHUNK_SIZE_Y + y) * CHUNK_SIZE_Z + lz;
        if (blocks[idx] == 0) blocks[idx] = 6;
    }

    auto addLeafSquare = [&](int y, int radius) {
        if (y < 0 || y >= CHUNK_SIZE_Y) return;
        for (int dx = -radius; dx <= radius; ++dx) {
            for (int dz = -radius; dz <= radius; ++dz) {
                if (dx == 0 && dz == 0) continue;
                setBlock(lx + dx, y, lz + dz, 7);
            }
        }
    };

    setBlock(lx, topY + 1, lz, 7);

    setBlock(lx + 1, topY, lz, 7);
    setBlock(lx - 1, topY, lz, 7);
    setBlock(lx, topY, lz + 1, 7);
    setBlock(lx, topY, lz - 1, 7);

    setBlock(lx + 1, topY - 1, lz, 7);
    setBlock(lx - 1, topY - 1, lz, 7);
    setBlock(lx, topY - 1, lz + 1, 7);
    setBlock(lx, topY - 1, lz - 1, 7);

    addLeafSquare(topY - 2, 2);
    addLeafSquare(topY - 3, 2);
}

struct ColumnInfo { float height, biomeTemp, biomeHumid, waterLevel; int biome; };

std::shared_ptr<ChunkData> generateChunk(int cx, int cz) {
    auto data = std::make_shared<ChunkData>(cx, cz);
    ColumnInfo columns[CHUNK_SIZE_X][CHUNK_SIZE_Z];
    for (int x = 0; x < CHUNK_SIZE_X; ++x)
        for (int z = 0; z < CHUNK_SIZE_Z; ++z) {
            int worldX = cx * CHUNK_SIZE_X + x, worldZ = cz * CHUNK_SIZE_Z + z;
            float temp, humid, water;
            float landHeight = getHeightAt(worldX, worldZ, temp, humid, water);
            int waterSurfaceY = (int)water, surfaceY = (int)landHeight;
            surfaceY = std::clamp(surfaceY, 0, CHUNK_SIZE_Y - 1);
            waterSurfaceY = std::clamp(waterSurfaceY, 0, CHUNK_SIZE_Y - 1);
            int biome = getBiome(temp, humid, surfaceY, waterSurfaceY);
            columns[x][z] = {landHeight, temp, humid, water, biome};
        }
    
    for (int x = 0; x < CHUNK_SIZE_X; ++x)
        for (int z = 0; z < CHUNK_SIZE_Z; ++z) {
            const auto& col = columns[x][z];
            int surfaceY = (int)col.height, waterSurfaceY = (int)col.waterLevel, biome = col.biome;
            for (int y = 0; y < CHUNK_SIZE_Y; ++y) {
                int blockId = 0;
                if (biome == 3 || biome == 4 || biome == 5) {
                    if (y == surfaceY) blockId = (biome == 5) ? 3 : 4;
                    else if (y > surfaceY && y <= waterSurfaceY) blockId = 5;
                    else if (y < surfaceY) blockId = (y > surfaceY - 4) ? 4 : 3;
                } else if (biome == 6) {
                    if (y == surfaceY) blockId = 4;
                    else if (y < surfaceY) blockId = (y > surfaceY - 3) ? 4 : 3;
                } else {
                    if (y == surfaceY) {
                        if (biome == 2 || biome == 7) blockId = 3;
                        else if (biome == 8) blockId = 4;
                        else blockId = 1;
                    } else if (y > surfaceY - 4 && y < surfaceY) {
                        if (biome == 2 || biome == 7) blockId = 3;
                        else if (biome == 8) blockId = 4;
                        else blockId = 2;
                    } else if (y < surfaceY - 4) blockId = 3;
                }
                data->blocks[(x * CHUNK_SIZE_Y + y) * CHUNK_SIZE_Z + z] = blockId;
            }
        }
    
    for (int x = 0; x < CHUNK_SIZE_X; ++x)
        for (int z = 0; z < CHUNK_SIZE_Z; ++z) {
            const auto& col = columns[x][z];
            float landHeight = col.height, waterLevel = col.waterLevel;
            int surfaceY = (int)landHeight;
            bool hasWaterNeighbor = false;
            for (int dx = -1; dx <= 1 && !hasWaterNeighbor; ++dx)
                for (int dz = -1; dz <= 1; ++dz) {
                    if (dx == 0 && dz == 0) continue;
                    int nx = x + dx, nz = z + dz;
                    if (nx >= 0 && nx < CHUNK_SIZE_X && nz >= 0 && nz < CHUNK_SIZE_Z)
                        if (columns[nx][nz].height <= columns[nx][nz].waterLevel)
                            { hasWaterNeighbor = true; break; }
                }
            if (hasWaterNeighbor && landHeight > waterLevel && landHeight - waterLevel < 4.0f) {
                int idx = (x * CHUNK_SIZE_Y + surfaceY) * CHUNK_SIZE_Z + z;
                if (data->blocks[idx] == 1 || data->blocks[idx] == 2) data->blocks[idx] = 4;
            }
        }
    
    // ----- НАЧАЛЬНАЯ УСТАНОВКА ОСВЕЩЕНИЯ (только вертикальный проход) -----
    // 1. Вертикальный проход небесного света
    for (int x = 0; x < CHUNK_SIZE_X; ++x)
        for (int z = 0; z < CHUNK_SIZE_Z; ++z) {
            const auto& col = columns[x][z];
            int surfaceY = (int)col.height;
            int waterSurfaceY = (int)col.waterLevel;
            int biome = col.biome;
            if ((biome == 0 || biome == 1) && surfaceY > waterSurfaceY + 2)
                addTree(cx, cz, x, z, surfaceY, data->blocks);
        }

    for (int x = 0; x < CHUNK_SIZE_X; ++x) {
        for (int z = 0; z < CHUNK_SIZE_Z; ++z) {
            bool blockedAbove = false;
            for (int y = CHUNK_SIZE_Y - 1; y >= 0; --y) {
                int idx = (x * CHUNK_SIZE_Y + y) * CHUNK_SIZE_Z + z;
                if (blockedAbove || isOpaque(data->blocks[idx])) {
                    data->skyLight[idx] = 0;
                    if (isOpaque(data->blocks[idx])) {
                        blockedAbove = true;
                    }
                } else {
                    data->skyLight[idx] = MAX_LIGHT;
                }
            }
        }
    }

    smoothSkyLightHorizontally(*data);

    // 2. Источники блочного света записываем, но BFS не запускаем
    for (int x = 0; x < CHUNK_SIZE_X; ++x) {
        for (int y = 0; y < CHUNK_SIZE_Y; ++y) {
            for (int z = 0; z < CHUNK_SIZE_Z; ++z) {
                int idx = (x * CHUNK_SIZE_Y + y) * CHUNK_SIZE_Z + z;
                int blockId = data->blocks[idx];
                int emission = (blockId >= 0 && blockId < 256) ? blockLightEmission[blockId] : 0;
                if (emission > 0) {
                    data->blockLight[idx] = emission;
                }
            }
        }
    }

    data->valid = true;
    return data;
}

// ----------------------------------------------------------------------
// Фоновые задачи
// ----------------------------------------------------------------------
std::mutex chunkMutex;
std::unordered_map<glm::ivec2, std::shared_ptr<ChunkData>, hash_ivec2> pendingData;
std::unordered_set<glm::ivec2, hash_ivec2> pendingLoad;
std::unordered_set<glm::ivec2, hash_ivec2> pendingGen;
std::atomic<bool> workerRunning(true);
std::atomic<bool> fastChunkLoadingMode(false);
std::thread workerThread;

void workerFunction() {
    while (workerRunning) {
        int processed = 0;
        {
            std::unique_lock<std::mutex> lock(chunkMutex);
            if (!pendingLoad.empty()) {
                glm::ivec2 pos = *pendingLoad.begin(); pendingLoad.erase(pos);
                lock.unlock();
                auto data = loadChunkFromFile(pos.x, pos.y);
                lock.lock();
                if (data) pendingData[pos] = data;
                else pendingGen.insert(pos);
                processed++;
            }
        }
        {
            std::unique_lock<std::mutex> lock(chunkMutex);
            const int maxProcessedPerTick = fastChunkLoadingMode ? 64 : 20;
            while (!pendingGen.empty() && processed < maxProcessedPerTick) {
                glm::ivec2 pos = *pendingGen.begin(); pendingGen.erase(pos);
                lock.unlock();
                auto data = generateChunk(pos.x, pos.y);
                lock.lock();
                pendingData[pos] = data;
                processed++;
            }
        }
        if (processed == 0) {
            const int idleSleepMs = fastChunkLoadingMode ? 1 : 30;
            std::this_thread::sleep_for(std::chrono::milliseconds(idleSleepMs));
        }
    }
}

// ----------------------------------------------------------------------
// Шейдерные переменные
// ----------------------------------------------------------------------
unsigned int shaderProgram, reticleProgram, reticleVAO;
unsigned int cloudTexture = 0, cloudVAO = 0, cloudVBO = 0;
unsigned int rainTexture = 0, rainVAO = 0, rainVBO = 0;
bool isRaining = false;
int u_time_location, u_isWater_location, u_sunDir_location, u_sunIntensity_location, u_ambientBase_location;

struct Chunk;
std::unordered_map<glm::ivec2, Chunk, hash_ivec2> loadedChunks;
static glm::vec3 lastCameraPosForWaterSort(0.0f);
static std::vector<Chunk*> waterChunksCache;
static bool waterChunksCacheValid = false;
static Chunk* lastChunkForMesh = nullptr;
static glm::ivec2 lastChunkCoordsForMesh(0,0);

static GLint u_modelLoc = -1, u_viewLoc = -1, u_projLoc = -1;

constexpr float DAY_DURATION_SECONDS = 60.0f;
constexpr float NIGHT_DURATION_SECONDS = 60.0f;
constexpr float FULL_CYCLE_SECONDS = DAY_DURATION_SECONDS + NIGHT_DURATION_SECONDS;

#if 0
void handleLanguageMenuClick(GLFWwindow* window, int button) {
    if (button != GLFW_MOUSE_BUTTON_LEFT) return;
    
    double mx, my;
    glfwGetCursorPos(window, &mx, &my);
    
    // Только кнопка Done (индекс 0)
    if (mx >= languageButtons[0].absX && mx <= languageButtons[0].absX + languageButtons[0].absW &&
        my >= languageButtons[0].absY && my <= languageButtons[0].absY + languageButtons[0].absH) {
        currentState = GameState::MAIN_MENU;
    }
}

void renderLanguageMenu(int screenW, int screenH) {
    glClearColor(0,0,0,1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (menuBackgroundLightTexture || menuBackgroundTexture)
        drawTiledBackground(menuBackgroundLightTexture != 0 ? menuBackgroundLightTexture : menuBackgroundTexture, screenW, screenH);
    
    drawMinecraftTextCentered("Language", screenW * 0.5f, 120.0f, 3.0f, screenW, screenH, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    
    // Только кнопка Done
    int i = 0;
    unsigned int tex = menuButtonTexture;
    const bool hovered = isMouseOverButton(languageButtons[i], mouseX, mouseY);
    if (menuButtonHighlightTexture && hovered) {
        tex = menuButtonHighlightTexture;
    }
    drawRectangle(languageButtons[i].absX, languageButtons[i].absY, 
                  languageButtons[i].absW, languageButtons[i].absH, tex, screenW, screenH);
    
    drawMinecraftTextCentered(
        languageButtons[i].label,
        languageButtons[i].absX + languageButtons[i].absW * 0.5f,
        languageButtons[i].absY + languageButtons[i].absH * 0.52f,
        fitMinecraftTextScale(languageButtons[i].label, languageButtons[i].absW, languageButtons[i].absH),
        screenW,
        screenH,
        getMenuTextColor(hovered)
    );

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}
#endif




// ----------------------------------------------------------------------
// UI (меню и HUD) – без изменений
// ----------------------------------------------------------------------
unsigned int uiShaderProgram;
unsigned int uiVAO, uiVBO, uiEBO;
unsigned int fontVAO = 0, fontVBO = 0, fontEBO = 0;
unsigned int menuBackgroundTexture = 0, menuBackgroundLightTexture = 0, menuBackgroundDarkTexture = 0;
unsigned int menuButtonTexture = 0, menuButtonHighlightTexture = 0, menuPhotoTexture = 0; unsigned int menuButtonDisabledTexture = 0;
unsigned int hotbarTexture = 0, heartFullTexture = 0, heartHalfTexture = 0, hotbarSelTexture = 0, heartContTexture = 0;
unsigned int minecraftAsciiTexture = 0;
unsigned int languageButtonTexture = 0;
unsigned int inventoryTexture = 0;
unsigned int inventoryScrollerTexture = 0;
unsigned int inventoryScrollerDisabledTexture = 0;
std::unordered_map<int, unsigned int> minecraftFontPages;
std::unordered_map<int, std::array<uint8_t, 256>> minecraftFontPageWidths;

unsigned int dimShaderProgram;
unsigned int dimVAO;

int currentHotbarSlot = 0;
int inventoryScrollRow = 0;
constexpr int MAIN_MENU_BUTTON_COUNT = 6;
constexpr int WORLD_SELECT_BUTTON_COUNT = 6;
constexpr int LANGUAGE_BUTTON_COUNT = 1;
constexpr int OPTIONS_ROW1_BUTTON_COUNT = 4;
constexpr int OPTIONS_ROW2_BUTTON_COUNT = 5;
struct Button {
    float relX, relY, relW, relH;
    float absX, absY, absW, absH;
    bool clicked;
    const char* label;
};

enum class UILanguage {
    English,
    Russian,
    Japanese
};

struct MenuListEntry {
    std::string title;
    std::string subtitle;
    std::string detail;
};

struct MenuListState {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
    float rowHeight = 72.0f;
    float scroll = 0.0f;
    int selectedIndex = -1;
    int currentIndex = -1;
    int hoveredIndex = -1;
    int lastClickedIndex = -1;
    double lastClickTime = 0.0;
};

UILanguage currentLanguage = UILanguage::English;
std::vector<MenuListEntry> worldMenuEntries;
std::vector<MenuListEntry> languageMenuEntries;
MenuListState worldListState;
MenuListState languageListState;

void updateUILabels();
void refreshWorldMenuEntries();
void refreshLanguageMenuEntries();
void applyLanguageSelection(int index);

Button languageButtons[LANGUAGE_BUTTON_COUNT];
Button optionsRow1Buttons[OPTIONS_ROW1_BUTTON_COUNT];
Button optionsRow2Buttons[OPTIONS_ROW2_BUTTON_COUNT];
Button optionsDifficultyButton;
Button optionsDoneButton;
void initLanguageMenu() {
    languageButtons[0] = {0.5f, 0.9f, 0.2025f, 0.0486f, 0,0,0,0, false, "Done"};
    updateUILabels();
    refreshLanguageMenuEntries();
    refreshWorldMenuEntries();
}
Button buttons[MAIN_MENU_BUTTON_COUNT] = {
    {0.5f, 0.49f, 0.36f, 0.074f, 0,0,0,0, false, "Singleplayer"},
    {0.5f, 0.575f, 0.36f, 0.074f, 0,0,0,0, false, "Multiplayer"},
    {0.5f, 0.66f, 0.36f, 0.074f, 0,0,0,0, false, "Mods and Resource Packs"},
    {0.41f, 0.79f, 0.175f, 0.074f, 0,0,0,0, false, "Options"},
    {0.59f, 0.79f, 0.175f, 0.074f, 0,0,0,0, false, "Quit"},
    {0.29f, 0.79f, 0.0410f, 0.071f, 0,0,0,0, false, ""}
};
Button worldButtons[WORLD_SELECT_BUTTON_COUNT] = {
    {0.39f, 0.875f, 0.2025f, 0.0486f, 0,0,0,0, false, "Play Selected World"},
    {0.337f, 0.936f, 0.0972f, 0.0486f, 0,0,0,0, false, "Rename"},
    {0.443f, 0.936f, 0.0972f, 0.0486f, 0,0,0,0, false, "Delete"},
    {0.61f, 0.875f, 0.2025f, 0.0486f, 0,0,0,0, false, "Create New World"},
    {0.557f, 0.936f, 0.0972f, 0.0486f, 0,0,0,0, false, "Re-Create"},
    {0.663f, 0.936f, 0.0972f, 0.0486f, 0,0,0,0, false, "Cancel"}
};
Button pauseResumeButton = {0.5f, 0.45f, 0.4f, 0.08f, 0,0,0,0, false, "Resume"};
Button pauseExitButton   = {0.5f, 0.55f, 0.4f, 0.08f, 0,0,0,0, false, "Exit to Menu"};

float photoRelX = 0.5f, photoRelY = 0.23f, photoRelW = 0.46f, photoRelH = 0.18f;
float photoAbsX, photoAbsY, photoAbsW, photoAbsH;

const char* uiVertexShaderSrc = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;
out vec2 TexCoord;
uniform mat4 projection;
uniform mat4 model;
void main() {
    gl_Position = projection * model * vec4(aPos, 0.0, 1.0);
    TexCoord = aTexCoord;
}
)";

const char* uiFragmentShaderSrc = R"(
#version 330 core
in vec2 TexCoord;
out vec4 FragColor;
uniform sampler2D uTexture;
uniform vec4 uColor = vec4(1.0);
uniform bool uUseTexture = true;

void main() {
    if (uUseTexture) {
        FragColor = texture(uTexture, TexCoord) * uColor;
    } else {
        FragColor = uColor;
    }
}
)";

const char* dimVertexShaderSrc = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)";

const char* dimFragmentShaderSrc = R"(
#version 330 core
out vec4 FragColor;
uniform vec4 uColor;
void main() {
    FragColor = uColor;
}
)";

// Добавьте эти структуры и функции в ваш код

// Структура для слайдера


// Текстуры для слайдера
unsigned int sliderTrackTexture = 0;
unsigned int sliderThumbTexture = 0;
std::vector<Slider> optionsSliders;
constexpr float OPTIONS_UI_SCALE = 1.3f;
constexpr float OPTIONS_BUTTON_W = 430.0f * OPTIONS_UI_SCALE;
constexpr float OPTIONS_BUTTON_H = 58.0f * OPTIONS_UI_SCALE;
constexpr float OPTIONS_SLIDER_TRACK_W = OPTIONS_BUTTON_W;
constexpr float OPTIONS_SLIDER_TRACK_H = OPTIONS_BUTTON_H;
constexpr float OPTIONS_SLIDER_THUMB_W = 16.0f * OPTIONS_UI_SCALE * 1.1f;
constexpr float OPTIONS_SLIDER_THUMB_H = OPTIONS_BUTTON_H;

// Загрузка текстур слайдера
void loadSliderTextures() {
    sliderTrackTexture = loadUITexture("textures/slider_track.png");
    sliderThumbTexture = loadUITexture("textures/slider_thumb.png");
}

// Функция для обновления позиций слайдера
void updateSliderPosition(Slider& slider, int screenW, int screenH) {
    slider.absW = slider.relW * screenW;
    slider.absH = slider.relH * screenH;
    slider.absX = slider.relX * screenW - slider.absW / 2;
    slider.absY = slider.relY * screenH - slider.absH / 2;
}

// Функция для обновления позиций нескольких слайдеров
void updateSliderPositions(std::vector<Slider>& sliders, int screenW, int screenH) {
    for (auto& slider : sliders) {
        updateSliderPosition(slider, screenW, screenH);
    }
}

// Форматирование значения слайдера в строку
std::string formatSliderValue(float value, int decimalPlaces) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(decimalPlaces) << value;
    return ss.str();
}

// Проверка, находится ли мышь над слайдером
bool isMouseOverSlider(const Slider& slider, double mouseX, double mouseY) {
    const float trackX = slider.absX + (slider.absW - OPTIONS_SLIDER_TRACK_W) * 0.5f;
    const float trackY = slider.absY + (slider.absH - OPTIONS_SLIDER_TRACK_H) * 0.5f;
    return mouseX >= trackX && mouseX <= trackX + OPTIONS_SLIDER_TRACK_W &&
           mouseY >= trackY && mouseY <= trackY + OPTIONS_SLIDER_TRACK_H;
}

// Отрисовка слайдера
void drawSlider(const Slider& slider, int screenW, int screenH) {
    if (!sliderTrackTexture || !sliderThumbTexture) return;
    const float trackWidth = OPTIONS_SLIDER_TRACK_W;
    const float trackHeight = OPTIONS_SLIDER_TRACK_H;
    const float thumbWidth = OPTIONS_SLIDER_THUMB_W;
    const float thumbHeight = OPTIONS_SLIDER_THUMB_H;
    const float trackX = slider.absX + (slider.absW - trackWidth) * 0.5f;
    const float trackY = slider.absY + (slider.absH - trackHeight) * 0.5f;
    
    // Проверка наведения
    bool hovered = isMouseOverSlider(slider, mouseX, mouseY) || slider.isDragging;
    
    // Рисуем дорожку слайдера
    drawRectangle(trackX, trackY, trackWidth, trackHeight, 
                  sliderTrackTexture, screenW, screenH);
    
    // Вычисляем позицию ползунка
    float normalizedValue = (*slider.value - slider.minValue) / (slider.maxValue - slider.minValue);
    normalizedValue = glm::clamp(normalizedValue, 0.0f, 1.0f);
    float thumbX = trackX + normalizedValue * (trackWidth - thumbWidth);
    float thumbY = trackY + (trackHeight - thumbHeight) * 0.5f;
    
    // Рисуем ползунок
    unsigned int thumbTex = sliderThumbTexture;
    drawRectangle(thumbX, thumbY, thumbWidth, thumbHeight, thumbTex, screenW, screenH);
    
    // Формируем текст слайдера
    std::string displayText = std::string(slider.label) + ": " + 
                             formatSliderValue(*slider.value, slider.decimalPlaces);
    
    // Отрисовываем текст по центру
    glm::vec4 textColor = hovered ? 
        glm::vec4(1.0f, 0.93f, 0.62f, 1.0f) :  // Желтоватый при наведении
        glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);     // Белый обычный
    
    float scale = fitMinecraftTextScale(displayText, trackWidth * 0.92f, trackHeight * 0.8f);
    
    drawMinecraftTextCentered(
        displayText,
        trackX + trackWidth * 0.5f,
        trackY + trackHeight * 0.52f,
        scale,
        screenW,
        screenH,
        textColor
    );
}

// Обработка кликов по слайдеру
bool handleSliderClick(Slider& slider, double mouseX, double mouseY) {
    const float trackWidth = OPTIONS_SLIDER_TRACK_W;
    const float trackHeight = OPTIONS_SLIDER_TRACK_H;
    const float thumbWidth = OPTIONS_SLIDER_THUMB_W;
    const float trackX = slider.absX + (slider.absW - trackWidth) * 0.5f;
    const float trackY = slider.absY + (slider.absH - trackHeight) * 0.5f;
    const bool insideTrack = mouseX >= trackX && mouseX <= trackX + trackWidth &&
                             mouseY >= trackY && mouseY <= trackY + trackHeight;
    if (!insideTrack) {
        slider.isDragging = false;
        return false;
    }
    
    slider.isDragging = true;
    
    // Вычисляем новое значение на основе позиции мыши
    float relativeX = (float)(mouseX - trackX - thumbWidth * 0.5f) /
                     (trackWidth - thumbWidth);
    relativeX = glm::clamp(relativeX, 0.0f, 1.0f);
    
    float rawValue = slider.minValue + relativeX * (slider.maxValue - slider.minValue);
    
    // Применяем шаг
    if (slider.step > 0) {
        rawValue = round(rawValue / slider.step) * slider.step;
    }
    
    *slider.value = glm::clamp(rawValue, slider.minValue, slider.maxValue);
    return true;
}

// Обработка перетаскивания слайдера
void handleSliderDrag(Slider& slider, double mouseX, double mouseY) {
    if (!slider.isDragging) return;
    const float trackWidth = OPTIONS_SLIDER_TRACK_W;
    const float thumbWidth = OPTIONS_SLIDER_THUMB_W;
    const float trackX = slider.absX + (slider.absW - trackWidth) * 0.5f;
    float relativeX = (float)(mouseX - trackX - thumbWidth * 0.5f) / 
                     (trackWidth - thumbWidth);
    relativeX = glm::clamp(relativeX, 0.0f, 1.0f);
    
    float rawValue = slider.minValue + relativeX * (slider.maxValue - slider.minValue);
    
    if (slider.step > 0) {
        rawValue = round(rawValue / slider.step) * slider.step;
    }
    
    *slider.value = glm::clamp(rawValue, slider.minValue, slider.maxValue);
}

// Освобождение слайдера
void releaseSlider(Slider& slider) {
    slider.isDragging = false;
}

// Глобальная переменная для FOV
float currentFOV = 65.0f;  // Стандартное значение FOV

// Функция инициализации слайдера FOV
void initFOVSlider(std::vector<Slider>& sliders) {
    Slider fovSlider;
    
    // Позиция: 10% от верха, 5% левее от середины (т.е. 45% от ширины экрана)
    fovSlider.relX = 0.5f - 0.05f;  // 5% левее от середины = 0.45 (45% ширины)
    fovSlider.relY = 0.1f;          // 10% от верха
    
    // Размеры слайдера
    fovSlider.relW = 0.4f;   // 40% ширины экрана
    fovSlider.relH = 0.08f;  // 8% высоты экрана
    
    // Текст и значения
    fovSlider.label = tr("FOV", "FOV", "FOV");
    fovSlider.minValue = 30.0f;
    fovSlider.maxValue = 110.0f;
    fovSlider.value = &currentFOV;
    fovSlider.step = 1.0f;
    fovSlider.isDragging = false;
    fovSlider.decimalPlaces = 0;  // Целое число, без десятичных знаков
    
    sliders.push_back(fovSlider);
}

void initUI() {
    unsigned int vs = glCreateShader(GL_VERTEX_SHADER), fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(vs, 1, &uiVertexShaderSrc, NULL); glCompileShader(vs);
    glShaderSource(fs, 1, &uiFragmentShaderSrc, NULL); glCompileShader(fs);
    uiShaderProgram = glCreateProgram();
    glAttachShader(uiShaderProgram, vs); glAttachShader(uiShaderProgram, fs);
    glLinkProgram(uiShaderProgram);
    glDeleteShader(vs); glDeleteShader(fs);

    float vertices[] = { 0,0,0,1, 1,0,1,1, 1,1,1,0, 0,1,0,0 };
    unsigned int indices[] = {0,1,2,0,2,3};
    glGenVertexArrays(1, &uiVAO); glGenBuffers(1, &uiVBO); glGenBuffers(1, &uiEBO);
    glBindVertexArray(uiVAO);
    glBindBuffer(GL_ARRAY_BUFFER, uiVBO); glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, uiEBO); glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,4*sizeof(float),(void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,4*sizeof(float),(void*)(2*sizeof(float))); glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    glGenVertexArrays(1, &fontVAO);
    glGenBuffers(1, &fontVBO);
    glGenBuffers(1, &fontEBO);
    glBindVertexArray(fontVAO);
    glBindBuffer(GL_ARRAY_BUFFER, fontVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, fontEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,4*sizeof(float),(void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,4*sizeof(float),(void*)(2*sizeof(float))); glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    float quadVertices[] = {
        -1.0f, -1.0f,
         1.0f, -1.0f,
         1.0f,  1.0f,
        -1.0f,  1.0f
    };
    unsigned int quadIndices[] = {0, 1, 2, 0, 2, 3};
    glGenVertexArrays(1, &dimVAO);
    glBindVertexArray(dimVAO);
    unsigned int dimVBO, dimEBO;
    glGenBuffers(1, &dimVBO);
    glGenBuffers(1, &dimEBO);
    glBindBuffer(GL_ARRAY_BUFFER, dimVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, dimEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(quadIndices), quadIndices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

unsigned int loadUITexture(const char* path) {
    unsigned int tex = 0;
    int w, h, ch;
    unsigned char* data = stbi_load(path, &w, &h, &ch, 4);
    if (data) {
        glGenTextures(1, &tex); glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        stbi_image_free(data);
    } else std::cerr << "Failed to load UI texture: " << path << std::endl;
    return tex;
}

uint32_t decodeNextUtf8Codepoint(const std::string& text, size_t& index) {
    if (index >= text.size()) return 0;

    unsigned char c0 = static_cast<unsigned char>(text[index++]);
    if (c0 < 0x80) return c0;

    auto readContinuation = [&](uint32_t& value) -> bool {
        if (index >= text.size()) return false;
        unsigned char cx = static_cast<unsigned char>(text[index]);
        if ((cx & 0xC0) != 0x80) return false;
        value = (value << 6) | (cx & 0x3F);
        ++index;
        return true;
    };

    if ((c0 & 0xE0) == 0xC0) {
        uint32_t value = c0 & 0x1F;
        return readContinuation(value) ? value : '?';
    }
    if ((c0 & 0xF0) == 0xE0) {
        uint32_t value = c0 & 0x0F;
        if (!readContinuation(value) || !readContinuation(value)) return '?';
        return value;
    }
    if ((c0 & 0xF8) == 0xF0) {
        uint32_t value = c0 & 0x07;
        if (!readContinuation(value) || !readContinuation(value) || !readContinuation(value)) return '?';
        return value;
    }
    return '?';
}

float getMinecraftGlyphAdvance(uint32_t codepoint, float scale) {
    if (codepoint == ' ') return 4.0f * scale;
    if (codepoint == '\t') return 16.0f * scale;
    if (codepoint > 0xFFFF) codepoint = '?';

    const int page = (codepoint < 128 && minecraftAsciiTexture != 0) ? -1 : static_cast<int>((codepoint >> 8) & 0xFF);
    auto widthIt = minecraftFontPageWidths.find(page);
    if (widthIt == minecraftFontPageWidths.end()) {
        std::array<uint8_t, 256> widths{};
        widths.fill(6);

        const char* fileName = nullptr;
        char unicodeFileName[64];
        if (page == -1) {
            fileName = "fonts/minecraft/ascii.png";
        } else {
            std::snprintf(unicodeFileName, sizeof(unicodeFileName), "fonts/minecraft/unicode_page_%02x.png", page);
            fileName = unicodeFileName;
        }

        int w = 0, h = 0, ch = 0;
        unsigned char* data = stbi_load(fileName, &w, &h, &ch, 4);
        if (data && w == 128 && h == 128) {
            for (int glyph = 0; glyph < 256; ++glyph) {
                const int cellX = (glyph & 0x0F) * 8;
                const int cellY = (glyph >> 4) * 8;
                int minX = 8;
                int maxX = -1;

                for (int py = 0; py < 8; ++py) {
                    for (int px = 0; px < 8; ++px) {
                        const int pixelIndex = ((cellY + py) * w + (cellX + px)) * 4 + 3;
                        if (data[pixelIndex] > 8) {
                            minX = std::min(minX, px);
                            maxX = std::max(maxX, px);
                        }
                    }
                }

                if (maxX >= minX) {
                    widths[glyph] = static_cast<uint8_t>(std::clamp((maxX - minX + 1) + 1, 2, 8));
                } else if (glyph == ' ') {
                    widths[glyph] = 4;
                } else {
                    widths[glyph] = 6;
                }
            }
            stbi_image_free(data);
        } else if (data) {
            stbi_image_free(data);
        }

        widthIt = minecraftFontPageWidths.emplace(page, widths).first;
    }

    const uint32_t glyphIndex = (codepoint < 128 && minecraftAsciiTexture != 0) ? codepoint : (codepoint & 0xFF);
    float advance = static_cast<float>(widthIt->second[glyphIndex]);
    if (page != -1) {
        advance = std::max(advance, 8.0f);
    }
    return advance * scale;
}

float measureMinecraftTextWidth(const std::string& text, float scale) {
    float width = 0.0f;
    size_t index = 0;
    while (index < text.size()) {
        uint32_t codepoint = decodeNextUtf8Codepoint(text, index);
        if (codepoint == '\n') break;
        width += getMinecraftGlyphAdvance(codepoint, scale);
    }
    return width;
}

float fitMinecraftTextScale(const std::string& text, float maxWidth, float maxHeight) {
    const float baseWidth = std::max(1.0f, measureMinecraftTextWidth(text, 1.0f));
    const float widthScale = (maxWidth * 0.72f) / baseWidth;
    const float heightScale = (maxHeight * 0.42f) / 8.0f;
    return std::max(0.5f, std::min(widthScale, heightScale));
}

float fitMinecraftButtonTextScale(const std::string& text, float maxWidth, float maxHeight) {
    float scale = fitMinecraftTextScale(text, maxWidth, maxHeight);
    if (currentLanguage == UILanguage::Russian) {
        scale *= 1.18f;
    }
    return scale;
}

unsigned int getMinecraftFontTexture(uint32_t codepoint) {
    if (codepoint < 128 && minecraftAsciiTexture != 0) {
        return minecraftAsciiTexture;
    }

    const int page = static_cast<int>((codepoint >> 8) & 0xFF);
    auto it = minecraftFontPages.find(page);
    if (it != minecraftFontPages.end()) {
        return it->second != 0 ? it->second : minecraftAsciiTexture;
    }

    char fileName[64];
    std::snprintf(fileName, sizeof(fileName), "fonts/minecraft/unicode_page_%02x.png", page);

    unsigned int tex = 0;
    if (fs::exists(fileName)) {
        tex = loadUITexture(fileName);
    }
    minecraftFontPages[page] = tex;
    return tex != 0 ? tex : minecraftAsciiTexture;
}

void loadMinecraftFontAssets() {
    if (minecraftAsciiTexture == 0 && fs::exists("fonts/minecraft/ascii.png")) {
        minecraftAsciiTexture = loadUITexture("fonts/minecraft/ascii.png");
    }
}

void drawMinecraftGlyph(uint32_t codepoint, float x, float y, float scale, int screenW, int screenH, const glm::vec4& color) {
    unsigned int texture = getMinecraftFontTexture(codepoint);
    if (texture == 0) return;

    if (codepoint > 0xFFFF) codepoint = '?';
    uint32_t glyphIndex = (codepoint < 128 && minecraftAsciiTexture != 0) ? codepoint : (codepoint & 0xFF);

    const int col = static_cast<int>(glyphIndex & 0x0F);
    const int row = static_cast<int>((glyphIndex >> 4) & 0x0F);
    const float u0 = col / 16.0f;
    const float u1 = (col + 1) / 16.0f;
    const float v0 = row / 16.0f;
    const float v1 = (row + 1) / 16.0f;
    const float vertices[] = {
        0.0f, 0.0f, u0, v0,
        1.0f, 0.0f, u1, v0,
        1.0f, 1.0f, u1, v1,
        0.0f, 1.0f, u0, v1
    };

    glm::mat4 proj = glm::ortho(0.0f, static_cast<float>(screenW), static_cast<float>(screenH), 0.0f);
    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, 0.0f));
    model = glm::scale(model, glm::vec3(8.0f * scale, 8.0f * scale, 1.0f));

    glUseProgram(uiShaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(uiShaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(proj));
    glUniformMatrix4fv(glGetUniformLocation(uiShaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniform1i(glGetUniformLocation(uiShaderProgram, "uUseTexture"), 1);
    glUniform4f(glGetUniformLocation(uiShaderProgram, "uColor"), color.r, color.g, color.b, color.a);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1i(glGetUniformLocation(uiShaderProgram, "uTexture"), 0);

    glBindVertexArray(fontVAO);
    glBindBuffer(GL_ARRAY_BUFFER, fontVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

void drawMinecraftText(const std::string& text, float x, float y, float scale, int screenW, int screenH, const glm::vec4& color, bool withShadow = true) {
    if (minecraftAsciiTexture == 0 || text.empty()) return;

    if (withShadow) {
        const glm::vec4 shadowColor(0.0f, 0.0f, 0.0f, color.a * 0.9f);
        drawMinecraftText(text, x + scale, y + scale, scale, screenW, screenH, shadowColor, false);
    }

    float cursorX = x;
    size_t index = 0;
    while (index < text.size()) {
        uint32_t codepoint = decodeNextUtf8Codepoint(text, index);
        if (codepoint == '\n') break;
        if (codepoint != ' ' && codepoint != '\t') {
            drawMinecraftGlyph(codepoint, cursorX, y, scale, screenW, screenH, color);
        }
        cursorX += getMinecraftGlyphAdvance(codepoint, scale);
    }

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void drawMinecraftTextCentered(const std::string& text, float centerX, float centerY, float scale, int screenW, int screenH, const glm::vec4& color) {
    const float width = measureMinecraftTextWidth(text, scale);
    const float height = 8.0f * scale;
    drawMinecraftText(text, centerX - width * 0.5f, centerY - height * 0.5f, scale, screenW, screenH, color, true);
}

glm::vec4 getMenuTextColor(bool hovered) {
    return hovered ? glm::vec4(1.0f, 0.93f, 0.62f, 1.0f) : glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
}

const char* tr(const char* en, const char* ru, const char* jp) {
    switch (currentLanguage) {
        case UILanguage::Russian: return ru;
        case UILanguage::Japanese: return jp;
        default: return en;
    }
}

void updateUILabels() {
    buttons[0].label = tr("Singleplayer", "Одиночная игра", "シングルプレイ");
    buttons[1].label = tr("Multiplayer", "Мультиплеер", "マルチプレイ");
    buttons[2].label = tr("Mods and Resource Packs", "Моды и текстурпаки", "Modとリソースパック");
    buttons[3].label = tr("Options...", "Опции", "設定");
    buttons[4].label = tr("Quit Game", "Выйти из игры", "終了");

    worldButtons[0].label = tr("Play Selected World", "Играть в выбранном мире", "選択したワールドで遊ぶ");
    worldButtons[1].label = tr("Rename", "Переименовать", "名前変更");
    worldButtons[2].label = tr("Delete", "Удалить", "削除");
    worldButtons[3].label = tr("Create New World", "Создать новый мир", "新しいワールドを作成");
    worldButtons[4].label = tr("Re-Create", "Пересоздать", "再作成");
    worldButtons[5].label = tr("Cancel", "Отмена", "キャンセル");

    pauseResumeButton.label = tr("Resume", "Продолжить", "ゲームに戻る");
    pauseExitButton.label = tr("Exit to Menu", "Выйти в меню", "メニューへ戻る");
    languageButtons[0].label = tr("Done", "Готово", "完了");
}

std::string formatFileTime(const fs::file_time_type& fileTime) {
    auto systemTime = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        fileTime - fs::file_time_type::clock::now() + std::chrono::system_clock::now()
    );
    std::time_t tt = std::chrono::system_clock::to_time_t(systemTime);
    std::tm localTm{};
#ifdef _WIN32
    localtime_s(&localTm, &tt);
#else
    localtime_r(&tt, &localTm);
#endif
    char buffer[64];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M", &localTm);
    return buffer;
}

// Вспомогательная функция: проверяет, доступна ли кнопка в меню выбора мира
bool isWorldButtonEnabled(int buttonIndex) {
    switch (buttonIndex) {
        case 0: // "Play Selected World"
            return worldListState.selectedIndex >= 0 &&
                   worldListState.selectedIndex < static_cast<int>(availableWorlds.size());
        case 1: // "Rename"
            return worldListState.selectedIndex >= 0 &&
                   worldListState.selectedIndex < static_cast<int>(availableWorlds.size());
        case 2: // "Delete"
            return worldListState.selectedIndex >= 0 &&
                   worldListState.selectedIndex < static_cast<int>(availableWorlds.size());
        case 3: // "Create New World"
            return true; // всегда доступна
        case 4: // "Re-Create"
            return worldListState.selectedIndex >= 0 &&
                   worldListState.selectedIndex < static_cast<int>(availableWorlds.size());
        case 5: // "Cancel"
            return true; // всегда доступна
        default:
            return true;
    }
}

void refreshWorldMenuEntries() {
    availableWorlds.clear();
    worldMenuEntries.clear();

    if (fs::exists(getSavesRootPath())) {
        for (const auto& saveEntry : fs::directory_iterator(getSavesRootPath())) {
            if (!saveEntry.is_directory()) continue;

            fs::path metadataPath = saveEntry.path() / "world.json";
            if (!fs::exists(metadataPath)) continue;

            std::ifstream in(metadataPath);
            if (!in.is_open()) continue;

            json metadata = json::parse(in, nullptr, false);
            if (metadata.is_discarded()) continue;

            WorldSummary summary;
            summary.folderName = saveEntry.path().filename().string();
            summary.displayName = metadata.value("name", summary.folderName);
            summary.gameMode = metadata.value("gameMode", "survival");
            summary.lastPlayed = fs::last_write_time(metadataPath);
            availableWorlds.push_back(summary);
        }
    }

    std::sort(availableWorlds.begin(), availableWorlds.end(), [](const WorldSummary& a, const WorldSummary& b) {
        return a.lastPlayed > b.lastPlayed;
    });

    for (const auto& world : availableWorlds) {
        MenuListEntry entry;
        entry.title = world.displayName;
        entry.subtitle = world.folderName + " (" + formatFileTime(world.lastPlayed) + ")";
        entry.detail = tr("Game Mode: Survival", "Режим игры: Выживание", "ゲームモード: サバイバル");
        worldMenuEntries.push_back(entry);
    }

    if (worldMenuEntries.empty()) {
        worldMenuEntries.push_back({
            tr("No worlds yet", "Миров пока нет", "ワールドはまだありません"),
            tr("Create one with the button below", "Создай его кнопкой ниже", "下のボタンで作成できます"),
            tr("Game Mode: Survival", "Режим игры: Выживание", "ゲームモード: サバイバル")
        });
    }
    
    if (!selectedWorldFolderName.empty()) {
        worldListState.currentIndex = -1;
        for (int i = 0; i < static_cast<int>(availableWorlds.size()); ++i) {
            if (availableWorlds[i].folderName == selectedWorldFolderName) {
                worldListState.currentIndex = i;
                break;
            }
        }
    } else {
        worldListState.currentIndex = -1;
    }

    worldListState.selectedIndex = -1;
    worldListState.lastClickedIndex = -1;
}

void refreshLanguageMenuEntries() {
    languageMenuEntries.clear();
    languageMenuEntries.push_back({"Русский", "", ""});
    languageMenuEntries.push_back({"English", "", ""});
    languageMenuEntries.push_back({"日本語", "", ""});

    languageListState.currentIndex =
        currentLanguage == UILanguage::Russian ? 0 :
        currentLanguage == UILanguage::English ? 1 : 2;
    languageListState.selectedIndex = languageListState.currentIndex;
}

void applyLanguageSelection(int index) {
    if (index == 0) currentLanguage = UILanguage::Russian;
    else if (index == 1) currentLanguage = UILanguage::English;
    else if (index == 2) currentLanguage = UILanguage::Japanese;
    updateUILabels();
    refreshLanguageMenuEntries();
    refreshWorldMenuEntries();
}

bool hasSavedWorld() {
    fs::path chunksPath = getCurrentChunksPath();
    if (chunksPath.empty() || !fs::exists(chunksPath) || !fs::is_directory(chunksPath)) {
        return false;
    }

    for (const auto& entry : fs::directory_iterator(chunksPath)) {
        if (entry.is_regular_file()) {
            return true;
        }
    }
    return false;
}

void loadMenuTextures() {
    menuBackgroundLightTexture = loadUITexture("textures/background_light.png");
    menuBackgroundDarkTexture = loadUITexture("textures/background_dark.png");
    menuBackgroundTexture = menuBackgroundLightTexture != 0 ? menuBackgroundLightTexture : loadUITexture("textures/menu_background.jpg");
    menuButtonTexture      = loadUITexture("textures/menu_button.png");
    menuButtonHighlightTexture = loadUITexture("textures/menu_button_highlighted.png");
    menuPhotoTexture       = loadUITexture("textures/menu_photo.png");
    languageButtonTexture = loadUITexture("textures/language.png");
    menuButtonDisabledTexture = loadUITexture("textures/menu_button_disabled.png");
    loadMinecraftFontAssets();
}

void loadHUDTextures() {
    hotbarTexture    = loadUITexture("textures/Hotbar.png");
    heartFullTexture  = loadUITexture("textures/heart_full.png");
    heartHalfTexture  = loadUITexture("textures/heart_half.png");
    hotbarSelTexture  = loadUITexture("textures/hotbar_sel.png");
    heartContTexture  = loadUITexture("textures/heart_cont.png");
    inventoryTexture  = loadUITexture("textures/creative_inventory.png");
    inventoryScrollerTexture = loadUITexture("textures/scroller.png");
    inventoryScrollerDisabledTexture = loadUITexture("textures/disable_scroller.png");
    if (!inventoryScrollerDisabledTexture) {
        inventoryScrollerDisabledTexture = inventoryScrollerTexture;
    }
}

void updateButtonPositions(int screenW, int screenH) {
    for (int i=0; i<MAIN_MENU_BUTTON_COUNT; ++i) {
        buttons[i].absW = buttons[i].relW * screenW;
        buttons[i].absH = buttons[i].relH * screenH;
        buttons[i].absX = buttons[i].relX * screenW - buttons[i].absW/2;
        buttons[i].absY = buttons[i].relY * screenH - buttons[i].absH/2;
    }
    for (int i=0; i<WORLD_SELECT_BUTTON_COUNT; ++i) {
        worldButtons[i].absW = worldButtons[i].relW * screenW;
        worldButtons[i].absH = worldButtons[i].relH * screenH;
        worldButtons[i].absX = worldButtons[i].relX * screenW - worldButtons[i].absW/2;
        worldButtons[i].absY = worldButtons[i].relY * screenH - worldButtons[i].absH/2;
    }
    for (int i=0; i<LANGUAGE_BUTTON_COUNT; ++i) {
        languageButtons[i].absW = languageButtons[i].relW * screenW;
        languageButtons[i].absH = languageButtons[i].relH * screenH;
        languageButtons[i].absX = languageButtons[i].relX * screenW - languageButtons[i].absW/2;
        languageButtons[i].absY = languageButtons[i].relY * screenH - languageButtons[i].absH/2;
    }
    pauseResumeButton.absW = pauseResumeButton.relW * screenW;
    pauseResumeButton.absH = pauseResumeButton.relH * screenH;
    pauseResumeButton.absX = pauseResumeButton.relX * screenW - pauseResumeButton.absW/2;
    pauseResumeButton.absY = pauseResumeButton.relY * screenH - pauseResumeButton.absH/2;

    pauseExitButton.absW = pauseExitButton.relW * screenW;
    pauseExitButton.absH = pauseExitButton.relH * screenH;
    pauseExitButton.absX = pauseExitButton.relX * screenW - pauseExitButton.absW/2;
    pauseExitButton.absY = pauseExitButton.relY * screenH - pauseExitButton.absH/2;
}

void updatePhotoPosition(int screenW, int screenH) {
    photoAbsW = photoRelW * screenW; photoAbsH = photoRelH * screenH;
    photoAbsX = photoRelX * screenW - photoAbsW / 2;
    photoAbsY = photoRelY * screenH - photoAbsH / 2;
}

void drawRectangle(float x, float y, float w, float h, unsigned int texture, int screenW, int screenH) {
    glm::mat4 proj = glm::ortho(0.0f, (float)screenW, (float)screenH, 0.0f);
    glUseProgram(uiShaderProgram);
    glUniform1i(glGetUniformLocation(uiShaderProgram, "uUseTexture"), 1);
    glUniform4f(glGetUniformLocation(uiShaderProgram, "uColor"), 1.0f, 1.0f, 1.0f, 1.0f);
    glUniformMatrix4fv(glGetUniformLocation(uiShaderProgram,"projection"),1,GL_FALSE,glm::value_ptr(proj));
    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(x,y,0));
    model = glm::scale(model, glm::vec3(w,h,1));
    glUniformMatrix4fv(glGetUniformLocation(uiShaderProgram,"model"),1,GL_FALSE,glm::value_ptr(model));
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1i(glGetUniformLocation(uiShaderProgram,"uTexture"),0);
    glBindVertexArray(uiVAO); glDrawElements(GL_TRIANGLES,6,GL_UNSIGNED_INT,0);
}

void drawRectangleUV(float x, float y, float w, float h, unsigned int texture, int screenW, int screenH, float u0, float v0, float u1, float v1) {
    glm::mat4 proj = glm::ortho(0.0f, static_cast<float>(screenW), static_cast<float>(screenH), 0.0f);
    const float vertices[] = {
        0.0f, 0.0f, u0, v1,
        1.0f, 0.0f, u1, v1,
        1.0f, 1.0f, u1, v0,
        0.0f, 1.0f, u0, v0
    };

    glUseProgram(uiShaderProgram);
    glUniform1i(glGetUniformLocation(uiShaderProgram, "uUseTexture"), 1);
    glUniform4f(glGetUniformLocation(uiShaderProgram, "uColor"), 1.0f, 1.0f, 1.0f, 1.0f);
    glUniformMatrix4fv(glGetUniformLocation(uiShaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(proj));

    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, 0.0f));
    model = glm::scale(model, glm::vec3(w, h, 1.0f));
    glUniformMatrix4fv(glGetUniformLocation(uiShaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1i(glGetUniformLocation(uiShaderProgram, "uTexture"), 0);

    glBindVertexArray(fontVAO);
    glBindBuffer(GL_ARRAY_BUFFER, fontVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void drawTiledBackground(unsigned int texture, int screenW, int screenH, float x = 0.0f, float y = 0.0f, float w = -1.0f, float h = -1.0f, float tileSize = 96.0f) {
    if (texture == 0) return;

    if (w < 0.0f) w = static_cast<float>(screenW);
    if (h < 0.0f) h = static_cast<float>(screenH);

    const int tilesX = static_cast<int>(std::ceil(w / tileSize));
    const int tilesY = static_cast<int>(std::ceil(h / tileSize));

    for (int ty = 0; ty < tilesY; ++ty) {
        for (int tx = 0; tx < tilesX; ++tx) {
            float drawX = x + tx * tileSize;
            float drawY = y + ty * tileSize;
            float drawW = std::min(tileSize, x + w - drawX);
            float drawH = std::min(tileSize, y + h - drawY);
            if (drawW > 0.0f && drawH > 0.0f) {
                drawRectangleUV(drawX, drawY, drawW, drawH, texture, screenW, screenH, 0.0f, 0.0f, drawW / tileSize, drawH / tileSize);
            }
        }
    }
}

void drawColorRectangle(float x, float y, float w, float h, int screenW, int screenH, const glm::vec4& color) {
    glm::mat4 proj = glm::ortho(0.0f, static_cast<float>(screenW), static_cast<float>(screenH), 0.0f);
    glUseProgram(uiShaderProgram);
    glUniform1i(glGetUniformLocation(uiShaderProgram, "uUseTexture"), 0);
    glUniform4f(glGetUniformLocation(uiShaderProgram, "uColor"), color.r, color.g, color.b, color.a);
    glUniformMatrix4fv(glGetUniformLocation(uiShaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(proj));
    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, 0.0f));
    model = glm::scale(model, glm::vec3(w, h, 1.0f));
    glUniformMatrix4fv(glGetUniformLocation(uiShaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glBindVertexArray(uiVAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

void drawFrame(float x, float y, float w, float h, int screenW, int screenH, const glm::vec4& color, float thickness = 2.0f) {
    drawColorRectangle(x, y, w, thickness, screenW, screenH, color);
    drawColorRectangle(x, y + h - thickness, w, thickness, screenW, screenH, color);
    drawColorRectangle(x, y, thickness, h, screenW, screenH, color);
    drawColorRectangle(x + w - thickness, y, thickness, h, screenW, screenH, color);
}

bool isInsideRect(float px, float py, float x, float y, float w, float h) {
    return px >= x && px <= x + w && py >= y && py <= y + h;
}

void renderMenuList(const MenuListState& state, const std::vector<MenuListEntry>& entries, int screenW, int screenH) {
    const float contentHeight = entries.size() * state.rowHeight;
    const float maxScroll = std::max(0.0f, contentHeight - state.h);
    const float scroll = std::clamp(state.scroll, 0.0f, maxScroll);
    const float verticalOffset = contentHeight < state.h ? (state.h - contentHeight) * 0.5f : 0.0f;

    for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
        const float rowY = state.y + verticalOffset + i * state.rowHeight - scroll;
        if (rowY + state.rowHeight < state.y || rowY > state.y + state.h) continue;

        const bool selected = (i == state.selectedIndex);
        const bool hovered = (i == state.hoveredIndex);

        if (selected) {
            drawColorRectangle(state.x + 4.0f, rowY + 4.0f, state.w - 8.0f, state.rowHeight - 8.0f, screenW, screenH, glm::vec4(0.0f, 0.0f, 0.0f, 0.82f));
            drawFrame(state.x + 4.0f, rowY + 4.0f, state.w - 8.0f, state.rowHeight - 8.0f, screenW, screenH, glm::vec4(0.62f, 0.62f, 0.62f, 0.95f), 2.0f);
        }
        const float centerX = state.x + state.w * 0.5f;
        const float textX = state.x + 18.0f;
        const bool simpleCentered = entries[i].subtitle.empty() && entries[i].detail.empty();
        if (simpleCentered) {
            drawMinecraftTextCentered(entries[i].title, centerX, rowY + state.rowHeight * 0.5f, 3.6f, screenW, screenH, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
        } else {
            drawMinecraftText(entries[i].title, textX, rowY + 10.0f, 3.2f, screenW, screenH, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), true);
            if (!entries[i].subtitle.empty()) {
                drawMinecraftText(entries[i].subtitle, textX, rowY + 50.0f, 1.9f, screenW, screenH, glm::vec4(0.72f, 0.72f, 0.72f, 1.0f), true);
            }
        }
        if (!simpleCentered && !entries[i].detail.empty()) {
            drawMinecraftText(entries[i].detail, textX, rowY + 78.0f, 1.9f, screenW, screenH, glm::vec4(0.72f, 0.72f, 0.72f, 1.0f), true);
        }
    }
}

bool handleMenuListClick(MenuListState& state, const std::vector<MenuListEntry>& entries, double mx, double my, double now, int& appliedIndex) {
    appliedIndex = -1;
    if (!isInsideRect(static_cast<float>(mx), static_cast<float>(my), state.x, state.y, state.w, state.h)) return false;

    const float contentHeight = entries.size() * state.rowHeight;
    const float maxScroll = std::max(0.0f, contentHeight - state.h);
    state.scroll = std::clamp(state.scroll, 0.0f, maxScroll);
    const float verticalOffset = contentHeight < state.h ? (state.h - contentHeight) * 0.5f : 0.0f;

    const float localY = static_cast<float>(my) - state.y - verticalOffset + state.scroll;
    const int index = static_cast<int>(localY / state.rowHeight);
    if (index < 0 || index >= static_cast<int>(entries.size())) return true;

    if (state.lastClickedIndex == index && (now - state.lastClickTime) <= 0.30) {
        state.selectedIndex = index;
        appliedIndex = index;
    } else {
        state.selectedIndex = index;
    }

    state.lastClickedIndex = index;
    state.lastClickTime = now;
    return true;
}

void scrollMenuList(MenuListState& state, const std::vector<MenuListEntry>& entries, double offset) {
    const float contentHeight = entries.size() * state.rowHeight;
    const float maxScroll = std::max(0.0f, contentHeight - state.h);
    state.scroll = std::clamp(state.scroll - static_cast<float>(offset) * 28.0f, 0.0f, maxScroll);
}

bool isMouseOverButton(const Button& btn, double mouseX, double mouseY) {
    return mouseX >= btn.absX && mouseX <= btn.absX + btn.absW &&
           mouseY >= btn.absY && mouseY <= btn.absY + btn.absH;
}


void drawDimOverlay(int screenW, int screenH, float alpha) {
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    glUseProgram(dimShaderProgram);
    glUniform4f(glGetUniformLocation(dimShaderProgram, "uColor"), 0.0f, 0.0f, 0.0f, alpha);
    glBindVertexArray(dimVAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    
    glDisable(GL_BLEND);
}

void syncInventoryHotbarFromGameHotbar() {
    for (int i = 0; i < 9; ++i) {
        playerInventoryItems[27 + i] = playerInventoryItems[27 + i];
    }
}

// Рисует инвентарь по центру экрана
void renderInventory(int screenW, int screenH) {
    if (!inventoryTexture) return;
    
    constexpr float INVENTORY_UI_SCALE = 2.6f;
    const int INV_W = static_cast<int>(195.0f * INVENTORY_UI_SCALE);
    const int INV_H = static_cast<int>(136.0f * INVENTORY_UI_SCALE);
    
    int posX = (screenW - INV_W) / 2;
    int posY = (screenH - INV_H) / 2;
    
    // Сохраняем состояния OpenGL
    GLboolean depthEnabled = glIsEnabled(GL_DEPTH_TEST);
    GLboolean cullEnabled = glIsEnabled(GL_CULL_FACE);
    GLboolean blendEnabled = glIsEnabled(GL_BLEND);
    GLboolean scissorEnabled = glIsEnabled(GL_SCISSOR_TEST);
    GLboolean depthMask;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
    GLint oldViewport[4];
    glGetIntegerv(GL_VIEWPORT, oldViewport);
    GLint oldProgram;
    glGetIntegerv(GL_CURRENT_PROGRAM, &oldProgram);
    GLint oldVAO;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &oldVAO);
    GLint blendSrc, blendDst;
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &blendSrc);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &blendDst);
    
    // Рисуем фон инвентаря
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    glUseProgram(uiShaderProgram);
    
    drawColorRectangle(
        static_cast<float>(posX), static_cast<float>(posY),
        static_cast<float>(INV_W), static_cast<float>(INV_H),
        screenW, screenH, glm::vec4(0.78f, 0.78f, 0.78f, 1.0f));
    
    drawFrame(
        static_cast<float>(posX), static_cast<float>(posY),
        static_cast<float>(INV_W), static_cast<float>(INV_H),
        screenW, screenH, glm::vec4(0.18f, 0.18f, 0.18f, 1.0f), 2.0f);
    
    drawRectangle(static_cast<float>(posX), static_cast<float>(posY),
                  static_cast<float>(INV_W), static_cast<float>(INV_H),
                  inventoryTexture, screenW, screenH);
    
    // Вычисляем размеры слотов
    const float scaleX = INV_W / 195.0f;
    const float scaleY = INV_H / 136.0f;
    const int slotW = static_cast<int>(18.0f * scaleX);
    const int slotH = static_cast<int>(18.0f * scaleY);
    
    const glm::mat4 blockPreviewProj = glm::perspective(glm::radians(25.0f), 1.0f, 0.01f, 100.0f);
    const glm::mat4 blockPreviewView = glm::lookAt(
        glm::vec3(0.0f, 0.0f, 3.5f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::mat4 blockPreviewModel = glm::scale(
        glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(30.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
                    glm::radians(225.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
        glm::vec3(1.0f));
    
    // Собираем все предметы для списка
    std::vector<int> allItemIds;
    allItemIds.reserve(itemTypes.size());
    for (const auto& kv : itemTypes) allItemIds.push_back(kv.first);
    std::sort(allItemIds.begin(), allItemIds.end());
    
    const int listStartX = posX + static_cast<int>(8.0f * scaleX);
    const int listStartY = posY + static_cast<int>(17.0f * scaleY);
    const int listCols = 9;
    const int listRowsVisible = 5;
    
    const int totalRows = static_cast<int>((allItemIds.size() + listCols - 1) / listCols);
    const int maxScrollRow = std::max(0, totalRows - listRowsVisible);
    inventoryScrollRow = std::clamp(inventoryScrollRow, 0, maxScrollRow);
    
    // === Рисуем список всех предметов ===
    const int startIndex = inventoryScrollRow * listCols;
    for (int row = 0; row < listRowsVisible; ++row) {
        for (int col = 0; col < listCols; ++col) {
            const int idx = startIndex + row * listCols + col;
            if (idx < 0 || idx >= static_cast<int>(allItemIds.size())) continue;
            
            const int itemId = allItemIds[idx];
            const int itemSpacingX = 1; // Отступ между предметами по X
            const int itemSpacingY = 1; // Отступ между предметами по Y
            const int x = listStartX + col * (slotW + itemSpacingX);
            const int y = listStartY + row * (slotH + itemSpacingY);
            const int itemPadding = std::max(1, std::min(slotW, slotH) / 8);
            const int itemSize = std::max(8, std::min(slotW, slotH) - itemPadding * 2);
            
            const auto it = itemTypes.find(itemId);
            const bool isBlockItem = (it == itemTypes.end()) || it->second.isBlock;
            
            if (isBlockItem) {
                glEnable(GL_SCISSOR_TEST);
                glScissor(x, screenH - y - slotH, slotW, slotH);
                glEnable(GL_DEPTH_TEST);
                glDepthMask(GL_TRUE);
                glDisable(GL_BLEND);
                glEnable(GL_CULL_FACE);
                
                glUseProgram(shaderProgram);
                glUniformMatrix4fv(u_viewLoc, 1, GL_FALSE, glm::value_ptr(blockPreviewView));
                glUniformMatrix4fv(u_projLoc, 1, GL_FALSE, glm::value_ptr(blockPreviewProj));
                glUniformMatrix4fv(u_modelLoc, 1, GL_FALSE, glm::value_ptr(blockPreviewModel));
                glUniform1f(u_sunIntensity_location, 1.0f);
                glUniform1f(u_ambientBase_location, 0.55f);
                glUniform1i(u_isWater_location, 0);
                
                int viewportY = screenH - y - slotH + itemPadding;
                glViewport(x + itemPadding, viewportY, itemSize, itemSize);
                glClear(GL_DEPTH_BUFFER_BIT);
                renderSingleBlockModel(itemId);
                
                glViewport(oldViewport[0], oldViewport[1], oldViewport[2], oldViewport[3]);
                glDisable(GL_DEPTH_TEST);
                glDisable(GL_CULL_FACE);
                glDisable(GL_SCISSOR_TEST);
                glDepthMask(GL_FALSE);
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glUseProgram(uiShaderProgram);
            } else {
                renderItemIconFlat(itemId, x + itemPadding, y + itemPadding, itemSize, screenW, screenH);
            }
        }
    }
    
    // === Рисуем слоты инвентаря игрока (3x9) ===
    syncInventoryHotbarFromGameHotbar();
    
    const int playerInvStartX = posX + static_cast<int>(8.0f * scaleX);
    const int playerInvStartY = posY + static_cast<int>(58.0f * scaleY);
    
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 9; ++col) {
            const int idx = row * 9 + col;
            if (playerInventoryItems[idx].blockType == 0) continue;
            
            const int itemId = playerInventoryItems[idx].blockType;
            const int x = playerInvStartX + col * slotW;
            const int y = playerInvStartY + row * slotH;
            const int itemPadding = std::max(1, std::min(slotW, slotH) / 8);
            const int itemSize = std::max(8, std::min(slotW, slotH) - itemPadding * 2);
            
            const auto it = itemTypes.find(itemId);
            const bool isBlockItem = (it == itemTypes.end()) || it->second.isBlock;
            
            if (isBlockItem) {
                glEnable(GL_SCISSOR_TEST);
                glScissor(x, screenH - (y + slotH), slotW, slotH);
                glEnable(GL_DEPTH_TEST);
                glDepthMask(GL_TRUE);
                glDisable(GL_BLEND);
                glEnable(GL_CULL_FACE);
                
                glUseProgram(shaderProgram);
                glUniformMatrix4fv(u_viewLoc, 1, GL_FALSE, glm::value_ptr(blockPreviewView));
                glUniformMatrix4fv(u_projLoc, 1, GL_FALSE, glm::value_ptr(blockPreviewProj));
                glUniformMatrix4fv(u_modelLoc, 1, GL_FALSE, glm::value_ptr(blockPreviewModel));
                glUniform1f(u_sunIntensity_location, 1.0f);
                glUniform1f(u_ambientBase_location, 0.55f);
                glUniform1i(u_isWater_location, 0);
                
                glViewport(x + itemPadding, screenH - (y + itemPadding + itemSize), itemSize, itemSize);
                glClear(GL_DEPTH_BUFFER_BIT);
                renderSingleBlockModel(itemId);
                
                glViewport(oldViewport[0], oldViewport[1], oldViewport[2], oldViewport[3]);
                glDisable(GL_DEPTH_TEST);
                glDisable(GL_CULL_FACE);
                glDisable(GL_SCISSOR_TEST);
                glDepthMask(GL_FALSE);
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glUseProgram(uiShaderProgram);
            } else {
                renderItemIconFlat(itemId, x + itemPadding, y + itemPadding, itemSize, screenW, screenH);
            }
            
            // Отрисовка количества
            if (playerInventoryItems[idx].count > 1) {
                std::string countStr = std::to_string(playerInventoryItems[idx].count);
                drawMinecraftText(
                    countStr,
                    x + slotW - measureMinecraftTextWidth(countStr, 1.5f) - 2,
                    y + slotH - 12,
                    1.5f, screenW, screenH,
                    glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
            }
        }
    }
    
    // === Рисуем хотбар в инвентаре (последние 9 слотов) ===
    const int hbStartX = posX + static_cast<int>(6.0f * scaleX);
    const int hbStartY = posY + static_cast<int>(111.0f * scaleY);
    
    for (int i = 0; i < 9; ++i) {
        const int invIdx = 27 + i;
        if (playerInventoryItems[invIdx].blockType == 0) continue;
        
        const int itemId = playerInventoryItems[invIdx].blockType;
        const int x = hbStartX + i * slotW + 8;
        const int y = hbStartY;
        const int itemPadding = std::max(1, std::min(slotW, slotH) / 8);
        const int itemSize = std::max(8, std::min(slotW, slotH) - itemPadding * 2);
        
        const auto it = itemTypes.find(itemId);
        const bool isBlockItem = (it == itemTypes.end()) || it->second.isBlock;
        
        if (isBlockItem) {
            glEnable(GL_SCISSOR_TEST);
            glScissor(x, screenH - (y + slotH), slotW, slotH);
            glEnable(GL_DEPTH_TEST);
            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
            glEnable(GL_CULL_FACE);
            
            glUseProgram(shaderProgram);
            glUniformMatrix4fv(u_viewLoc, 1, GL_FALSE, glm::value_ptr(blockPreviewView));
            glUniformMatrix4fv(u_projLoc, 1, GL_FALSE, glm::value_ptr(blockPreviewProj));
            glUniformMatrix4fv(u_modelLoc, 1, GL_FALSE, glm::value_ptr(blockPreviewModel));
            glUniform1f(u_sunIntensity_location, 1.0f);
            glUniform1f(u_ambientBase_location, 0.55f);
            glUniform1i(u_isWater_location, 0);
            
            glViewport(x + itemPadding, screenH - (y + itemPadding + itemSize), itemSize, itemSize);
            glClear(GL_DEPTH_BUFFER_BIT);
            renderSingleBlockModel(itemId);
            
            glViewport(oldViewport[0], oldViewport[1], oldViewport[2], oldViewport[3]);
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_CULL_FACE);
            glDisable(GL_SCISSOR_TEST);
            glDepthMask(GL_FALSE);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glUseProgram(uiShaderProgram);
        } else {
            renderItemIconFlat(itemId, x + itemPadding, y + itemPadding, itemSize, screenW, screenH);
        }
        
        // Отрисовка количества
        if (playerInventoryItems[invIdx].count > 1) {
            std::string countStr = std::to_string(playerInventoryItems[invIdx].count);
            drawMinecraftText(
                countStr,
                x + slotW - measureMinecraftTextWidth(countStr, 1.5f) - 8,
                y + slotH - 24,
                2.1f, screenW, screenH,
                glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
        }
    }
    
    // === Рисуем скроллер ===
    const bool canScroll = maxScrollRow > 0;
    unsigned int scrollTex = canScroll ? inventoryScrollerTexture : inventoryScrollerDisabledTexture;
    if (scrollTex == 0) scrollTex = inventoryScrollerTexture;
    
    if (scrollTex != 0) {
        const int trackX = posX + static_cast<int>(174.0f * scaleX) + 1;
        const int trackTopY = posY + static_cast<int>(17.0f * scaleY);
        const int trackBottomY = posY + static_cast<int>(126.0f * scaleY);
        const int trackH = std::max(1, trackBottomY - trackTopY);
        const int scrollerW = std::max(8, static_cast<int>((186.0f - 174.0f) * scaleX));
        const int scrollerH = std::max(8, static_cast<int>(15.0f * scaleY));
        
        int scrollerY = trackTopY;
        if (canScroll) {
            const float t = (maxScrollRow > 0) ? (static_cast<float>(inventoryScrollRow) / maxScrollRow) : 0.0f;
            scrollerY = trackTopY + static_cast<int>(t * std::max(0, trackH - scrollerH));
        }
        
        glUseProgram(uiShaderProgram);
        glBindVertexArray(uiVAO);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        
        drawRectangle(static_cast<float>(trackX), static_cast<float>(scrollerY),
                     static_cast<float>(scrollerW), static_cast<float>(scrollerH),
                     scrollTex, screenW, screenH);
    }
    
    // Восстанавливаем состояния OpenGL
    glViewport(oldViewport[0], oldViewport[1], oldViewport[2], oldViewport[3]);
    glUseProgram(oldProgram);
    glBindVertexArray(oldVAO);
    
    if (depthEnabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (cullEnabled) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    if (blendEnabled) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    if (scissorEnabled) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
    glDepthMask(depthMask);
    glBlendFunc(blendSrc, blendDst);
}

void drawHUD(int screenW, int screenH, float currentTime)
{
    if (!hotbarTexture ||
        !heartFullTexture ||
        !heartHalfTexture ||
        !heartContTexture ||
        !hotbarSelTexture)
    {
        return;
    }

    // =========================================================
    // HOTBAR SETTINGS
    // =========================================================

    const int SLOT_SIZE = 60;
    const int SLOT_SPACING = -4;
    const int HOTBAR_SLOTS = 9;

    const int totalWidth =
        HOTBAR_SLOTS * SLOT_SIZE +
        (HOTBAR_SLOTS - 1) * SLOT_SPACING;

    const int startX = (screenW - totalWidth) / 2;
    const int startY = screenH - SLOT_SIZE - 5;

    // =========================================================
    // SAVE OPENGL STATE
    // =========================================================

    GLboolean depthEnabled = glIsEnabled(GL_DEPTH_TEST);
    GLboolean blendEnabled = glIsEnabled(GL_BLEND);
    GLboolean cullEnabled  = glIsEnabled(GL_CULL_FACE);
    GLboolean scissorEnabled = glIsEnabled(GL_SCISSOR_TEST);
    GLboolean prevDepthMask = GL_TRUE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &prevDepthMask);

    GLint oldViewport[4];
    glGetIntegerv(GL_VIEWPORT, oldViewport);

    // =========================================================
    // DRAW 2D HOTBAR BACKGROUND
    // =========================================================

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(uiShaderProgram);

    drawRectangle(
        startX,
        startY,
        totalWidth,
        SLOT_SIZE,
        hotbarTexture,
        screenW,
        screenH
    );

    // =========================================================
    // DRAW SELECTED SLOT
    // =========================================================

    {
        int selX = startX + currentHotbarSlot * (SLOT_SIZE + SLOT_SPACING);

        drawRectangle(
            selX - 4,
            startY - 4,
            SLOT_SIZE + 8,
            SLOT_SIZE + 8,
            hotbarSelTexture,
            screenW,
            screenH
        );
    }

    // =========================================================
    // PREPARE FOR ITEM RENDER
    // =========================================================

    glm::mat4 proj = glm::perspective(glm::radians(25.0f), 1.0f, 0.01f, 100.0f);
    glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 3.5f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    // =========================================================
    // DRAW 3D BLOCKS FIRST
    // =========================================================

    for (int i = 0; i < HOTBAR_SLOTS; i++)
    {
        const int invIdx = 27 + i;
        if (playerInventoryItems[invIdx].blockType == 0) continue;

        const int itemId = playerInventoryItems[invIdx].blockType;
        const auto itemIt = itemTypes.find(itemId);
        const bool isBlockItem = (itemIt == itemTypes.end()) || itemIt->second.isBlock;

        if (!isBlockItem) continue; // Пропускаем не-блоки, нарисуем их позже

        const int slotX = startX + i * (SLOT_SIZE + SLOT_SPACING);
        const int slotY = startY;
        const int itemPadding = 8;
        const int itemSize = SLOT_SIZE - itemPadding * 2;

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
        glEnable(GL_SCISSOR_TEST);
        glUseProgram(shaderProgram);

        glUniformMatrix4fv(u_viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(u_projLoc, 1, GL_FALSE, glm::value_ptr(proj));
        glUniform1f(u_sunIntensity_location, 1.0f);
        glUniform1f(u_ambientBase_location, 0.55f);
        glUniform1i(u_isWater_location, 0);

        int viewportX = slotX + itemPadding;
        int viewportY = screenH - (slotY + itemPadding + itemSize);
        glViewport(viewportX, viewportY, itemSize, itemSize);
        glScissor(viewportX, viewportY, itemSize, itemSize);
        glClear(GL_DEPTH_BUFFER_BIT);

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::rotate(model, glm::radians(30.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        model = glm::rotate(model, glm::radians(225.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::scale(model, glm::vec3(0.87f));
        glUniformMatrix4fv(u_modelLoc, 1, GL_FALSE, glm::value_ptr(model));
        
        glDisable(GL_CULL_FACE);
        renderSingleBlockModel(itemId);
        glEnable(GL_CULL_FACE);
    }

    // =========================================================
    // RESTORE VIEWPORT AND STATE FOR 2D ITEMS
    // =========================================================

    glViewport(oldViewport[0], oldViewport[1], oldViewport[2], oldViewport[3]);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glUseProgram(uiShaderProgram);

    // =========================================================
    // DRAW 2D ITEMS SECOND
    // =========================================================

    for (int i = 0; i < HOTBAR_SLOTS; i++)
    {
        const int invIdx = 27 + i;
        if (playerInventoryItems[invIdx].blockType == 0) continue;

        const int itemId = playerInventoryItems[invIdx].blockType;
        const auto itemIt = itemTypes.find(itemId);
        const bool isBlockItem = (itemIt == itemTypes.end()) || itemIt->second.isBlock;

        if (isBlockItem) continue; // Блоки уже нарисованы

        const int slotX = startX + i * (SLOT_SIZE + SLOT_SPACING);
        const int slotY = startY;
        const int itemPadding = 10;
        const int itemSize = SLOT_SIZE - itemPadding * 2;

        renderItemIconFlat(itemId, slotX + itemPadding, slotY + itemPadding, itemSize, screenW, screenH);

        // Отрисовка количества
        if (playerInventoryItems[invIdx].count > 1) {
            std::string countStr = std::to_string(playerInventoryItems[invIdx].count);
            drawMinecraftText(
                countStr,
                slotX + SLOT_SIZE - measureMinecraftTextWidth(countStr, 1.5f) - 6,
                slotY + SLOT_SIZE - 12,
                1.5f, screenW, screenH,
                glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
        }
    }
    // Отрисовка количества для блоков (которые уже нарисованы в 3D)
    for (int i = 0; i < HOTBAR_SLOTS; i++)
    {
        const int invIdx = 27 + i;
        if (playerInventoryItems[invIdx].blockType == 0) continue;
        if (playerInventoryItems[invIdx].count <= 1) continue; // Показываем только если >1
        
        const int itemId = playerInventoryItems[invIdx].blockType;
        const auto itemIt = itemTypes.find(itemId);
        const bool isBlockItem = (itemIt == itemTypes.end()) || itemIt->second.isBlock;
        
        if (!isBlockItem) continue; // Только для блоков
        
        const int slotX = startX + i * (SLOT_SIZE + SLOT_SPACING);
        const int slotY = startY;
        
        std::string countStr = std::to_string(playerInventoryItems[invIdx].count);
        drawMinecraftText(
            countStr,
            slotX + SLOT_SIZE - measureMinecraftTextWidth(countStr, 1.5f) - 6,
            slotY + SLOT_SIZE - 12,
            1.5f, screenW, screenH,
            glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    }

    // =========================================================
    // DRAW HEARTS
    // =========================================================

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(uiShaderProgram);

    const int HEART_SIZE = 25;
    const int HEART_SPACING = -3;

    int heartsTotal = MAX_PLAYER_HEALTH / 2;

    int heartsX = startX;
    int heartsY = startY - HEART_SIZE - 6;

    for (int i = 0; i < heartsTotal; i++)
    {
        int x = heartsX + i * (HEART_SIZE + HEART_SPACING);

        drawRectangle(
            x,
            heartsY,
            HEART_SIZE,
            HEART_SIZE,
            heartContTexture,
            screenW,
            screenH
        );

        int hp = playerHealth - i * 2;

        unsigned int tex = 0;

        if (hp >= 2)
            tex = heartFullTexture;
        else if (hp == 1)
            tex = heartHalfTexture;

        if (tex)
        {
            drawRectangle(
                x,
                heartsY,
                HEART_SIZE,
                HEART_SIZE,
                tex,
                screenW,
                screenH
            );
        }
    }

    // =========================================================
    // RESTORE OPENGL STATE
    // =========================================================

    if (depthEnabled)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);

    if (blendEnabled)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);

    if (cullEnabled)
        glEnable(GL_CULL_FACE);
    else
        glDisable(GL_CULL_FACE);

    glDepthMask(prevDepthMask);
}

// ----------------------------------------------------------------------
// Система настроения
// ----------------------------------------------------------------------
float currentMood = 0.0f;        // Текущее настроение (0-100%)
float lastMoodCheckTime = 0.0f;  // Время последней проверки
std::vector<sf::SoundBuffer> ambientSoundBuffers;  // Хранилище буферов звуков
sf::Sound* ambientSound = nullptr;  // Звук для воспроизведения (используем обычный указатель)
bool ambientSoundPlaying = false;

// ----------------------------------------------------------------------
// Музыка (без изменений)
// ----------------------------------------------------------------------
std::vector<std::string> musicFiles;
sf::Music currentMusic;
int currentTrackIndex = -1;
bool musicPlaying = false, musicTransitioning = false;
float trackStartTime = 0.0f;

void playRandomMusic() {
    if (musicFiles.empty() || musicTransitioning) return;
    musicTransitioning = true;
    static std::random_device rd; static std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, (int)musicFiles.size() - 1);
    int newIndex = dis(gen);
    if (newIndex == currentTrackIndex && musicFiles.size() > 1) newIndex = (newIndex + 1) % musicFiles.size();
    currentTrackIndex = newIndex;
    std::string path = musicFiles[currentTrackIndex];
    currentMusic.stop();
    if (currentMusic.openFromFile(path)) { currentMusic.play(); musicPlaying = true; trackStartTime = glfwGetTime(); }
    musicTransitioning = false;
}
void stopMusic() { musicPlaying = false; musicTransitioning = false; currentMusic.stop(); }
void startMusic() { if (musicFiles.empty()) return; musicPlaying = true; playRandomMusic(); }
void scanMusicFolder() {
    std::string musicDir = "music";
    if (!fs::exists(musicDir)) fs::create_directory(musicDir);
    for (const auto& entry : fs::directory_iterator(musicDir))
        if (entry.is_regular_file()) {
            std::string ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext == ".wav" || ext == ".ogg" || ext == ".flac" || ext == ".mp3")
                musicFiles.push_back(entry.path().string());
        }
}
void updateMusic() {
    if (!musicPlaying || musicTransitioning) return;
    if (currentMusic.getStatus() == sf::SoundSource::Status::Stopped && glfwGetTime() - trackStartTime > 3.0f) {
        musicPlaying = false; playRandomMusic();
    }
}

// ----------------------------------------------------------------------
// Структура чанка (с обновлённой системой освещения)
// ----------------------------------------------------------------------
struct Chunk {
    glm::ivec2 pos;
    std::shared_ptr<ChunkData> data;
    unsigned int vao[256] = {0}, vbo[256] = {0};
    size_t vertexCount[256] = {0};
    bool meshReady = false, dirty = false;

    Chunk(int cx, int cz, bool loadFromFile) : pos(cx, cz) {
        std::lock_guard<std::mutex> lock(chunkMutex);
        if (loadFromFile) {
            std::string filename = (getCurrentChunksPath() / ("chunk_" + std::to_string(cx) + "_" + std::to_string(cz) + ".bin")).string();
            if (fs::exists(filename)) pendingLoad.insert(pos);
            else pendingGen.insert(pos);
        } else pendingGen.insert(pos);
    }
    ~Chunk() { for (int i=0; i<256; ++i) if (vao[i]) { glDeleteVertexArrays(1, &vao[i]); glDeleteBuffers(1, &vbo[i]); } }

    bool updateData() {
        if (data) return true;
        std::lock_guard<std::mutex> lock(chunkMutex);
        auto it = pendingData.find(pos);
        if (it != pendingData.end()) {
            data = it->second; pendingData.erase(it);
            meshReady = false; dirty = false; invalidateNeighbors();
            return true;
        }
        return false;
    }
    void invalidateNeighbors() {
        glm::ivec2 neighbors[4] = {{pos.x-1,pos.y},{pos.x+1,pos.y},{pos.x,pos.y-1},{pos.x,pos.y+1}};
        for (auto& npos : neighbors) {
            auto it = loadedChunks.find(npos);
            if (it != loadedChunks.end()) it->second.meshReady = false;
        }
    }
    int getLocalBlock(int x, int y, int z) const { return data ? data->blocks[(x * CHUNK_SIZE_Y + y) * CHUNK_SIZE_Z + z] : 0; }
    uint8_t getLocalSkyLight(int x, int y, int z) const { return data ? data->skyLight[(x * CHUNK_SIZE_Y + y) * CHUNK_SIZE_Z + z] : 0; }
    uint8_t getLocalBlockLight(int x, int y, int z) const { return data ? data->blockLight[(x * CHUNK_SIZE_Y + y) * CHUNK_SIZE_Z + z] : 0; }

    void setLocalSkyLight(int x, int y, int z, uint8_t val) {
        if (!data) return;
        data->skyLight[(x * CHUNK_SIZE_Y + y) * CHUNK_SIZE_Z + z] = val;
        // можно не помечать грязным меш, т.к. освещение обновляется глобально
    }
    void setLocalBlockLight(int x, int y, int z, uint8_t val) {
        if (!data) return;
        data->blockLight[(x * CHUNK_SIZE_Y + y) * CHUNK_SIZE_Z + z] = val;
    }

    void setLocalBlock(int x, int y, int z, int id) {
        if (!data) return;
        int idx = (x * CHUNK_SIZE_Y + y) * CHUNK_SIZE_Z + z;
        int oldId = data->blocks[idx];
        data->blocks[idx] = id;
        meshReady = false; dirty = true;
        // Глобальное обновление света будет вызвано из setBlockAt
    }

    bool isAOSolid(int wx, int wy, int wz) const {
        int blockId = getBlockAtForMesh(wx, wy, wz);
        if (blockId == BLOCK_UNKNOWN) return false;
        return isOpaque(blockId);
    }

    float getVertexAO(int lx, int ly, int lz, const glm::vec3& normal, const glm::vec3& vertexOffset) const {
        int wx = pos.x * CHUNK_SIZE_X + lx;
        int wz = pos.y * CHUNK_SIZE_Z + lz;
    
        int sx = (vertexOffset.x >= 0.0f) ? 1 : -1;
        int sy = (vertexOffset.y >= 0.0f) ? 1 : -1;
        int sz = (vertexOffset.z >= 0.0f) ? 1 : -1;
    
        bool side1 = false;
        bool side2 = false;
        bool corner = false;
    
        if (normal.y > 0.5f) {
            // Верхняя грань: проверяем блоки НАД
            side1 = isAOSolid(wx + sx, ly + 1, wz);
            side2 = isAOSolid(wx, ly + 1, wz + sz);
            corner = isAOSolid(wx + sx, ly + 1, wz + sz);
        } else if (normal.y < -0.5f) {
            // Нижняя грань: проверяем блоки ПОД
            side1 = isAOSolid(wx + sx, ly - 1, wz);
            side2 = isAOSolid(wx, ly - 1, wz + sz);
            corner = isAOSolid(wx + sx, ly - 1, wz + sz);
        } else if (normal.x > 0.5f) {
            // Правая грань
            side1 = isAOSolid(wx + 1, ly + sy, wz);
            side2 = isAOSolid(wx + 1, ly, wz + sz);
            corner = isAOSolid(wx + 1, ly + sy, wz + sz);
        } else if (normal.x < -0.5f) {
            // Левая грань
            side1 = isAOSolid(wx - 1, ly + sy, wz);
            side2 = isAOSolid(wx - 1, ly, wz + sz);
            corner = isAOSolid(wx - 1, ly + sy, wz + sz);
        } else if (normal.z > 0.5f) {
            // Передняя грань
            side1 = isAOSolid(wx + sx, ly, wz + 1);
            side2 = isAOSolid(wx, ly + sy, wz + 1);
            corner = isAOSolid(wx + sx, ly + sy, wz + 1);
        } else if (normal.z < -0.5f) {
            // Задняя грань
            side1 = isAOSolid(wx + sx, ly, wz - 1);
            side2 = isAOSolid(wx, ly + sy, wz - 1);
            corner = isAOSolid(wx + sx, ly + sy, wz - 1);
        }
    
        int occlusion = (side1 && side2) ? 3 : (int)side1 + (int)side2 + (int)corner;
        static const float aoLevels[4] = {1.0f, 0.93f, 0.78f, 0.64f};
        return aoLevels[occlusion];
    }

    // Вычисление освещённости вершины (использует глобальные функции для соседних чанков)
    float getVertexLight(int lx, int ly, int lz, const glm::vec3& normal, const glm::vec3& vertexOffset, const std::array<glm::ivec3,4>& neighborOffsets) {
        int wx = pos.x * CHUNK_SIZE_X + lx;
        int wz = pos.y * CHUNK_SIZE_Z + lz;
        float ao = getVertexAO(lx, ly, lz, normal, vertexOffset);
        auto boostedLight = [](uint8_t skyLight, uint8_t blockLight) {
            float boostedBlockLight = glm::min(static_cast<float>(MAX_LIGHT), blockLight * 1.1f);
            float dominant = glm::max(static_cast<float>(skyLight), boostedBlockLight);
            return dominant / static_cast<float>(MAX_LIGHT);
        };
    
        if (normal.y > 0.5f) {
            // Верхняя грань - сэмплируем 4 точки над блоком
            int sampleY = ly + 1;
            if (sampleY < 0 || sampleY >= CHUNK_SIZE_Y) return 1.0f;
    
            int sx = (vertexOffset.x >= 0.0f) ? 1 : -1;
            int sz = (vertexOffset.z >= 0.0f) ? 1 : -1;
    
            float totalLight = 0.0f;
            int count = 0;
    
            const int offsets[4][2] = {
                {0, 0},
                {sx, 0},
                {0, sz},
                {sx, sz}
            };
    
            for (int i = 0; i < 4; ++i) {
                int sampleWX = wx + offsets[i][0];
                int sampleWZ = wz + offsets[i][1];
                uint8_t sl = getSkyLightAt(sampleWX, sampleY, sampleWZ);
                uint8_t bl = getBlockLightAt(sampleWX, sampleY, sampleWZ);
                totalLight += boostedLight(sl, bl);
                count++;
            }
    
            float baseLight = count > 0 ? totalLight / count : 1.0f;
            return baseLight * ao;
        }
    
        if (normal.y < -0.5f) {
            // Нижняя грань - ТОЖЕ сэмплируем 4 точки под блоком
            int sampleY = ly - 1;
            if (sampleY < 0 || sampleY >= CHUNK_SIZE_Y) return 0.0f;
    
            int sx = (vertexOffset.x >= 0.0f) ? 1 : -1;
            int sz = (vertexOffset.z >= 0.0f) ? 1 : -1;
    
            float totalLight = 0.0f;
            int count = 0;
    
            const int offsets[4][2] = {
                {0, 0},
                {sx, 0},
                {0, sz},
                {sx, sz}
            };
    
            for (int i = 0; i < 4; ++i) {
                int sampleWX = wx + offsets[i][0];
                int sampleWZ = wz + offsets[i][1];
                uint8_t sl = getSkyLightAt(sampleWX, sampleY, sampleWZ);
                uint8_t bl = getBlockLightAt(sampleWX, sampleY, sampleWZ);
                totalLight += boostedLight(sl, bl);
                count++;
            }
    
            float baseLight = count > 0 ? totalLight / count : 0.0f;
            return baseLight * ao;
        }
    
        // Боковые грани - используем neighborOffsets (уже 4 точки)
        float totalLight = 0.0f;
        int count = 0;
        for (const auto& off : neighborOffsets) {
            int nx = lx + off.x, ny = ly + off.y, nz = lz + off.z;
            int sampleWX = pos.x * CHUNK_SIZE_X + nx;
            int sampleWZ = pos.y * CHUNK_SIZE_Z + nz;
            if (ny < 0 || ny >= CHUNK_SIZE_Y) continue;
            uint8_t sl = getSkyLightAt(sampleWX, ny, sampleWZ);
            uint8_t bl = getBlockLightAt(sampleWX, ny, sampleWZ);
            totalLight += boostedLight(sl, bl);
            count++;
        }
        float baseLight = count > 0 ? totalLight / count : 1.0f;
        return baseLight * ao;
    }

    float getVertexBlockLight(int lx, int ly, int lz, const glm::vec3& normal, const glm::vec3& vertexOffset, const std::array<glm::ivec3,4>& neighborOffsets) {
        int wx = pos.x * CHUNK_SIZE_X + lx;
        int wz = pos.y * CHUNK_SIZE_Z + lz;

        auto normBlock = [](uint8_t blockLight) {
            return static_cast<float>(blockLight) / static_cast<float>(MAX_LIGHT);
        };

        if (normal.y > 0.5f || normal.y < -0.5f) {
            int sampleY = normal.y > 0.5f ? ly + 1 : ly - 1;
            if (sampleY < 0 || sampleY >= CHUNK_SIZE_Y) return 0.0f;
            int sx = (vertexOffset.x >= 0.0f) ? 1 : -1;
            int sz = (vertexOffset.z >= 0.0f) ? 1 : -1;
            const int offsets[4][2] = {{0,0},{sx,0},{0,sz},{sx,sz}};
            float total = 0.0f;
            for (int i = 0; i < 4; ++i) {
                int sampleWX = wx + offsets[i][0];
                int sampleWZ = wz + offsets[i][1];
                total += normBlock(getBlockLightAt(sampleWX, sampleY, sampleWZ));
            }
            return total * 0.25f;
        }

        float total = 0.0f;
        int count = 0;
        for (const auto& off : neighborOffsets) {
            int nx = lx + off.x, ny = ly + off.y, nz = lz + off.z;
            int sampleWX = pos.x * CHUNK_SIZE_X + nx;
            int sampleWZ = pos.y * CHUNK_SIZE_Z + nz;
            if (ny < 0 || ny >= CHUNK_SIZE_Y) continue;
            total += normBlock(getBlockLightAt(sampleWX, ny, sampleWZ));
            ++count;
        }
        return count > 0 ? total / count : 0.0f;
    }

    void buildMesh() {
        if (!data) return;
        for (int i=0; i<256; ++i) if (vao[i]) { glDeleteVertexArrays(1, &vao[i]); glDeleteBuffers(1, &vbo[i]); vao[i]=vbo[i]=0; vertexCount[i]=0; }
        std::unordered_map<int, std::vector<float>> verticesPerType;
        const float leftFace[] = { -0.5f,-0.5f,-0.5f, -0.5f,-0.5f,0.5f, -0.5f,0.5f,0.5f, -0.5f,0.5f,0.5f, -0.5f,0.5f,-0.5f, -0.5f,-0.5f,-0.5f };
        const float rightFace[] = { 0.5f,-0.5f,0.5f, 0.5f,-0.5f,-0.5f, 0.5f,0.5f,-0.5f, 0.5f,0.5f,-0.5f, 0.5f,0.5f,0.5f, 0.5f,-0.5f,0.5f };
        const float bottomFace[] = { -0.5f,-0.5f,-0.5f, 0.5f,-0.5f,-0.5f, 0.5f,-0.5f,0.5f, 0.5f,-0.5f,0.5f, -0.5f,-0.5f,0.5f, -0.5f,-0.5f,-0.5f };
        const float topFace[] = { -0.5f,0.5f,0.5f, 0.5f,0.5f,0.5f, 0.5f,0.5f,-0.5f, 0.5f,0.5f,-0.5f, -0.5f,0.5f,-0.5f, -0.5f,0.5f,0.5f };
        const float frontFace[] = { -0.5f,-0.5f,0.5f, 0.5f,-0.5f,0.5f, 0.5f,0.5f,0.5f, 0.5f,0.5f,0.5f, -0.5f,0.5f,0.5f, -0.5f,-0.5f,0.5f };
        const float backFace[] = { 0.5f,-0.5f,-0.5f, -0.5f,-0.5f,-0.5f, -0.5f,0.5f,-0.5f, -0.5f,0.5f,-0.5f, 0.5f,0.5f,-0.5f, 0.5f,-0.5f,-0.5f };
        const float baseUV[] = { 0,0, 1,0, 1,1, 1,1, 0,1, 0,0 };
    
        const std::array<std::array<glm::ivec3,4>,6> faceNeighborOffsets = {{
            { glm::ivec3(-1,0,0), glm::ivec3(-1,1,0), glm::ivec3(-1,0,1), glm::ivec3(-1,1,1) },
            { glm::ivec3(1,0,0), glm::ivec3(1,1,0), glm::ivec3(1,0,1), glm::ivec3(1,1,1) },
            { glm::ivec3(0,-1,0), glm::ivec3(1,-1,0), glm::ivec3(0,-1,1), glm::ivec3(1,-1,1) },
            { glm::ivec3(0,1,0), glm::ivec3(1,1,0), glm::ivec3(0,1,1), glm::ivec3(1,1,1) },
            { glm::ivec3(0,0,1), glm::ivec3(1,0,1), glm::ivec3(0,1,1), glm::ivec3(1,1,1) },
            { glm::ivec3(0,0,-1), glm::ivec3(1,0,-1), glm::ivec3(0,1,-1), glm::ivec3(1,1,-1) }
        }};
    
        for (int x=0; x<CHUNK_SIZE_X; ++x) for (int y=0; y<CHUNK_SIZE_Y; ++y) for (int z=0; z<CHUNK_SIZE_Z; ++z) {
            int type = getLocalBlock(x,y,z); if (type==0) continue;
            float ox = pos.x*CHUNK_SIZE_X + x, oy = y, oz = pos.y*CHUNK_SIZE_Z + z;
            
            // Получаем количество частей текстуры для этого типа блока
            int textureParts = 3; // По умолчанию
            auto btIt = blockTypes.find(type);
            if (btIt != blockTypes.end()) {
                textureParts = btIt->second.textureParts;
            }
            
            // Функция для получения UV-смещения и масштаба в зависимости от грани
            auto getUVForFace = [&](int faceIndex, float& uOff, float& uScale) {
                uScale = 1.0f / textureParts;
                
                if (textureParts == 3) {
                    // Стандарт: верх (0), бок (1), низ (2)
                    if (faceIndex == 3) uOff = 0.0f;           // Верх
                    else if (faceIndex == 2) uOff = 2.0f * uScale; // Низ
                    else uOff = uScale;                        // Боковые грани
                }
                else if (textureParts == 4) {
                    // Верх (0), бок1 (1), бок2 (2), низ (3)
                    if (faceIndex == 3) uOff = 0.0f;           // Верх
                    else if (faceIndex == 2) uOff = 3.0f * uScale; // Низ
                    else if (faceIndex == 0 || faceIndex == 1) // Лево/право
                        uOff = 1.0f * uScale;                  // Бок1
                    else                                        // Перед/зад
                        uOff = 2.0f * uScale;                  // Бок2
                }
                else if (textureParts == 6) {
                    // Верх (0), перед (1), право (2), зад (3), лево (4), низ (5)
                    switch(faceIndex) {
                        case 3: uOff = 0.0f; break;            // Верх
                        case 4: uOff = 1.0f * uScale; break;   // Перед
                        case 1: uOff = 2.0f * uScale; break;   // Право
                        case 5: uOff = 3.0f * uScale; break;   // Зад
                        case 0: uOff = 4.0f * uScale; break;   // Лево
                        case 2: uOff = 5.0f * uScale; break;   // Низ
                    }
                }
            };
            
            auto addFace = [&](const float* face, int faceIdx, std::vector<float>& out) {
                glm::vec3 normal;
                switch(faceIdx) {
                    case 0: normal = glm::vec3(-1,0,0); break;
                    case 1: normal = glm::vec3(1,0,0); break;
                    case 2: normal = glm::vec3(0,-1,0); break;
                    case 3: normal = glm::vec3(0,1,0); break;
                    case 4: normal = glm::vec3(0,0,1); break;
                    case 5: normal = glm::vec3(0,0,-1); break;
                }
                
                float uOff, uScale;
                getUVForFace(faceIdx, uOff, uScale);
                
                for (int i=0; i<18; i+=3) {
                    out.push_back(face[i]+ox); 
                    out.push_back(face[i+1]+oy); 
                    out.push_back(face[i+2]+oz);
                    
                    int uvIdx = (i/3)*2; 
                    float u = baseUV[uvIdx], v = baseUV[uvIdx+1];
                    
                    // Для боковых граней в 3-частевой текстуре переворачиваем V
                    if (textureParts == 3 && faceIdx != 2 && faceIdx != 3) {
                        v = 1.0f - v;
                    }
                    
                    out.push_back(uOff + u * uScale);
                    out.push_back(v);
                    out.push_back(normal.x); 
                    out.push_back(normal.y); 
                    out.push_back(normal.z);
                    
                    glm::vec3 vertexOffset(face[i], face[i+1], face[i+2]);
                    float light = getVertexLight(x, y, z, normal, vertexOffset, faceNeighborOffsets[faceIdx]);
                    float blockLight = getVertexBlockLight(x, y, z, normal, vertexOffset, faceNeighborOffsets[faceIdx]);
                    out.push_back(light);
                    out.push_back(blockLight);
                }
            };
            
            std::vector<float>& verts = verticesPerType[type];
            int neighbor;
            if (type==5) {
                neighbor=getBlockAtForMesh(ox-1,oy,oz); if(neighbor!=5&&neighbor!=BLOCK_UNKNOWN&&!isOpaque(neighbor)) addFace(leftFace,0,verts);
                neighbor=getBlockAtForMesh(ox+1,oy,oz); if(neighbor!=5&&neighbor!=BLOCK_UNKNOWN&&!isOpaque(neighbor)) addFace(rightFace,1,verts);
                neighbor=getBlockAtForMesh(ox,oy,oz+1); if(neighbor!=5&&neighbor!=BLOCK_UNKNOWN&&!isOpaque(neighbor)) addFace(frontFace,4,verts);
                neighbor=getBlockAtForMesh(ox,oy,oz-1); if(neighbor!=5&&neighbor!=BLOCK_UNKNOWN&&!isOpaque(neighbor)) addFace(backFace,5,verts);
                neighbor=getBlockAtForMesh(ox,oy+1,oz); if(neighbor!=5&&neighbor!=BLOCK_UNKNOWN&&!isOpaque(neighbor)) addFace(topFace,3,verts);
                neighbor=getBlockAtForMesh(ox,oy-1,oz); if(neighbor!=5&&neighbor!=BLOCK_UNKNOWN&&!isOpaque(neighbor)) addFace(bottomFace,2,verts);
            }
            else {
                neighbor=getBlockAtForMesh(ox-1,oy,oz); if(neighbor==0||neighbor==5) addFace(leftFace,0,verts);
                neighbor=getBlockAtForMesh(ox+1,oy,oz); if(neighbor==0||neighbor==5) addFace(rightFace,1,verts);
                neighbor=getBlockAtForMesh(ox,oy,oz+1); if(neighbor==0||neighbor==5) addFace(frontFace,4,verts);
                neighbor=getBlockAtForMesh(ox,oy,oz-1); if(neighbor==0||neighbor==5) addFace(backFace,5,verts);
                neighbor=getBlockAtForMesh(ox,oy+1,oz); if(neighbor==0||neighbor==5) addFace(topFace,3,verts);
                neighbor=getBlockAtForMesh(ox,oy-1,oz); if(neighbor==0||neighbor==5) addFace(bottomFace,2,verts);
            }
        }
        for (auto& [type, verts] : verticesPerType) {
            if (verts.empty()) continue;
            glGenVertexArrays(1, &vao[type]); glGenBuffers(1, &vbo[type]);
            glBindVertexArray(vao[type]); glBindBuffer(GL_ARRAY_BUFFER, vbo[type]);
            glBufferData(GL_ARRAY_BUFFER, verts.size()*sizeof(float), verts.data(), GL_STATIC_DRAW);
            glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,10*sizeof(float),(void*)0); glEnableVertexAttribArray(0);
            glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,10*sizeof(float),(void*)(3*sizeof(float))); glEnableVertexAttribArray(1);
            glVertexAttribPointer(2,3,GL_FLOAT,GL_FALSE,10*sizeof(float),(void*)(5*sizeof(float))); glEnableVertexAttribArray(2);
            glVertexAttribPointer(3,1,GL_FLOAT,GL_FALSE,10*sizeof(float),(void*)(8*sizeof(float))); glEnableVertexAttribArray(3);
            glVertexAttribPointer(4,1,GL_FLOAT,GL_FALSE,10*sizeof(float),(void*)(9*sizeof(float))); glEnableVertexAttribArray(4);
            vertexCount[type] = verts.size() / 10;
        }
        meshReady = true;
    }

    void render() {
        if (!data || !meshReady) return;
        for (int type=0; type<256; ++type) {
            if (type==5 || !vao[type]) continue;
            auto it = blockTypes.find(type); if (it==blockTypes.end()) continue;
            glUniform1i(u_isWater_location, 0);
            glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, it->second.textureID);
            glBindVertexArray(vao[type]); glDrawArrays(GL_TRIANGLES, 0, vertexCount[type]);
        }
    }
    void renderWater() {
        if (!meshReady || !vao[5]) return;
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); glDepthMask(GL_FALSE);
        glUniform1i(u_isWater_location, 1);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, blockTypes[5].textureID);
        glBindVertexArray(vao[5]); glDrawArrays(GL_TRIANGLES, 0, vertexCount[5]);
        glDepthMask(GL_TRUE); glDisable(GL_BLEND);
    }
    void saveAsync() {
        if (!dirty || !data) return; dirty = false;
        auto blocksCopy = data->blocks;
        auto skyCopy = data->skyLight;
        auto blockCopy = data->blockLight;
        glm::ivec2 posCopy = pos;
        fs::path chunksPathCopy = getCurrentChunksPath();
        std::thread([chunksPathCopy, posCopy, blocksCopy, skyCopy, blockCopy]() {
            saveChunkToFile(chunksPathCopy, posCopy, blocksCopy, skyCopy, blockCopy);
        }).detach();
    }
};

void rebuildBlockLightRegion(const LightRegion& rawRegion) {
    LightRegion region = rawRegion;
    region.minY = std::max(0, region.minY);
    region.maxY = std::min(CHUNK_SIZE_Y - 1, region.maxY);

    if (region.minY > region.maxY) return;

    for (int x = region.minX; x <= region.maxX; ++x) {
        for (int y = region.minY; y <= region.maxY; ++y) {
            for (int z = region.minZ; z <= region.maxZ; ++z) {
                setBlockLightAt(x, y, z, 0);
            }
        }
    }

    int sourceMinX = region.minX - maxBlockLightRadius;
    int sourceMaxX = region.maxX + maxBlockLightRadius;
    int sourceMinY = std::max(0, region.minY - maxBlockLightRadius);
    int sourceMaxY = std::min(CHUNK_SIZE_Y - 1, region.maxY + maxBlockLightRadius);
    int sourceMinZ = region.minZ - maxBlockLightRadius;
    int sourceMaxZ = region.maxZ + maxBlockLightRadius;

    for (int x = sourceMinX; x <= sourceMaxX; ++x) {
        for (int y = sourceMinY; y <= sourceMaxY; ++y) {
            for (int z = sourceMinZ; z <= sourceMaxZ; ++z) {
                int blockId = getBlockAt(x, y, z);
                int radius = (blockId >= 0 && blockId < 256) ? blockLightEmission[blockId] : 0;
                if (radius > 0) {
                    addBlockLightInRegion(x, y, z, static_cast<uint8_t>(radius), region);
                }
            }
        }
    }
}

// Глобальные функции доступа к свету (реализация)
uint8_t getSkyLightAt(int wx, int wy, int wz) {
    if (wy < 0 || wy >= CHUNK_SIZE_Y) return 0;
    int cx = (wx >= 0) ? wx / CHUNK_SIZE_X : (wx - CHUNK_SIZE_X + 1) / CHUNK_SIZE_X;
    int cz = (wz >= 0) ? wz / CHUNK_SIZE_Z : (wz - CHUNK_SIZE_Z + 1) / CHUNK_SIZE_Z;
    auto it = loadedChunks.find({cx, cz});
    if (it != loadedChunks.end()) {
        int lx = wx - cx * CHUNK_SIZE_X;
        int lz = wz - cz * CHUNK_SIZE_Z;
        return it->second.getLocalSkyLight(lx, wy, lz);
    }
    return 0;
}
uint8_t getBlockLightAt(int wx, int wy, int wz) {
    if (wy < 0 || wy >= CHUNK_SIZE_Y) return 0;
    int cx = (wx >= 0) ? wx / CHUNK_SIZE_X : (wx - CHUNK_SIZE_X + 1) / CHUNK_SIZE_X;
    int cz = (wz >= 0) ? wz / CHUNK_SIZE_Z : (wz - CHUNK_SIZE_Z + 1) / CHUNK_SIZE_Z;
    auto it = loadedChunks.find({cx, cz});
    if (it != loadedChunks.end()) {
        int lx = wx - cx * CHUNK_SIZE_X;
        int lz = wz - cz * CHUNK_SIZE_Z;
        return it->second.getLocalBlockLight(lx, wy, lz);
    }
    return 0;
}
void setSkyLightAt(int wx, int wy, int wz, uint8_t value) {
    if (wy < 0 || wy >= CHUNK_SIZE_Y) return;
    int cx = (wx >= 0) ? wx / CHUNK_SIZE_X : (wx - CHUNK_SIZE_X + 1) / CHUNK_SIZE_X;
    int cz = (wz >= 0) ? wz / CHUNK_SIZE_Z : (wz - CHUNK_SIZE_Z + 1) / CHUNK_SIZE_Z;
    auto it = loadedChunks.find({cx, cz});
    if (it != loadedChunks.end()) {
        int lx = wx - cx * CHUNK_SIZE_X;
        int lz = wz - cz * CHUNK_SIZE_Z;
        it->second.setLocalSkyLight(lx, wy, lz, value);
        it->second.meshReady = false; // освещение изменилось
    }
}
void setBlockLightAt(int wx, int wy, int wz, uint8_t value) {
    if (wy < 0 || wy >= CHUNK_SIZE_Y) return;
    int cx = (wx >= 0) ? wx / CHUNK_SIZE_X : (wx - CHUNK_SIZE_X + 1) / CHUNK_SIZE_X;
    int cz = (wz >= 0) ? wz / CHUNK_SIZE_Z : (wz - CHUNK_SIZE_Z + 1) / CHUNK_SIZE_Z;
    auto it = loadedChunks.find({cx, cz});
    if (it != loadedChunks.end()) {
        int lx = wx - cx * CHUNK_SIZE_X;
        int lz = wz - cz * CHUNK_SIZE_Z;
        it->second.setLocalBlockLight(lx, wy, lz, value);
        it->second.meshReady = false;
    }
}

int getBlockAtForMesh(int wx, int wy, int wz) {
    if (wy<0 || wy>=CHUNK_SIZE_Y) return 0;
    int cx = (wx>=0) ? wx/CHUNK_SIZE_X : (wx-CHUNK_SIZE_X+1)/CHUNK_SIZE_X;
    int cz = (wz>=0) ? wz/CHUNK_SIZE_Z : (wz-CHUNK_SIZE_Z+1)/CHUNK_SIZE_Z;
    if (lastChunkForMesh && lastChunkCoordsForMesh.x==cx && lastChunkCoordsForMesh.y==cz) {
        int lx = wx - cx*CHUNK_SIZE_X, lz = wz - cz*CHUNK_SIZE_Z;
        if (lx>=0&&lx<CHUNK_SIZE_X && lz>=0&&lz<CHUNK_SIZE_Z) return lastChunkForMesh->getLocalBlock(lx,wy,lz);
        return BLOCK_UNKNOWN;
    }
    auto it = loadedChunks.find({cx,cz});
    if (it != loadedChunks.end()) {
        lastChunkForMesh = &it->second; lastChunkCoordsForMesh = {cx,cz};
        int lx = wx - cx*CHUNK_SIZE_X, lz = wz - cz*CHUNK_SIZE_Z;
        if (lx>=0&&lx<CHUNK_SIZE_X && lz>=0&&lz<CHUNK_SIZE_Z) return it->second.getLocalBlock(lx,wy,lz);
    }
    return BLOCK_UNKNOWN;
}
int getBlockAt(int wx, int wy, int wz) {
    if (wy<0||wy>=CHUNK_SIZE_Y) return 0;
    int cx = (wx>=0) ? wx/CHUNK_SIZE_X : (wx-CHUNK_SIZE_X+1)/CHUNK_SIZE_X;
    int cz = (wz>=0) ? wz/CHUNK_SIZE_Z : (wz-CHUNK_SIZE_Z+1)/CHUNK_SIZE_Z;
    auto it = loadedChunks.find({cx,cz}); if (it==loadedChunks.end()) return 0;
    int lx = wx - cx*CHUNK_SIZE_X, lz = wz - cz*CHUNK_SIZE_Z;
    return it->second.getLocalBlock(lx,wy,lz);
}
void setBlockAt(int wx, int wy, int wz, int type) {
    if (wy<0||wy>=CHUNK_SIZE_Y) return;
    int cx = (wx>=0) ? wx/CHUNK_SIZE_X : (wx-CHUNK_SIZE_X+1)/CHUNK_SIZE_X;
    int cz = (wz>=0) ? wz/CHUNK_SIZE_Z : (wz-CHUNK_SIZE_Z+1)/CHUNK_SIZE_Z;
    auto it = loadedChunks.find({cx,cz}); if (it==loadedChunks.end()) return;
    int lx = wx - cx*CHUNK_SIZE_X, lz = wz - cz*CHUNK_SIZE_Z;
    it->second.setLocalBlock(lx,wy,lz,type);
    if (lx==0) { auto n=loadedChunks.find({cx-1,cz}); if(n!=loadedChunks.end()) n->second.meshReady=false; }
    if (lx==CHUNK_SIZE_X-1) { auto n=loadedChunks.find({cx+1,cz}); if(n!=loadedChunks.end()) n->second.meshReady=false; }
    if (lz==0) { auto n=loadedChunks.find({cx,cz-1}); if(n!=loadedChunks.end()) n->second.meshReady=false; }
    if (lz==CHUNK_SIZE_Z-1) { auto n=loadedChunks.find({cx,cz+1}); if(n!=loadedChunks.end()) n->second.meshReady=false; }
    waterChunksCacheValid = false;

    // Глобальное обновление света
    onBlockChangedGlobal(wx, wy, wz);
    rebuildChunkMeshesImmediatelyAround(cx, cz, lx, lz);
}

void integratePendingChunkData(int maxPerFrame) {
    std::vector<std::pair<glm::ivec2, std::shared_ptr<ChunkData>>> readyChunks;
    readyChunks.reserve(maxPerFrame);

    {
        std::lock_guard<std::mutex> lock(chunkMutex);
        auto it = pendingData.begin();
        while (it != pendingData.end() && (int)readyChunks.size() < maxPerFrame) {
            readyChunks.push_back(*it);
            it = pendingData.erase(it);
        }
    }

    for (auto& [chunkPos, chunkData] : readyChunks) {
        auto loadedIt = loadedChunks.find(chunkPos);
        if (loadedIt == loadedChunks.end()) continue;

        loadedIt->second.data = chunkData;
        loadedIt->second.meshReady = false;
        loadedIt->second.dirty = false;
        loadedIt->second.invalidateNeighbors();
    }
}

void updateChunksAroundCamera(const glm::vec3& cameraPos, bool loadFromFile) {
    int centerCX = (int)std::floor(cameraPos.x / CHUNK_SIZE_X);
    int centerCZ = (int)std::floor(cameraPos.z / CHUNK_SIZE_Z);
    static glm::ivec2 lastRequestedCenter(std::numeric_limits<int>::min(), std::numeric_limits<int>::min());

    const int integrateBudget = fastChunkLoadingMode ? 24 : 2;
    integratePendingChunkData(integrateBudget);

    if (!loadedChunks.empty() && lastRequestedCenter.x == centerCX && lastRequestedCenter.y == centerCZ) {
        return;
    }

    lastRequestedCenter = {centerCX, centerCZ};

    const int LOAD_RADIUS = 13;
    std::unordered_set<glm::ivec2, hash_ivec2> needed;
    for (int dx=-LOAD_RADIUS; dx<=LOAD_RADIUS; ++dx)
        for (int dz=-LOAD_RADIUS; dz<=LOAD_RADIUS; ++dz)
            needed.insert({centerCX+dx, centerCZ+dz});
    bool changed = false;
    for (auto it=loadedChunks.begin(); it!=loadedChunks.end(); ) {
        if (needed.find(it->first)==needed.end()) { it->second.saveAsync(); it=loadedChunks.erase(it); changed=true; }
        else ++it;
    }
    for (const auto& key : needed)
        if (loadedChunks.find(key)==loadedChunks.end()) {
            loadedChunks.emplace(std::piecewise_construct, std::forward_as_tuple(key.x,key.y), std::forward_as_tuple(key.x,key.y,loadFromFile));
            changed=true;
        }
        if (changed) {
            waterChunksCacheValid = false;
            lastChunkForMesh = nullptr;  // ДОБАВЬТЕ ЭТУ СТРОКУ
        }
}

void buildChunkMeshesNearCamera(int maxPerFrame) {
    struct Candidate {
        float dist2;
        Chunk* chunk;
    };

    std::vector<Candidate> candidates;
    candidates.reserve(loadedChunks.size());

    int built = 0;
    for (auto& [pos, chunk] : loadedChunks) {
        if (!chunk.data || chunk.meshReady) continue;

        float chunkCenterX = pos.x * CHUNK_SIZE_X + CHUNK_SIZE_X * 0.5f;
        float chunkCenterZ = pos.y * CHUNK_SIZE_Z + CHUNK_SIZE_Z * 0.5f;
        float dx = chunkCenterX - cameraPos.x;
        float dz = chunkCenterZ - cameraPos.z;
        candidates.push_back({dx * dx + dz * dz, &chunk});
    }

    std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
        return a.dist2 < b.dist2;
    });

    for (const Candidate& candidate : candidates) {
        if (built >= maxPerFrame) break;
        candidate.chunk->buildMesh();
        built++;
    }
}

void rebuildChunkMeshesImmediatelyAround(int cx, int cz, int lx, int lz) {
    glm::ivec2 rebuildList[5];
    int rebuildCount = 0;

    rebuildList[rebuildCount++] = {cx, cz};
    if (lx == 0) rebuildList[rebuildCount++] = {cx - 1, cz};
    if (lx == CHUNK_SIZE_X - 1) rebuildList[rebuildCount++] = {cx + 1, cz};
    if (lz == 0) rebuildList[rebuildCount++] = {cx, cz - 1};
    if (lz == CHUNK_SIZE_Z - 1) rebuildList[rebuildCount++] = {cx, cz + 1};

    for (int i = 0; i < rebuildCount; ++i) {
        auto it = loadedChunks.find(rebuildList[i]);
        if (it == loadedChunks.end()) continue;
        if (!it->second.data) continue;
        it->second.buildMesh();
    }
}

void updateWaterChunksCache() {
    if (waterChunksCacheValid) return;
    waterChunksCache.clear();
    for (auto& pair : loadedChunks) if (pair.second.vao[5]) waterChunksCache.push_back(&pair.second);
    waterChunksCacheValid = true;
}
void saveAllChunks() {
    saveCurrentWorldMetadata();
    for (auto& p : loadedChunks) {
        if (p.second.dirty) p.second.saveAsync();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
}

unsigned int loadTextureStrip(const char* path, bool forceAlpha) {
    unsigned int tex; glGenTextures(1,&tex); glBindTexture(GL_TEXTURE_2D,tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    int w,h,c; unsigned char* data = stbi_load(path, &w, &h, &c, forceAlpha?4:0);
    if (data) {
        GLenum fmt = (forceAlpha||c==4) ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_2D,0,fmt,w,h,0,fmt,GL_UNSIGNED_BYTE,data); glGenerateMipmap(GL_TEXTURE_2D);
    } else { unsigned char d[4]={255,255,255,255}; glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,d); }
    stbi_image_free(data); return tex;
}

bool loadBlockConfig(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return false;
    json data = json::parse(f);
    for (auto& item : data["blocks"]) {
        BlockType bt;
        bt.id = item["id"];
        bt.name = item["name"];
        std::string texPath = item["texture_strip"];
        bt.textureID = loadTextureStrip(texPath.c_str(), bt.id==5);
        bt.lightEmission = item.value("light", 0);
        bt.textureParts = item.value("texture_parts", 3);  // По умолчанию 3 части
        
        blockTypes[bt.id] = bt;
        isSolidBlockFast[bt.id] = (bt.id!=0 && bt.id!=5);
        blockLightEmission[bt.id] = bt.lightEmission;
        maxBlockLightRadius = std::max(maxBlockLightRadius, bt.lightEmission);
        blockOpacity[bt.id] = getLightOpacity(bt.id);

        if (item.contains("sounds")) {
            for (auto& [category, pattern] : item["sounds"].items()) {
                std::string patternStr = pattern;
                soundManager.loadSoundsForBlock(bt.id, "sounds/", patternStr);
            }
        }
    }
    return !blockTypes.empty();
}

bool rayCast(glm::vec3 origin, glm::vec3 dir, int& hx, int& hy, int& hz, int& face, float maxDist) {
    dir = glm::normalize(dir);
    glm::ivec3 pos((int)std::floor(origin.x+0.5f), (int)std::floor(origin.y+0.5f), (int)std::floor(origin.z+0.5f));
    glm::ivec3 step; step.x = (dir.x>0)?1:(dir.x<0)?-1:0; step.y = (dir.y>0)?1:(dir.y<0)?-1:0; step.z = (dir.z>0)?1:(dir.z<0)?-1:0;
    float tDeltaX = (dir.x!=0)?fabs(1.0f/dir.x):INFINITY, tDeltaY = (dir.y!=0)?fabs(1.0f/dir.y):INFINITY, tDeltaZ = (dir.z!=0)?fabs(1.0f/dir.z):INFINITY;
    float tMaxX, tMaxY, tMaxZ;
    if (dir.x>0) tMaxX = ((pos.x+0.5f)-origin.x)/dir.x; else if (dir.x<0) tMaxX = ((pos.x-0.5f)-origin.x)/dir.x; else tMaxX=INFINITY;
    if (dir.y>0) tMaxY = ((pos.y+0.5f)-origin.y)/dir.y; else if (dir.y<0) tMaxY = ((pos.y-0.5f)-origin.y)/dir.y; else tMaxY=INFINITY;
    if (dir.z>0) tMaxZ = ((pos.z+0.5f)-origin.z)/dir.z; else if (dir.z<0) tMaxZ = ((pos.z-0.5f)-origin.z)/dir.z; else tMaxZ=INFINITY;
    for (int i=0; i<200; ++i) {
        int block = getBlockAt(pos.x,pos.y,pos.z);
        if (block!=0 && block!=5) { hx=pos.x; hy=pos.y; hz=pos.z; return true; }
        if (tMaxX <= tMaxY && tMaxX <= tMaxZ) { if (tMaxX>maxDist) break; pos.x+=step.x; face=(dir.x>0)?1:0; tMaxX+=tDeltaX; }
        else if (tMaxY <= tMaxX && tMaxY <= tMaxZ) { if (tMaxY>maxDist) break; pos.y+=step.y; face=(dir.y>0)?3:2; tMaxY+=tDeltaY; }
        else { if (tMaxZ>maxDist) break; pos.z+=step.z; face=(dir.z>0)?5:4; tMaxZ+=tDeltaZ; }
    }
    return false;
}

// Функция расчета урона от падения (по формуле Minecraft)
float calculateFallDamage(float distance) {
    // В Minecraft: урон = расстояние_падения - 3
    // Каждые 1 сердце = 2 HP
    float damage = distance - MIN_FALL_DAMAGE_HEIGHT;
    return glm::clamp(damage, 0.0f, MAX_FALL_DAMAGE_HEIGHT - MIN_FALL_DAMAGE_HEIGHT);
}

bool isPlayerInWater() {
    glm::vec3 feetPos = cameraPos - glm::vec3(0.0f, EYE_HEIGHT, 0.0f);
    int blockAtFeet = getBlockAt(floor(feetPos.x), floor(feetPos.y + 0.1f), floor(feetPos.z));
    int blockAtWaist = getBlockAt(floor(cameraPos.x), floor(cameraPos.y - 0.5f), floor(cameraPos.z));
    int blockAtHead = getBlockAt(floor(cameraPos.x), floor(cameraPos.y), floor(cameraPos.z));
    
    return (blockAtFeet == 5 || blockAtWaist == 5 || blockAtHead == 5);
}

// Функция нанесения урона
void applyDamage(int damage) {
    if (damage <= 0 || invulnerabilityTimer > 0.0f) return;
    
    playerHealth -= damage;
    invulnerabilityTimer = INVULNERABILITY_DURATION;
    soundManager.playPlayerSound("hurt", true);
    // Эффект отбрасывания (красный экран) можно добавить позже
    std::cout << "Player took " << damage << " damage! Health: " << playerHealth << std::endl;
    
    // Проверка смерти
    if (playerHealth <= 0) {
        playerHealth = 0;
        // Здесь можно добавить логику смерти (респавн и т.д.)
        std::cout << "Player died!" << std::endl;
        
        // Временный респавн: восстанавливаем здоровье и телепортируем на спавн
        playerHealth = MAX_PLAYER_HEALTH;
        cameraPos = glm::vec3(0.0f, 200.0f, 0.0f);
        playerVelocity = glm::vec3(0.0f);
        fallDistance = 0.0f;
        placePlayerOnGround();
    }
}

void scanAmbientSounds() {
    std::string ambientDir = "sounds/ambient_sounds";
    if (!fs::exists(ambientDir)) {
        fs::create_directories(ambientDir);
        std::cout << "Created ambient sounds directory: " << ambientDir << std::endl;
        return;
    }
    
    for (const auto& entry : fs::directory_iterator(ambientDir)) {
        if (entry.is_regular_file()) {
            std::string ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext == ".wav" || ext == ".ogg" || ext == ".flac") { // SFML не поддерживает MP3 через SoundBuffer
                sf::SoundBuffer buffer;
                if (buffer.loadFromFile(entry.path().string())) {
                    ambientSoundBuffers.push_back(std::move(buffer));
                    std::cout << "Loaded ambient sound: " << entry.path().filename() << std::endl;
                } else {
                    std::cerr << "Failed to load ambient sound: " << entry.path() << std::endl;
                }
            }
        }
    }
    
    std::cout << "Total ambient sounds loaded: " << ambientSoundBuffers.size() << std::endl;
}

// ----------------------------------------------------------------------
// Состояния игры
// ----------------------------------------------------------------------
enum class GameState {
    MAIN_MENU,
    WORLD_SELECT_MENU,
    LOADING_GAME,
    LANGUAGE_MENU,
    IN_GAME,
    MAIN_MENU_OPTIONS,
    PAUSE_MENU,
    CREATIVE_INVENTORY
};


GameState currentState = GameState::MAIN_MENU;
bool gameStarted = false;          // для совместимости со звуком и некоторыми проверками
bool isLoadingGame = false;
bool movementEnabled = false;
bool playerPlaced = false;
float loadingTimer = 0.0f;
bool gamePaused = false;           // дублирует состояние, но оставлено для удобства

double mouseX=0, mouseY=0;
int currentScreenW=SCR_WIDTH, currentScreenH=SCR_HEIGHT;
float stepSoundTimer = 0.0f;
const float STEP_SOUND_INTERVAL = 0.4f;

// Forward declarations функций состояний
void updateMainMenu(GLFWwindow* window);
void renderMainMenu(int screenW, int screenH);
void handleMainMenuClick(GLFWwindow* window, int button);
void renderLanguageMenu(int screenW, int screenH);
void handleLanguageMenuClick(GLFWwindow* window, int button);
void updateWorldSelectMenu(GLFWwindow* window);
void renderWorldSelectMenu(int screenW, int screenH);
void handleWorldSelectMenuClick(GLFWwindow* window, int button);

void startNewGame();
void loadGame();
void exitToMenu(GLFWwindow* window, int& sw, int& sh);

void updateGame(GLFWwindow* window, float deltaTime);
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
    fastChunkLoadingMode = true;

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
    fastChunkLoadingMode = true;
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
    fastChunkLoadingMode = false;
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
        buildChunkMeshesNearCamera(24); // максимальная скорость во время загрузки мира
        if (areChunksReady()) {
            if (!playerPlaced) {
                placePlayerOnGround();
                playerPlaced = true;
            }
            movementEnabled = true;
            fastChunkLoadingMode = false;
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

    static bool pWasPressed = false;
    bool pPressed = glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS;
    if (pPressed && !pWasPressed) {
        isRaining = !isRaining;
    }
    pWasPressed = pPressed;

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

    // Сохраняем фактическое горизонтальное движение за кадр,
    // чтобы рендер модели корректно знал направление движения тела.
    if (deltaTime > 0.00001f) {
        playerVelocity.x = actualDelta.x / deltaTime;
        playerVelocity.z = actualDelta.z / deltaTime;
    } else {
        playerVelocity.x = 0.0f;
        playerVelocity.z = 0.0f;
    }

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
    glm::vec3 feetPos = cameraPos - glm::vec3(0.0f, EYE_HEIGHT, 0.0f);
    gameplayRayOrigin = cameraPos;
    gameplayRayDir = cameraFront;
    renderCameraPos = cameraPos;
    if (cameraMode == CameraMode::ThirdPersonBack) {
        // Центр третьего лица — голова/глаза игрока, как в Minecraft.
        glm::vec3 headCenter = feetPos + glm::vec3(0.0f, EYE_HEIGHT, 0.0f);

        glm::vec3 lookDir = cameraFront;
        if (glm::length(lookDir) < 0.001f) {
            lookDir = glm::vec3(0.0f, 0.0f, -1.0f);
        } else {
            lookDir = glm::normalize(lookDir);
        }

        renderCameraPos = headCenter - lookDir * thirdPersonDistance;
    }
}

void initPlayerRenderer() {
    playerTexHead = loadTextureStrip("textures/entity/player/head.png", true);
    playerTexBody = loadTextureStrip("textures/entity/player/body.png", true);
    playerTexArmL = loadTextureStrip("textures/entity/player/arm_left.png", true);
    playerTexArmR = loadTextureStrip("textures/entity/player/arm_right.png", true);
    playerTexLegL = loadTextureStrip("textures/entity/player/leg_left.png", true);
    playerTexLegR = loadTextureStrip("textures/entity/player/leg_right.png", true);
    auto setupPlayerTex = [](unsigned int tex) {
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        // Отключаем mipmap-уровни для скина, чтобы в движении не было "плавающих" пикселей.
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    };
    setupPlayerTex(playerTexHead);
    setupPlayerTex(playerTexBody);
    setupPlayerTex(playerTexArmL);
    setupPlayerTex(playerTexArmR);
    setupPlayerTex(playerTexLegL);
    setupPlayerTex(playerTexLegR);
    glGenVertexArrays(1, &playerVAO);
    glGenBuffers(1, &playerVBO);
}

void renderPlayerModel(const glm::vec3& feetPos, const glm::vec3& lookDir, float currentTime) {
    (void)currentTime;
    static float bodyYaw = 0.0f;

    (void)lookDir;
    // Используем только yaw камеры: это исключает любые влияния коллизий/скольжения/диагональных входов.
    // При yaw = -90° (вперёд по -Z) тело имеет нулевой разворот модели.
    bodyYaw = glm::radians(yaw + 270.0f);
    if (!playerVAO) return;

    int sampleX = static_cast<int>(std::floor(feetPos.x));
    int sampleZ = static_cast<int>(std::floor(feetPos.z));
    int sampleYFeet = static_cast<int>(std::floor(feetPos.y + 0.2f));
    int sampleYBody = static_cast<int>(std::floor(feetPos.y + 1.0f));
    int sampleYHead = static_cast<int>(std::floor(feetPos.y + 1.6f));
    uint8_t localBlockLight = std::max({
        getBlockLightAt(sampleX, sampleYFeet, sampleZ),
        getBlockLightAt(sampleX, sampleYBody, sampleZ),
        getBlockLightAt(sampleX, sampleYHead, sampleZ)
    });
    float playerBlockLight = static_cast<float>(localBlockLight) / static_cast<float>(MAX_LIGHT);

    auto pushFace = [&](std::vector<float>& verts, glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d, glm::vec3 n,
                       float u0, float v0, float u1, float v1) {
        float face[] = {
            a.x,a.y,a.z,u0,v1,n.x,n.y,n.z,1,playerBlockLight,   // Исправлено: v1 вместо v0
            b.x,b.y,b.z,u1,v1,n.x,n.y,n.z,1,playerBlockLight,
            c.x,c.y,c.z,u1,v0,n.x,n.y,n.z,1,playerBlockLight,
            c.x,c.y,c.z,u1,v0,n.x,n.y,n.z,1,playerBlockLight,
            d.x,d.y,d.z,u0,v0,n.x,n.y,n.z,1,playerBlockLight,
            a.x,a.y,a.z,u0,v1,n.x,n.y,n.z,1,playerBlockLight
        };
        verts.insert(verts.end(), std::begin(face), std::end(face));
    };

    struct UVRect { float u0, v0, u1, v1; };
    
    auto rotateAroundFeetY = [&](const glm::vec3& p) {
        float s = std::sin(bodyYaw);
        float c = std::cos(bodyYaw);
        glm::vec3 rel = p - feetPos;
        return feetPos + glm::vec3(rel.x * c - rel.z * s, rel.y, rel.x * s + rel.z * c);
    };

    auto rotateNormalY = [&](const glm::vec3& n) {
        float s = std::sin(bodyYaw);
        float c = std::cos(bodyYaw);
        return glm::vec3(n.x * c - n.z * s, n.y, n.x * s + n.z * c);
    };

    auto drawBox = [&](glm::vec3 center, glm::vec3 size, unsigned int tex, const std::array<UVRect, 6>& uv) {
        float x0 = center.x - size.x * 0.5f;
        float x1 = center.x + size.x * 0.5f;
        float y0 = center.y;
        float y1 = center.y + size.y;
        float z0 = center.z - size.z * 0.5f;
        float z1 = center.z + size.z * 0.5f;
        
        glm::vec3 p000 = rotateAroundFeetY({x0,y0,z0});
        glm::vec3 p001 = rotateAroundFeetY({x0,y0,z1});
        glm::vec3 p010 = rotateAroundFeetY({x0,y1,z0});
        glm::vec3 p011 = rotateAroundFeetY({x0,y1,z1});
        glm::vec3 p100 = rotateAroundFeetY({x1,y0,z0});
        glm::vec3 p101 = rotateAroundFeetY({x1,y0,z1});
        glm::vec3 p110 = rotateAroundFeetY({x1,y1,z0});
        glm::vec3 p111 = rotateAroundFeetY({x1,y1,z1});

        std::vector<float> v;
        v.reserve(36 * 10);
        
        // Левая грань (X-)
        pushFace(v, p000, p001, p011, p010, rotateNormalY({-1,0,0}), uv[0].u0, uv[0].v0, uv[0].u1, uv[0].v1);
        // Передняя грань (Z+)
        pushFace(v, p101, p001, p011, p111, rotateNormalY({0,0,1}), uv[1].u0, uv[1].v0, uv[1].u1, uv[1].v1);
        // Правая грань (X+)
        pushFace(v, p101, p100, p110, p111, rotateNormalY({1,0,0}), uv[2].u0, uv[2].v0, uv[2].u1, uv[2].v1);
        // Задняя грань (Z-)
        pushFace(v, p000, p100, p110, p010, rotateNormalY({0,0,-1}), uv[3].u0, uv[3].v0, uv[3].u1, uv[3].v1);
        // Верхняя грань (Y+)
        pushFace(v, p011, p111, p110, p010, rotateNormalY({0,1,0}), uv[4].u0, uv[4].v0, uv[4].u1, uv[4].v1);
        // Нижняя грань (Y-)
        pushFace(v, p101, p001, p000, p100, rotateNormalY({0,-1,0}), uv[5].u0, uv[5].v0, uv[5].u1, uv[5].v1);
        
        glBindTexture(GL_TEXTURE_2D, tex);
        glBindVertexArray(playerVAO);
        glBindBuffer(GL_ARRAY_BUFFER, playerVBO);
        glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(float), v.data(), GL_DYNAMIC_DRAW);
        
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 10 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 10 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 10 * sizeof(float), (void*)(5 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, 10 * sizeof(float), (void*)(8 * sizeof(float)));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, 10 * sizeof(float), (void*)(9 * sizeof(float)));
        glEnableVertexAttribArray(4);
        
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(v.size() / 10));
    };
    
    float y = feetPos.y;
    
    const std::array<UVRect, 6> headUv = {{
        {0.0f/48.0f, 0.0f/8.0f,  8.0f/48.0f, 1.0f},
        {8.0f/48.0f, 0.0f/8.0f, 16.0f/48.0f, 1.0f},
        {16.0f/48.0f,0.0f/8.0f, 24.0f/48.0f, 1.0f},
        {24.0f/48.0f,0.0f/8.0f, 32.0f/48.0f, 1.0f},
        {32.0f/48.0f,0.0f/8.0f, 40.0f/48.0f, 1.0f},
        {40.0f/48.0f,0.0f/8.0f, 48.0f/48.0f, 1.0f}
    }};

    const std::array<UVRect, 6> limbUv = {{
        {0.0f/24.0f, 0.0f/12.0f,  4.0f/24.0f, 1.0f},
        {4.0f/24.0f, 0.0f/12.0f,  8.0f/24.0f, 1.0f},
        {8.0f/24.0f, 0.0f/12.0f, 12.0f/24.0f, 1.0f},
        {12.0f/24.0f,0.0f/12.0f, 16.0f/24.0f, 1.0f},
        {16.0f/24.0f,8.0f/12.0f, 20.0f/24.0f, 1.0f},
        {20.0f/24.0f,8.0f/12.0f, 24.0f/24.0f, 1.0f}
    }};

    const std::array<UVRect, 6> bodyUv = {{
        {0.0f/40.0f, 0.0f/12.0f,  4.0f/40.0f, 1.0f},
        {4.0f/40.0f, 0.0f/12.0f, 12.0f/40.0f, 1.0f},
        {12.0f/40.0f,0.0f/12.0f, 16.0f/40.0f, 1.0f},
        {16.0f/40.0f,0.0f/12.0f, 24.0f/40.0f, 1.0f},
        {24.0f/40.0f,8.0f/12.0f, 32.0f/40.0f, 1.0f},
        {32.0f/40.0f,8.0f/12.0f, 40.0f/40.0f, 1.0f}
    }};
    
    drawBox(glm::vec3(feetPos.x, y + 0.75f, feetPos.z), glm::vec3(0.5f, 0.75f, 0.25f), playerTexBody, bodyUv);
    drawBox(glm::vec3(feetPos.x, y + 1.5f, feetPos.z), glm::vec3(0.5f, 0.5f, 0.5f), playerTexHead, headUv);
    drawBox(glm::vec3(feetPos.x - 0.375f, y + 0.75f, feetPos.z), glm::vec3(0.25f, 0.75f, 0.25f), playerTexArmL, limbUv);
    drawBox(glm::vec3(feetPos.x + 0.375f, y + 0.75f, feetPos.z), glm::vec3(0.25f, 0.75f, 0.25f), playerTexArmR, limbUv);
    drawBox(glm::vec3(feetPos.x - 0.125f, y + 0.0f, feetPos.z), glm::vec3(0.25f, 0.75f, 0.25f), playerTexLegL, limbUv);
    drawBox(glm::vec3(feetPos.x + 0.125f, y + 0.0f, feetPos.z), glm::vec3(0.25f, 0.75f, 0.25f), playerTexLegR, limbUv);
}

void renderGame(int screenW, int screenH, float currentTime) {
    glm::vec3 sunDir, skyColor;
    float sunIntensity, ambientBase;
    evaluateDayNightCycle(currentTime, sunDir, sunIntensity, ambientBase, skyColor);
    if (isRaining) {
        float dayBlend = glm::clamp((sunIntensity - 0.2f) / 0.8f, 0.0f, 1.0f);
        glm::vec3 rainyDaySky(0.40f, 0.42f, 0.45f);
        glm::vec3 rainyNightSky(0.06f, 0.07f, 0.10f);
        glm::vec3 rainyTarget = glm::mix(rainyNightSky, rainyDaySky, dayBlend);
        skyColor = glm::mix(skyColor, rainyTarget, 0.78f);
        sunIntensity *= 0.55f;
        ambientBase *= (0.60f + 0.20f * dayBlend);
    }

    glClearColor(skyColor.r, skyColor.g, skyColor.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glm::mat4 model(1.0f);
    updateGameplayCamera();
    glm::vec3 safeRenderCameraPos = renderCameraPos;
    glm::vec3 safeCameraFront = cameraFront;
    if (!std::isfinite(safeRenderCameraPos.x) || !std::isfinite(safeRenderCameraPos.y) || !std::isfinite(safeRenderCameraPos.z)) {
        safeRenderCameraPos = cameraPos;
    }
    if (!std::isfinite(safeCameraFront.x) || !std::isfinite(safeCameraFront.y) || !std::isfinite(safeCameraFront.z) || glm::length(safeCameraFront) < 0.001f) {
        safeCameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
    } else {
        safeCameraFront = glm::normalize(safeCameraFront);
    }
    glm::mat4 view = glm::lookAt(safeRenderCameraPos, safeRenderCameraPos + safeCameraFront, cameraUp);
    
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
    renderRainLayer(currentTime);

    // Рендер всех чанков
    for (auto& p : loadedChunks)
        p.second.render();

    // Рендер воды (с сортировкой)
    updateWaterChunksCache();
    if (glm::distance(safeRenderCameraPos, lastCameraPosForWaterSort) > 0.5f) {
        std::sort(waterChunksCache.begin(), waterChunksCache.end(), [&](Chunk* a, Chunk* b) {
            glm::vec3 ca(a->pos.x * CHUNK_SIZE_X + CHUNK_SIZE_X / 2, 30, a->pos.y * CHUNK_SIZE_Z + CHUNK_SIZE_Z / 2);
            glm::vec3 cb(b->pos.x * CHUNK_SIZE_X + CHUNK_SIZE_X / 2, 30, b->pos.y * CHUNK_SIZE_Z + CHUNK_SIZE_Z / 2);
            return glm::distance(safeRenderCameraPos, ca) > glm::distance(safeRenderCameraPos, cb);
        });
        lastCameraPosForWaterSort = safeRenderCameraPos;
    }
    for (Chunk* ch : waterChunksCache)
        ch->renderWater();

    // Дождь рисуем после геометрии мира, чтобы капли корректно были видны перед камерой.
    renderRainLayer(currentTime);

    // После рендера воды обязательно возвращаем обычный режим шейдера:
    // иначе UV-анимация воды (u_isWater=1) применяется и к модели игрока.
    glUniform1i(u_isWater_location, 0);

    if (cameraMode == CameraMode::ThirdPersonBack) {
        GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);
        if (cullWasEnabled) glDisable(GL_CULL_FACE);
        glm::vec3 feetPos = gameplayRayOrigin - glm::vec3(0.0f, EYE_HEIGHT, 0.0f);
        renderPlayerModel(feetPos, cameraFront, currentTime);
        if (cullWasEnabled) glEnable(GL_CULL_FACE);
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
    rainTexture = loadTextureStrip("textures/environment/rain.png", true);
    if (rainTexture) {
        glBindTexture(GL_TEXTURE_2D, rainTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        // Убираем mip-уровни, чтобы не тянуть чёрный фон с соседних texel при альфа-границах капель.
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    }

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

    glGenVertexArrays(1, &rainVAO);
    glGenBuffers(1, &rainVBO);
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

void renderRainLayer(float currentTime) {
    if (!isRaining || !rainTexture || !rainVAO || !rainVBO) return;

    const float radius = 16.0f;
    const float heightTop = 14.0f;
    const float heightBottom = -2.0f;

    std::vector<float> v;
    v.reserve(64 * 6 * 10);

    glm::vec3 camFlat(renderCameraPos.x, 0.0f, renderCameraPos.z);
    glm::vec3 right = glm::normalize(glm::vec3(cameraFront.z, 0.0f, -cameraFront.x));
    if (glm::length(right) < 0.001f) right = glm::vec3(1,0,0);

    int minX = (int)std::floor(camFlat.x - radius);
    int maxX = (int)std::ceil(camFlat.x + radius);
    int minZ = (int)std::floor(camFlat.z - radius);
    int maxZ = (int)std::ceil(camFlat.z + radius);

    float vScroll = std::fmod(currentTime * 1.8f, 1.0f);
    for (int x = minX; x <= maxX; x += 2) {
        for (int z = minZ; z <= maxZ; z += 2) {
            float fx = x + 0.5f;
            float fz = z + 0.5f;
            glm::vec3 center(fx, cameraPos.y, fz);
            glm::vec3 half = right * 0.20f;
            glm::vec3 a = center - half + glm::vec3(0,heightTop,0);
            glm::vec3 b = center + half + glm::vec3(0,heightTop,0);
            glm::vec3 c = center + half + glm::vec3(0,heightBottom,0);
            glm::vec3 d = center - half + glm::vec3(0,heightBottom,0);
            float quad[] = {
                a.x,a.y,a.z,0,0+vScroll,0,0,1,1,0.85f, b.x,b.y,b.z,1,0+vScroll,0,0,1,1,0.85f, c.x,c.y,c.z,1,1+vScroll,0,0,1,1,0.85f,
                c.x,c.y,c.z,1,1+vScroll,0,0,1,1,0.85f, d.x,d.y,d.z,0,1+vScroll,0,0,1,1,0.85f, a.x,a.y,a.z,0,0+vScroll,0,0,1,1,0.85f
            };
            v.insert(v.end(), std::begin(quad), std::end(quad));
        }
    }

    glBindVertexArray(rainVAO);
    glBindBuffer(GL_ARRAY_BUFFER, rainVBO);
    glBufferData(GL_ARRAY_BUFFER, v.size()*sizeof(float), v.data(), GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 10*sizeof(float), (void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 10*sizeof(float), (void*)(3*sizeof(float))); glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 10*sizeof(float), (void*)(5*sizeof(float))); glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, 10*sizeof(float), (void*)(8*sizeof(float))); glEnableVertexAttribArray(3);
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, 10*sizeof(float), (void*)(9*sizeof(float))); glEnableVertexAttribArray(4);

    glBindTexture(GL_TEXTURE_2D, rainTexture);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(v.size()/10));
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glBindVertexArray(0);
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
    if (rainTexture) glDeleteTextures(1, &rainTexture);
    if (rainVAO) glDeleteVertexArrays(1, &rainVAO);
    if (rainVBO) glDeleteBuffers(1, &rainVBO);
    glDeleteProgram(shaderProgram);
    glDeleteVertexArrays(1, &reticleVAO);
    glDeleteProgram(reticleProgram);
    glfwTerminate();
    return 0;
}
