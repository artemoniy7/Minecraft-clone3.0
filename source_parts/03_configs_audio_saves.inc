// Типы блоков/предметов, звук, сохранения мира и инициализация шума.
// Этот файл подключается из main2.cpp и рассчитан на сборку в одном translation unit.

struct BlockType {
    int id;
    std::string name;
    unsigned int textureID;
    int lightEmission = 0;
    bool transparent = false;
    int textureParts = 3;  // Количество частей в текстурной полосе (3, 4 или 6)
};
std::unordered_map<int, BlockType> blockTypes;

// Alpha-блоки, заданные прямо в коде, имеют приоритет над blocks.json.
// Такие блоки пропускают свет и могут рисовать грани рядом с таким же блоком.
// Грани рядом с непрозрачными блоками скрываются, чтобы листва не мерцала внутри стволов/земли.
// Если id не указан здесь, transparent берётся из blocks.json и ведёт себя как стекло:
// свет проходит, но внутренние грани между одинаковыми alpha-блоками скрываются.
const std::unordered_set<int> codeAlphaBlocks = {
    BLOCK_OAK_LEAVES // oak_leaves
};

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
    jumpKeyWasPressed = false;
    heldJumpRepeatTimer = 0.0f;
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
        jumpKeyWasPressed = false;
        heldJumpRepeatTimer = 0.0f;
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

    mountainNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    mountainNoise.SetFrequency(0.0035f);
    mountainNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    mountainNoise.SetFractalOctaves(3);
    mountainNoise.SetFractalGain(0.42f);
    mountainNoise.SetFractalLacunarity(2.0f);
    mountainNoise.SetSeed(seed + 1);

    riverNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    riverNoise.SetFrequency(0.0075f);
    riverNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    riverNoise.SetFractalOctaves(2);
    riverNoise.SetSeed(seed + 2);

    biomeNoiseA.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    biomeNoiseA.SetFrequency(0.0048f);
    biomeNoiseA.SetFractalType(FastNoiseLite::FractalType_FBm);
    biomeNoiseA.SetFractalOctaves(2);
    biomeNoiseA.SetSeed(seed + 3);

    biomeNoiseB.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    biomeNoiseB.SetFrequency(0.0048f);
    biomeNoiseB.SetFractalType(FastNoiseLite::FractalType_FBm);
    biomeNoiseB.SetFractalOctaves(2);
    biomeNoiseB.SetSeed(seed + 4);

    detailNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    detailNoise.SetFrequency(0.018f);
    detailNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    detailNoise.SetFractalOctaves(2);
    detailNoise.SetSeed(seed + 5);

    treeNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    treeNoise.SetFrequency(0.12f);
    treeNoise.SetSeed(seed + 6);

    seaLevelNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    seaLevelNoise.SetFrequency(0.0022f);
    seaLevelNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    seaLevelNoise.SetFractalOctaves(2);
    seaLevelNoise.SetFractalLacunarity(2.0f);
    seaLevelNoise.SetFractalGain(0.5f);
    seaLevelNoise.SetSeed(seed + 7);

    transitionNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    transitionNoise.SetFrequency(0.0065f);
    transitionNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    transitionNoise.SetFractalOctaves(2);
    transitionNoise.SetSeed(seed + 8);
}
