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

// ----------------------------------------------------------------------
// Таблица твёрдости блоков и светимости
// ----------------------------------------------------------------------
bool isSolidBlockFast[256] = {false};
int blockLightEmission[256] = {0};
int blockOpacity[256] = {0};
int maxBlockLightRadius = 0;

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
    
    newFeetPos.y += delta.y;
    if (checkPlayerCollision(newFeetPos)) {
        if (delta.y > 0.0f) playerVelocity.y = 0.0f;
        else if (delta.y < 0.0f) playerVelocity.y = 0.0f;
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
    
    isOnGround = isOnGroundCheck(newFeetPos);
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
}

// ----------------------------------------------------------------------
// Типы блоков
// ----------------------------------------------------------------------
struct BlockType {
    int id;
    std::string name;
    unsigned int textureID;
    int lightEmission = 0;
};
std::unordered_map<int, BlockType> blockTypes;
int currentBlockType = 1;

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

private:
    std::unordered_map<int, std::unordered_map<std::string, SoundSet>> soundSets;
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
const std::string SAVE_DIR = "saves/world";
const std::string CHUNKS_DIR = SAVE_DIR + "/chunks";

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

void saveChunkToFile(const glm::ivec2& pos, const std::vector<int>& blocks,
                     const std::vector<uint8_t>& skyLight, const std::vector<uint8_t>& blockLight) {
    if (!fs::exists(CHUNKS_DIR)) fs::create_directories(CHUNKS_DIR);
    
    std::string filename = CHUNKS_DIR + "/chunk_" + std::to_string(pos.x) + "_" + std::to_string(pos.y) + ".bin";
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
    std::string filename = CHUNKS_DIR + "/chunk_" + std::to_string(cx) + "_" + std::to_string(cz) + ".bin";
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
    if (fs::exists(CHUNKS_DIR))
        for (const auto& entry : fs::directory_iterator(CHUNKS_DIR))
            if (entry.is_regular_file()) fs::remove(entry.path());
}

void initWorldNoise() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 999999);
    int seed = dis(gen);
    
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
const int MAX_LIGHT = 30;

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

        for (int i = 0; i < 6; ++i) {
            int nx = node.x + dirs[i][0];
            int ny = node.y + dirs[i][1];
            int nz = node.z + dirs[i][2];
            
            if (ny < 0 || ny >= CHUNK_SIZE_Y) continue;

            int blockId = getBlockAt(nx, ny, nz);
            if (isOpaque(blockId)) continue;

            uint8_t current = getBlockLightAt(nx, ny, nz);
            uint8_t newLight = computeSphericalBlockLight(nx, ny, nz, node.sourceX, node.sourceY, node.sourceZ, node.radius);

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
    setBlockLightAt(x, y, z, static_cast<uint8_t>(MAX_LIGHT));
    queue.push({x, y, z, x, y, z, light, static_cast<uint8_t>(MAX_LIGHT)});
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

        for (int i = 0; i < 6; ++i) {
            int nx = node.x + dirs[i][0];
            int ny = node.y + dirs[i][1];
            int nz = node.z + dirs[i][2];

            if (!isInsideLightRegion(region, nx, ny, nz)) continue;
            if (ny < 0 || ny >= CHUNK_SIZE_Y) continue;

            int blockId = getBlockAt(nx, ny, nz);
            if (isOpaque(blockId)) continue;

            uint8_t current = getBlockLightAt(nx, ny, nz);
            uint8_t newLight = computeSphericalBlockLight(nx, ny, nz, node.sourceX, node.sourceY, node.sourceZ, node.radius);

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
        if (MAX_LIGHT > current) {
            setBlockLightAt(x, y, z, static_cast<uint8_t>(MAX_LIGHT));
        }
    }

    queue.push({x, y, z, x, y, z, radius, static_cast<uint8_t>(MAX_LIGHT)});
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
            for (int y = surfaceY; y <= surfaceY+1; ++y) {
                if (y >= CHUNK_SIZE_Y) continue;
                if (blocks[(x * CHUNK_SIZE_Y + y) * CHUNK_SIZE_Z + z] == 6) return true;
            }
        }
    return false;
}

void addTree(int cx, int cz, int lx, int lz, int surfaceY, std::vector<int>& blocks) {
    int worldX = cx * CHUNK_SIZE_X + lx, worldZ = cz * CHUNK_SIZE_Z + lz;
    float treeRand = treeNoise.GetNoise((float)worldX, (float)worldZ);
    if (treeRand < 0.65f) return;
    if (isTreeNearby(lx, lz, surfaceY, blocks)) return;

    int trunkHeight = 4 + (static_cast<int>(treeRand * 100) % 2);
    auto setBlock = [&](int x, int y, int z, int type) {
        if (x < 0 || x >= CHUNK_SIZE_X || z < 0 || z >= CHUNK_SIZE_Z || y < 0 || y >= CHUNK_SIZE_Y) return;
        int idx = (x * CHUNK_SIZE_Y + y) * CHUNK_SIZE_Z + z;
        if (blocks[idx] == 0) blocks[idx] = type;
    };

    int topY = surfaceY + trunkHeight;
    if (topY + 2 >= CHUNK_SIZE_Y) return;

    for (int h = 1; h <= trunkHeight; ++h) {
        int y = surfaceY + h;
        int idx = (lx * CHUNK_SIZE_Y + y) * CHUNK_SIZE_Z + lz;
        if (blocks[idx] == 0) blocks[idx] = 6;
    }

    auto addLeafLayer = [&](int y, int radius, bool trimCorners) {
        if (y < 0 || y >= CHUNK_SIZE_Y) return;
        for (int dx = -radius; dx <= radius; ++dx) {
            for (int dz = -radius; dz <= radius; ++dz) {
                if (dx == 0 && dz == 0) continue;
                if (trimCorners && std::abs(dx) == radius && std::abs(dz) == radius) continue;
                setBlock(lx + dx, y, lz + dz, 7);
            }
        }
    };

    addLeafLayer(topY - 2, 2, false);
    addLeafLayer(topY - 1, 2, true);
    addLeafLayer(topY, 1, false);
    addLeafLayer(topY + 1, 1, true);
    setBlock(lx, topY + 1, lz, 7);
    setBlock(lx, topY + 2, lz, 7);
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
            if ((biome == 0 || biome == 1) && surfaceY > waterSurfaceY + 2)
                addTree(cx, cz, x, z, surfaceY, data->blocks);
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
            while (!pendingGen.empty() && processed < 20) {
                glm::ivec2 pos = *pendingGen.begin(); pendingGen.erase(pos);
                lock.unlock();
                auto data = generateChunk(pos.x, pos.y);
                lock.lock();
                pendingData[pos] = data;
                processed++;
            }
        }
        if (processed == 0) std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
}

// ----------------------------------------------------------------------
// Шейдерные переменные
// ----------------------------------------------------------------------
unsigned int shaderProgram, reticleProgram, reticleVAO;
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
unsigned int hotbarSlotTexture = 0, heartFullTexture = 0, heartHalfTexture = 0, hotbarSelTexture = 0, heartContTexture = 0;
unsigned int minecraftAsciiTexture = 0;
unsigned int languageButtonTexture = 0;
std::unordered_map<int, unsigned int> minecraftFontPages;
std::unordered_map<int, std::array<uint8_t, 256>> minecraftFontPageWidths;

unsigned int dimShaderProgram;
unsigned int dimVAO;

int currentHotbarSlot = 0;
constexpr int MAIN_MENU_BUTTON_COUNT = 6;
constexpr int WORLD_SELECT_BUTTON_COUNT = 6;
constexpr int LANGUAGE_BUTTON_COUNT = 1;
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
            return worldListState.selectedIndex >= 0; // доступна, только если выбран мир
        case 1: // "Rename"
            return worldListState.selectedIndex >= 0; // доступна, только если выбран мир
        case 2: // "Delete"
            return worldListState.selectedIndex >= 0; // доступна, только если выбран мир
        case 3: // "Create New World"
            return true; // всегда доступна
        case 4: // "Re-Create"
            return worldListState.selectedIndex >= 0; // доступна, только если выбран мир
        case 5: // "Cancel"
            return true; // всегда доступна
        default:
            return true;
    }
}

void refreshWorldMenuEntries() {
    worldMenuEntries.clear();
    for (int i = 1; i <= 15; ++i) {
        MenuListEntry entry;
        entry.title = "Test World " + std::to_string(i);
        entry.subtitle = "Folder: test_world_" + std::to_string(i) + " (2024-01-01 12:00)";
        entry.detail = "Game Mode: Survival";
        worldMenuEntries.push_back(entry);
    }
    
    // Если есть реальный сохраненный мир, добавляем его в начало
    if (fs::exists(SAVE_DIR)) {
        MenuListEntry realEntry;
        realEntry.title = tr("My World", "Мой мир", "マイワールド");
        std::string folderName = fs::path(SAVE_DIR).filename().string();
        std::string timeText = formatFileTime(fs::last_write_time(SAVE_DIR));
        realEntry.subtitle = folderName + " (" + timeText + ")";
        realEntry.detail = tr("Game Mode: Survival", "Режим игры: Выживание", "ゲームモード: サバイバル");
        worldMenuEntries.insert(worldMenuEntries.begin(), realEntry);
    }
    
    // ИЗМЕНЕНИЕ: Сбрасываем selectedIndex на -1 (ничего не выбрано)
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
    if (!fs::exists(CHUNKS_DIR) || !fs::is_directory(CHUNKS_DIR)) {
        return false;
    }

    for (const auto& entry : fs::directory_iterator(CHUNKS_DIR)) {
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
    hotbarSlotTexture = loadUITexture("textures/hotbar_slot.png");
    heartFullTexture  = loadUITexture("textures/heart_full.png");
    heartHalfTexture  = loadUITexture("textures/heart_half.png");
    hotbarSelTexture  = loadUITexture("textures/hotbar_sel.png");
    heartContTexture  = loadUITexture("textures/heart_cont.png");
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

void drawHUD(int screenW, int screenH) {
    if (!hotbarSlotTexture || !heartFullTexture || !heartHalfTexture || !hotbarSelTexture || !heartContTexture) return;

    GLboolean depthTest = glIsEnabled(GL_DEPTH_TEST);
    GLboolean blend = glIsEnabled(GL_BLEND);
    GLboolean cull = glIsEnabled(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE); glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const int SLOT_SIZE = 60, SLOT_SPACING = -4, HOTBAR_SLOTS = 9;
    int totalWidth = HOTBAR_SLOTS * SLOT_SIZE + (HOTBAR_SLOTS - 1) * SLOT_SPACING;
    int startX = (screenW - totalWidth) / 2, startY = screenH - SLOT_SIZE;

    for (int i = 0; i < HOTBAR_SLOTS; ++i)
        drawRectangle(startX + i * (SLOT_SIZE + SLOT_SPACING), startY, SLOT_SIZE, SLOT_SIZE, hotbarSlotTexture, screenW, screenH);

    {
        int selX = startX + currentHotbarSlot * (SLOT_SIZE + SLOT_SPACING);
        int selW = SLOT_SIZE + 5, selH = SLOT_SIZE + 5, offsetX = -3, offsetY = -2;
        drawRectangle(selX + offsetX, startY + offsetY, selW, selH, hotbarSelTexture, screenW, screenH);
    }

    const int HEART_SIZE = 25, HEART_SPACING = -3;
    int heartsTotal = MAX_PLAYER_HEALTH / 2;
    int heartsX = startX, heartsY = startY - HEART_SIZE - 5;

    for (int i = 0; i < heartsTotal; ++i) {
        int healthRemaining = playerHealth - i * 2;
        unsigned int tex = 0;
        if (healthRemaining >= 2) tex = heartFullTexture;
        else if (healthRemaining == 1) tex = heartHalfTexture;
        else break;
        int x = heartsX + i * (HEART_SIZE + HEART_SPACING);
        drawRectangle(x, heartsY, HEART_SIZE, HEART_SIZE, heartContTexture, screenW, screenH);
        drawRectangle(x, heartsY, HEART_SIZE, HEART_SIZE, tex, screenW, screenH);
    }

    if (depthTest) glEnable(GL_DEPTH_TEST);
    if (!blend) glDisable(GL_BLEND);
    if (cull) glEnable(GL_CULL_FACE);
}

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
            std::string filename = CHUNKS_DIR + "/chunk_" + std::to_string(cx) + "_" + std::to_string(cz) + ".bin";
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
            side1 = isAOSolid(wx + sx, ly + 1, wz);
            side2 = isAOSolid(wx, ly + 1, wz + sz);
            corner = isAOSolid(wx + sx, ly + 1, wz + sz);
        } else if (normal.y < -0.5f) {
            side1 = isAOSolid(wx + sx, ly - 1, wz);
            side2 = isAOSolid(wx, ly - 1, wz + sz);
            corner = isAOSolid(wx + sx, ly - 1, wz + sz);
        } else if (normal.x > 0.5f) {
            side1 = isAOSolid(wx + 1, ly + sy, wz);
            side2 = isAOSolid(wx + 1, ly, wz + sz);
            corner = isAOSolid(wx + 1, ly + sy, wz + sz);
        } else if (normal.x < -0.5f) {
            side1 = isAOSolid(wx - 1, ly + sy, wz);
            side2 = isAOSolid(wx - 1, ly, wz + sz);
            corner = isAOSolid(wx - 1, ly + sy, wz + sz);
        } else if (normal.z > 0.5f) {
            side1 = isAOSolid(wx + sx, ly, wz + 1);
            side2 = isAOSolid(wx, ly + sy, wz + 1);
            corner = isAOSolid(wx + sx, ly + sy, wz + 1);
        } else if (normal.z < -0.5f) {
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
            float boostedBlockLight = glm::min(static_cast<float>(MAX_LIGHT), blockLight * 1.5f);
            return (skyLight + boostedBlockLight) / 30.0f;
        };

        if (normal.y > 0.5f) {
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
            int sampleY = ly - 1;
            if (sampleY < 0 || sampleY >= CHUNK_SIZE_Y) return 0.0f;
            uint8_t sl = getSkyLightAt(wx, sampleY, wz);
            uint8_t bl = getBlockLightAt(wx, sampleY, wz);
            return boostedLight(sl, bl) * ao;
        }

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
            auto addFace = [&](const float* face, float uOff, const glm::vec3& n, int faceIdx, std::vector<float>& out) {
                for (int i=0; i<18; i+=3) {
                    out.push_back(face[i]+ox); out.push_back(face[i+1]+oy); out.push_back(face[i+2]+oz);
                    int uvIdx = (i/3)*2; float u = baseUV[uvIdx], v = baseUV[uvIdx+1];
                    if (uOff == 1.0f/3.0f) v = 1.0f - v;
                    out.push_back(uOff + u*(1.0f/3.0f)); out.push_back(v);
                    out.push_back(n.x); out.push_back(n.y); out.push_back(n.z);
                    glm::vec3 vertexOffset(face[i], face[i+1], face[i+2]);
                    float light = getVertexLight(x, y, z, n, vertexOffset, faceNeighborOffsets[faceIdx]);
                    out.push_back(light);
                }
            };
            std::vector<float>& verts = verticesPerType[type];
            int neighbor;
            if (type==5) {
                neighbor=getBlockAtForMesh(ox-1,oy,oz); if(neighbor!=5&&neighbor!=BLOCK_UNKNOWN) addFace(leftFace,1.0f/3.0f,glm::vec3(-1,0,0),0,verts);
                neighbor=getBlockAtForMesh(ox+1,oy,oz); if(neighbor!=5&&neighbor!=BLOCK_UNKNOWN) addFace(rightFace,1.0f/3.0f,glm::vec3(1,0,0),1,verts);
                neighbor=getBlockAtForMesh(ox,oy,oz+1); if(neighbor!=5&&neighbor!=BLOCK_UNKNOWN) addFace(frontFace,1.0f/3.0f,glm::vec3(0,0,1),4,verts);
                neighbor=getBlockAtForMesh(ox,oy,oz-1); if(neighbor!=5&&neighbor!=BLOCK_UNKNOWN) addFace(backFace,1.0f/3.0f,glm::vec3(0,0,-1),5,verts);
                neighbor=getBlockAtForMesh(ox,oy+1,oz); if(neighbor!=5&&neighbor!=BLOCK_UNKNOWN) addFace(topFace,0.0f,glm::vec3(0,1,0),3,verts);
                neighbor=getBlockAtForMesh(ox,oy-1,oz); if(neighbor!=5&&neighbor!=BLOCK_UNKNOWN) addFace(bottomFace,2.0f/3.0f,glm::vec3(0,-1,0),2,verts);
            } else {
                neighbor=getBlockAtForMesh(ox-1,oy,oz); if(neighbor==0||neighbor==5) addFace(leftFace,1.0f/3.0f,glm::vec3(-1,0,0),0,verts);
                neighbor=getBlockAtForMesh(ox+1,oy,oz); if(neighbor==0||neighbor==5) addFace(rightFace,1.0f/3.0f,glm::vec3(1,0,0),1,verts);
                neighbor=getBlockAtForMesh(ox,oy,oz+1); if(neighbor==0||neighbor==5) addFace(frontFace,1.0f/3.0f,glm::vec3(0,0,1),4,verts);
                neighbor=getBlockAtForMesh(ox,oy,oz-1); if(neighbor==0||neighbor==5) addFace(backFace,1.0f/3.0f,glm::vec3(0,0,-1),5,verts);
                neighbor=getBlockAtForMesh(ox,oy+1,oz); if(neighbor==0||neighbor==5) addFace(topFace,0.0f,glm::vec3(0,1,0),3,verts);
                neighbor=getBlockAtForMesh(ox,oy-1,oz); if(neighbor==0||neighbor==5) addFace(bottomFace,2.0f/3.0f,glm::vec3(0,-1,0),2,verts);
            }
        }
        for (auto& [type, verts] : verticesPerType) {
            if (verts.empty()) continue;
            glGenVertexArrays(1, &vao[type]); glGenBuffers(1, &vbo[type]);
            glBindVertexArray(vao[type]); glBindBuffer(GL_ARRAY_BUFFER, vbo[type]);
            glBufferData(GL_ARRAY_BUFFER, verts.size()*sizeof(float), verts.data(), GL_STATIC_DRAW);
            glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,9*sizeof(float),(void*)0); glEnableVertexAttribArray(0);
            glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,9*sizeof(float),(void*)(3*sizeof(float))); glEnableVertexAttribArray(1);
            glVertexAttribPointer(2,3,GL_FLOAT,GL_FALSE,9*sizeof(float),(void*)(5*sizeof(float))); glEnableVertexAttribArray(2);
            glVertexAttribPointer(3,1,GL_FLOAT,GL_FALSE,9*sizeof(float),(void*)(8*sizeof(float))); glEnableVertexAttribArray(3);
            vertexCount[type] = verts.size() / 9;
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
        std::thread([posCopy, blocksCopy, skyCopy, blockCopy]() {
            saveChunkToFile(posCopy, blocksCopy, skyCopy, blockCopy);
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

    integratePendingChunkData(2);

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
    if (changed) waterChunksCacheValid = false;
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
void saveAllChunks() { for (auto& p : loadedChunks) if (p.second.dirty) p.second.saveAsync(); std::this_thread::sleep_for(std::chrono::milliseconds(2000)); }

unsigned int loadTextureStrip(const char* path, bool forceAlpha = false) {
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

// ----------------------------------------------------------------------
// Состояния игры
// ----------------------------------------------------------------------
enum class GameState {
    MAIN_MENU,
    WORLD_SELECT_MENU,
    LOADING_GAME,
    LANGUAGE_MENU,
    IN_GAME,
    PAUSE_MENU
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

// ----------------------------------------------------------------------
// Реализация функций состояний
// ----------------------------------------------------------------------

void startNewGame() {
    gameStarted = true;
    movementEnabled = false;
    playerPlaced = false;
    loadingTimer = 0.0f;
    isLoadingGame = false;
    deleteOldWorld();
    initWorldNoise();
    workerThread = std::thread(workerFunction);
    updateChunksAroundCamera(cameraPos, false);
    initReticle();
    startMusic();
    glfwSetInputMode(glfwGetCurrentContext(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    currentState = GameState::LOADING_GAME;
}

void loadGame() {
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
    currentState = GameState::MAIN_MENU;
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
    drawMinecraftTextCentered(tr("Select World", "Выбор мира", "ワールド選択"), screenW * 0.5f, 60.0f, 3.0f, screenW, screenH, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    renderMenuList(worldListState, worldMenuEntries, screenW, screenH);

    // 3. ЗАТЕМ рисуем светлые полосы сверху и снизу (поверх списка)
    if (lightTexture) {
        drawTiledBackground(lightTexture, screenW, screenH, 0.0f, 0.0f, static_cast<float>(screenW), topStripHeight);
        drawTiledBackground(lightTexture, screenW, screenH, 0.0f, screenH - bottomStripHeight, static_cast<float>(screenW), bottomStripHeight);
    }

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
        if (appliedIndex >= 0) applyLanguageSelection(appliedIndex);
        return;
    }
    if (mx >= languageButtons[0].absX && mx <= languageButtons[0].absX + languageButtons[0].absW &&
        my >= languageButtons[0].absY && my <= languageButtons[0].absY + languageButtons[0].absH) {
        if (languageListState.selectedIndex >= 0) applyLanguageSelection(languageListState.selectedIndex);
        currentState = GameState::MAIN_MENU;
    }
}

void handleWorldSelectMenuClick(GLFWwindow* window, int button) {
    if (button != GLFW_MOUSE_BUTTON_LEFT) return;

    double mx, my;
    glfwGetCursorPos(window, &mx, &my);
    int appliedIndex = -1;
    if (handleMenuListClick(worldListState, worldMenuEntries, mx, my, glfwGetTime(), appliedIndex)) {
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
        if (i == 0) {
            if (worldListState.selectedIndex >= 0 && worldListState.selectedIndex < static_cast<int>(worldMenuEntries.size())) {
                if (hasSavedWorld() && worldListState.selectedIndex == 0) {
                    loadGame();
                } else {
                    startNewGame();
                }
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
                    // Пока ничего
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
    } else if (currentState == GameState::IN_GAME) {
        updateChunksAroundCamera(cameraPos, isLoadingGame);
        buildChunkMeshesNearCamera(2);
        processInputInGame(window, deltaTime);
        updateMusic();
    }
}

void processInputInGame(GLFWwindow* window, float deltaTime) {
    static bool escWasPressed = false;
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        if (!escWasPressed) {
            gamePaused = true;
            currentState = GameState::PAUSE_MENU;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
        escWasPressed = true;
        return;
    } else {
        escWasPressed = false;
    }

    if (!movementEnabled) return;

    float moveSpeed = WALK_SPEED;
    glm::vec3 moveDir(0.0f);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) moveDir += cameraFront;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) moveDir -= cameraFront;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) moveDir -= glm::normalize(glm::cross(cameraFront, cameraUp));
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) moveDir += glm::normalize(glm::cross(cameraFront, cameraUp));
    
    bool moving = glm::length(moveDir) > 0.1f;
    if (moving) moveDir = glm::normalize(moveDir);
    glm::vec3 desiredMove = moveDir * moveSpeed * deltaTime;
    
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS && isOnGround) {
        playerVelocity.y = JUMP_POWER;
        isOnGround = false;
    }
    
    playerVelocity.y += GRAVITY * deltaTime;
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

    if (isOnGround && moving) {
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

    for (int i = 0; i < 8; ++i)
        if (glfwGetKey(window, GLFW_KEY_1 + i) == GLFW_PRESS)
            currentHotbarSlot = i;
    if (glfwGetKey(window, GLFW_KEY_9) == GLFW_PRESS)
        currentBlockType = 9;
    for (int i = 1; i <= 9; ++i)
        if (glfwGetKey(window, GLFW_KEY_0 + i) == GLFW_PRESS && blockTypes.count(i))
            currentBlockType = i;
}

void renderGame(int screenW, int screenH, float currentTime) {
    glm::vec3 sunDir, skyColor;
    float sunIntensity, ambientBase;
    evaluateDayNightCycle(currentTime, sunDir, sunIntensity, ambientBase, skyColor);

    glClearColor(skyColor.r, skyColor.g, skyColor.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glm::mat4 model(1.0f);
    glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
    glm::mat4 proj = glm::perspective(glm::radians(65.0f), (float)screenW / screenH, 0.1f, 1000.0f);

    glUseProgram(shaderProgram);
    glUniformMatrix4fv(u_modelLoc, 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(u_viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(u_projLoc, 1, GL_FALSE, glm::value_ptr(proj));
    glUniform1f(u_time_location, currentTime);
    glUniform3fv(u_sunDir_location, 1, glm::value_ptr(sunDir));
    glUniform1f(u_sunIntensity_location, sunIntensity);
    glUniform1f(u_ambientBase_location, ambientBase);

    for (auto& p : loadedChunks)
        p.second.render();

    updateWaterChunksCache();
    if (glm::distance(cameraPos, lastCameraPosForWaterSort) > 0.5f) {
        std::sort(waterChunksCache.begin(), waterChunksCache.end(), [&](Chunk* a, Chunk* b) {
            glm::vec3 ca(a->pos.x * CHUNK_SIZE_X + CHUNK_SIZE_X / 2, 30, a->pos.y * CHUNK_SIZE_Z + CHUNK_SIZE_Z / 2);
            glm::vec3 cb(b->pos.x * CHUNK_SIZE_X + CHUNK_SIZE_X / 2, 30, b->pos.y * CHUNK_SIZE_Z + CHUNK_SIZE_Z / 2);
            return glm::distance(cameraPos, ca) > glm::distance(cameraPos, cb);
        });
        lastCameraPosForWaterSort = cameraPos;
    }
    for (Chunk* ch : waterChunksCache)
        ch->renderWater();

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    drawHUD(screenW, screenH);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);

    glUseProgram(reticleProgram);
    glBindVertexArray(reticleVAO);
    glDrawArrays(GL_POINTS, 0, 1);
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
            gamePaused = false;
            currentState = GameState::IN_GAME;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            glfwGetCursorPos(window, &lastX, &lastY);
            firstMouse = true;
        } else if (mx >= pauseExitButton.absX && mx <= pauseExitButton.absX + pauseExitButton.absW &&
                   my >= pauseExitButton.absY && my <= pauseExitButton.absY + pauseExitButton.absH) {
            exitToMenu(window, currentScreenW, currentScreenH);
        }
    }
}

// ----------------------------------------------------------------------
// Коллбэки GLFW
// ----------------------------------------------------------------------
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (action != GLFW_PRESS) return;

    if (currentState == GameState::MAIN_MENU) {
        handleMainMenuClick(window, button);
    } else if (currentState == GameState::WORLD_SELECT_MENU) {
        handleWorldSelectMenuClick(window, button);
    } else if (currentState == GameState::PAUSE_MENU) {
        handlePauseMenuClick(window, button);
    } else if (currentState == GameState::LANGUAGE_MENU) {
        handleLanguageMenuClick(window, button);
    } else if (currentState == GameState::IN_GAME) {
        if (!movementEnabled) return;
        glm::vec3 rayDir = cameraFront;
        int hx, hy, hz, face;
        if (rayCast(cameraPos, rayDir, hx, hy, hz, face, 10.0f)) {
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
out vec2 TexCoord; out vec3 FragPos; out vec3 Normal; out float LightLevel;
uniform mat4 model; uniform mat4 view; uniform mat4 projection;
void main() {
    vec4 worldPos = model * vec4(aPos, 1.0);
    gl_Position = projection * view * worldPos;
    TexCoord = aTexCoord; FragPos = worldPos.xyz;
    Normal = mat3(transpose(inverse(model))) * aNormal;
    LightLevel = aLight;
}
)";
const char *fragmentShaderSource = R"(
#version 330 core
in vec2 TexCoord; out vec4 FragColor;
uniform sampler2D ourTexture; uniform float u_time; uniform int u_isWater;
uniform vec3 u_sunDir; uniform float u_sunIntensity; uniform float u_ambientBase;
in vec3 FragPos; in vec3 Normal; in float LightLevel;
void main() {
    vec2 uv = TexCoord;
    if (u_isWater == 1) {
        float frames = 32.0, speed = 0.7;
        float frame = fract(u_time * speed) * frames;
        uv.y = uv.y / frames + floor(frame) / frames;
    }
    vec4 color = texture(ourTexture, uv); if (u_isWater==1) color.a = 0.7;
    vec3 n = normalize(Normal); vec3 lightDir = normalize(-u_sunDir);
    float diffuse = max(dot(n, lightDir), 0.0) * u_sunIntensity;
    float vertexLight = clamp(LightLevel, 0.0, 1.0);
    float shade = mix(0.22, 1.0, pow(vertexLight, 0.85));
    float ambientFactor = mix(0.10, 1.0, pow(vertexLight, 1.15));
    float lighting = u_ambientBase * ambientFactor + diffuse * shade;
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
    initUI(); loadMenuTextures(); loadHUDTextures(); initLanguageMenu();

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
                updateGame(window, deltaTime);
                renderGame(screenW, screenH, now);
                if (currentState == GameState::PAUSE_MENU)
                    break;
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
    glDeleteTextures(1, &menuBackgroundTexture);
    glDeleteTextures(1, &menuBackgroundLightTexture);
    glDeleteTextures(1, &menuBackgroundDarkTexture);
    glDeleteTextures(1, &menuButtonTexture);
    glDeleteTextures(1, &menuButtonHighlightTexture);
    glDeleteTextures(1, &menuPhotoTexture);
    glDeleteTextures(1, &menuButtonDisabledTexture);
    glDeleteTextures(1, &hotbarSlotTexture);
    glDeleteTextures(1, &heartFullTexture);
    glDeleteTextures(1, &heartHalfTexture);
    glDeleteTextures(1, &hotbarSelTexture);
    glDeleteTextures(1, &heartContTexture);
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
    glDeleteProgram(shaderProgram);
    glDeleteVertexArrays(1, &reticleVAO);
    glDeleteProgram(reticleProgram);
    glfwTerminate();
    return 0;
}