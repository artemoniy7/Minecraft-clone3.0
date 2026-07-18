// Свет, генерация чанков, фоновые workers и меню языка ранней загрузки.
// Этот файл подключается из main2.cpp и рассчитан на сборку в одном translation unit.

inline bool isAlphaBlock(int blockId) {
    return blockId >= 0 && blockId < 256 && blockHasAlpha[blockId];
}

// Проверка, является ли блок НЕПРОЗРАЧНЫМ (полностью блокирует свет).
// Блоки с alpha-текстурой (стекло, листва и т.п.) пропускают свет как в Minecraft.
bool isOpaque(int blockId) {
    if (blockId == 0) return false;  // воздух
    if (blockId == 5) return false;  // вода
    if (isAlphaBlock(blockId)) return false;
    return true;
}

// Ослабление света при прохождении через блок.
// Alpha-блоки не являются воздухом, но свет через них проходит с минимальной потерей.
int getLightOpacity(int blockId) {
    if (blockId == 0) return 0;
    if (blockId == 5) return 1;
    if (isAlphaBlock(blockId)) return 1;
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
void updateBlockLightIncremental(int x, int y, int z, int oldEmission, int newEmission);
void rebuildSkyLightRegion(const LightRegion& rawRegion);
void scheduleChunkMeshRebuildRegion(const LightRegion& rawRegion);

// Обновление небесного света при изменении блока
void updateSkyLightAt(int x, int y, int z) {
    LightRegion region;
    region.minX = x - MAX_LIGHT;
    region.maxX = x + MAX_LIGHT;
    region.minY = 0;
    region.maxY = CHUNK_SIZE_Y - 1;
    region.minZ = z - MAX_LIGHT;
    region.maxZ = z + MAX_LIGHT;
    rebuildSkyLightRegion(region);
}

// Обработчик изменения блока - ТЕПЕРЬ ОБНОВЛЯЕТ И НЕБЕСНЫЙ СВЕТ ТОЖЕ
void onBlockChangedGlobal(int x, int y, int z, bool updateSkyLight, bool rebuildBlockLight) {
    if (updateSkyLight) {
        updateSkyLightAt(x, y, z);
    }

    LightRegion region;
    region.minX = x - maxBlockLightRadius;
    region.maxX = x + maxBlockLightRadius;
    region.minY = std::max(0, y - maxBlockLightRadius);
    region.maxY = std::min(CHUNK_SIZE_Y - 1, y + maxBlockLightRadius);
    region.minZ = z - maxBlockLightRadius;
    region.maxZ = z + maxBlockLightRadius;
    if (rebuildBlockLight) {
        rebuildBlockLightRegion(region);
    }

    LightRegion meshRegion;
    const bool includeBlockLightRadius = rebuildBlockLight;
    meshRegion.minX = updateSkyLight ? std::min(x - MAX_LIGHT, region.minX) : (includeBlockLightRadius ? region.minX : x);
    meshRegion.maxX = updateSkyLight ? std::max(x + MAX_LIGHT, region.maxX) : (includeBlockLightRadius ? region.maxX : x);
    meshRegion.minY = 0;
    meshRegion.maxY = CHUNK_SIZE_Y - 1;
    meshRegion.minZ = updateSkyLight ? std::min(z - MAX_LIGHT, region.minZ) : (includeBlockLightRadius ? region.minZ : z);
    meshRegion.maxZ = updateSkyLight ? std::max(z + MAX_LIGHT, region.maxZ) : (includeBlockLightRadius ? region.maxZ : z);
    scheduleChunkMeshRebuildRegion(meshRegion);
}

void rebuildBlockLightAroundChunk(int cx, int cz) {
    LightRegion region;
    region.minX = cx * CHUNK_SIZE_X - maxBlockLightRadius;
    region.maxX = (cx + 1) * CHUNK_SIZE_X - 1 + maxBlockLightRadius;
    region.minY = 0;
    region.maxY = CHUNK_SIZE_Y - 1;
    region.minZ = cz * CHUNK_SIZE_Z - maxBlockLightRadius;
    region.maxZ = (cz + 1) * CHUNK_SIZE_Z - 1 + maxBlockLightRadius;
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
float getHeightAt(int wx, int wz, float& outBiomeA, float& outBiomeB, float& outWaterLevel) {
    outBiomeA = biomeNoiseA.GetNoise(static_cast<float>(wx), static_cast<float>(wz));
    outBiomeB = biomeNoiseB.GetNoise(static_cast<float>(wx), static_cast<float>(wz));

    constexpr float BASE_SEA_LEVEL = 62.0f;
    float seaNoise = seaLevelNoise.GetNoise(static_cast<float>(wx), static_cast<float>(wz));
    outWaterLevel = glm::clamp(BASE_SEA_LEVEL + seaNoise * 0.8f, 61.0f, 63.0f);

    float continent = mountainNoise.GetNoise(static_cast<float>(wx) * 0.45f, static_cast<float>(wz) * 0.45f);
    float broadHills = transitionNoise.GetNoise(static_cast<float>(wx), static_cast<float>(wz));
    float rollingDetail = detailNoise.GetNoise(static_cast<float>(wx), static_cast<float>(wz));

    // Minecraft 1.7.x-like terrain: broad rolling land first, rare mountains second.
    // Avoid high-frequency spikes so columns stay continuous and do not create
    // unnatural gaps between terrain layers.
    float height = 64.0f;
    height += continent * 14.0f;
    height += broadHills * 10.0f;
    height += rollingDetail * 2.25f;

    float mountainMask = glm::smoothstep(0.58f, 0.86f, continent);
    if (mountainMask > 0.0f) {
        float ridge = glm::max(0.0f, mountainNoise.GetNoise(static_cast<float>(wx) * 0.9f + 211.0f,
                                                            static_cast<float>(wz) * 0.9f - 97.0f));
        height += mountainMask * (18.0f + ridge * 20.0f);
    }

    float river = std::abs(riverNoise.GetNoise(static_cast<float>(wx), static_cast<float>(wz)));
    float riverMask = 1.0f - glm::smoothstep(0.045f, 0.165f, river);
    if (riverMask > 0.0f) {
        const float riverBed = outWaterLevel - 2.2f;
        height = glm::mix(height, riverBed, riverMask * 0.68f);
    }

    const float minH = 4.0f;
    const float maxH = CHUNK_SIZE_Y - 16.0f;
    return glm::clamp(height, minH, maxH);
}

enum BiomeId {
    BIOME_PLAINS = 0,
    BIOME_FOREST = 1,
    BIOME_MOUNTAINS = 2,
    BIOME_RIVER = 3,
    BIOME_OCEAN = 4,
    BIOME_DEEP_OCEAN = 5,
    BIOME_BEACH = 6,
    BIOME_STONY_PEAKS = 7,
    BIOME_DESERT = 8
};

int getBiome(float biomeA, float biomeB, float height, float waterLevel) {
    float depth = waterLevel - height;

    if (depth > 0.2f && depth < 5.0f) return BIOME_RIVER;
    if (depth > 7.0f) return BIOME_DEEP_OCEAN;
    if (depth > 1.5f) return BIOME_OCEAN;
    if (std::abs(height - waterLevel) < 1.2f) return BIOME_BEACH;

    if (height > waterLevel + 58.0f) return BIOME_STONY_PEAKS;
    if (height > waterLevel + 34.0f) return BIOME_MOUNTAINS;
    if (biomeA > 0.5f) return BIOME_FOREST;
    if (biomeB < -0.3f) return BIOME_DESERT;
    return BIOME_PLAINS;
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

void addTree(int cx, int cz, int lx, int lz, int surfaceY, std::vector<int>& blocks, float treeThreshold = 0.65f) {
    int worldX = cx * CHUNK_SIZE_X + lx, worldZ = cz * CHUNK_SIZE_Z + lz;
    float treeRand = treeNoise.GetNoise((float)worldX, (float)worldZ);
    if (treeRand < treeThreshold) return;

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

struct ColumnInfo { float height, biomeA, biomeB, waterLevel; int biome; };

std::shared_ptr<ChunkData> generateChunk(int cx, int cz) {
    auto data = std::make_shared<ChunkData>(cx, cz);
    ColumnInfo columns[CHUNK_SIZE_X][CHUNK_SIZE_Z];
    for (int x = 0; x < CHUNK_SIZE_X; ++x)
        for (int z = 0; z < CHUNK_SIZE_Z; ++z) {
            int worldX = cx * CHUNK_SIZE_X + x, worldZ = cz * CHUNK_SIZE_Z + z;
            float biomeA, biomeB, water;
            float landHeight = getHeightAt(worldX, worldZ, biomeA, biomeB, water);
            int waterSurfaceY = (int)water, surfaceY = (int)landHeight;
            surfaceY = std::clamp(surfaceY, 0, CHUNK_SIZE_Y - 1);
            waterSurfaceY = std::clamp(waterSurfaceY, 0, CHUNK_SIZE_Y - 1);
            int biome = getBiome(biomeA, biomeB, surfaceY, waterSurfaceY);
            columns[x][z] = {landHeight, biomeA, biomeB, water, biome};
        }
    
    for (int x = 0; x < CHUNK_SIZE_X; ++x)
        for (int z = 0; z < CHUNK_SIZE_Z; ++z) {
            const auto& col = columns[x][z];
            int surfaceY = (int)col.height, waterSurfaceY = (int)col.waterLevel, biome = col.biome;
            for (int y = 0; y < CHUNK_SIZE_Y; ++y) {
                int blockId = 0;
                if (biome == BIOME_RIVER || biome == BIOME_OCEAN || biome == BIOME_DEEP_OCEAN) {
                    if (y == surfaceY) blockId = (biome == BIOME_DEEP_OCEAN) ? 3 : 4;
                    else if (y > surfaceY && y <= waterSurfaceY) blockId = 5;
                    else if (y < surfaceY) blockId = (y > surfaceY - 4) ? 4 : 3;
                } else if (biome == BIOME_BEACH) {
                    if (y == surfaceY) blockId = 4;
                    else if (y < surfaceY) blockId = (y > surfaceY - 3) ? 4 : 3;
                } else {
                    if (y == surfaceY) {
                        if (biome == BIOME_MOUNTAINS || biome == BIOME_STONY_PEAKS) blockId = 3;
                        else if (biome == BIOME_DESERT) blockId = 4;
                        else blockId = 1;
                    } else if (y > surfaceY - 4 && y < surfaceY) {
                        if (biome == BIOME_MOUNTAINS || biome == BIOME_STONY_PEAKS) blockId = 3;
                        else if (biome == BIOME_DESERT) blockId = 4;
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
            if (hasWaterNeighbor && (col.biome == BIOME_BEACH || col.biome == BIOME_RIVER ||
                                      col.biome == BIOME_OCEAN || col.biome == BIOME_DEEP_OCEAN) &&
                landHeight > waterLevel && landHeight - waterLevel < 4.0f) {
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
            if ((biome == BIOME_PLAINS || biome == BIOME_FOREST) && surfaceY > waterSurfaceY + 2) {
                float treeThreshold = (biome == BIOME_FOREST) ? 0.42f : 0.82f;
                addTree(cx, cz, x, z, surfaceY, data->blocks, treeThreshold);
            }
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
unsigned int sunTexture = 0, moonPhasesTexture = 0, celestialVAO = 0, celestialVBO = 0;
unsigned int starProgram = 0, starVAO = 0, starVBO = 0;
unsigned int skyGlowProgram = 0, skyGlowVAO = 0, skyGlowVBO = 0;
int currentMoonPhase = 0;
bool moonWasBelowWorld = false;
bool isRaining = false;
FastNoiseLite starDistributionNoise;
FastNoiseLite starTwinkleNoise;

struct StarPoint {
    glm::vec3 direction;
    float brightness;
    float size;
    float twinkleSeed;
};

std::vector<StarPoint> starPoints;
std::vector<float> starVertexData;
int u_time_location, u_isWater_location, u_isRain_location, u_sunDir_location, u_sunIntensity_location, u_ambientBase_location, u_useBiomeTint_location, u_guiBiomeTint_location;

struct Chunk;
std::unordered_map<glm::ivec2, Chunk, hash_ivec2> loadedChunks;
static glm::vec3 lastCameraPosForWaterSort(0.0f);
static glm::vec3 lastCameraPosForAlphaSort(0.0f);
static std::vector<Chunk*> waterChunksCache;
static std::vector<Chunk*> alphaChunksCache;
static bool waterChunksCacheValid = false;
static bool alphaChunksCacheValid = false;
static std::queue<glm::ivec2> pendingBlockLightRebuildChunks;
static std::unordered_set<glm::ivec2, hash_ivec2> queuedBlockLightRebuildChunks;
static std::queue<glm::ivec2> pendingLightMeshRebuildChunks;
static std::unordered_set<glm::ivec2, hash_ivec2> queuedLightMeshRebuildChunks;
static std::queue<glm::ivec2> pendingInteractionMeshRebuildChunks;
static std::unordered_set<glm::ivec2, hash_ivec2> queuedInteractionMeshRebuildChunks;
static double lastInteractionMeshRebuildTime = 0.0;
static std::queue<glm::ivec2> pendingSkyLightColumnUpdates;
static std::unordered_set<glm::ivec2, hash_ivec2> queuedSkyLightColumnUpdates;
static Chunk* lastChunkForMesh = nullptr;
static glm::ivec2 lastChunkCoordsForMesh(0,0);

static GLint u_modelLoc = -1, u_viewLoc = -1, u_projLoc = -1;

void scheduleSkyLightColumnAsync(int x, int z) {
    glm::ivec2 column(x, z);
    if (queuedSkyLightColumnUpdates.insert(column).second) {
        pendingSkyLightColumnUpdates.push(column);
    }
}

void scheduleSkyLightColumnsAroundAsync(int x, int z) {
    const int offsets[5][2] = {
        {0, 0},
        {1, 0},
        {-1, 0},
        {0, 1},
        {0, -1}
    };

    for (int i = 0; i < 5; ++i) {
        scheduleSkyLightColumnAsync(x + offsets[i][0], z + offsets[i][1]);
    }
}

void processPendingSkyLightColumnUpdates(int maxColumnsPerFrame) {
    auto floorDiv = [](int value, int divisor) {
        return (value >= 0) ? value / divisor : (value - divisor + 1) / divisor;
    };

    int processed = 0;
    while (processed < maxColumnsPerFrame && !pendingSkyLightColumnUpdates.empty()) {
        glm::ivec2 column = pendingSkyLightColumnUpdates.front();
        pendingSkyLightColumnUpdates.pop();
        queuedSkyLightColumnUpdates.erase(column);

        rebuildSkyLightColumnBase(column.x, column.y);
        smoothSkyLightColumn(column.x, column.y);

        glm::ivec2 chunkPos(floorDiv(column.x, CHUNK_SIZE_X), floorDiv(column.y, CHUNK_SIZE_Z));
        if (queuedLightMeshRebuildChunks.insert(chunkPos).second) {
            pendingLightMeshRebuildChunks.push(chunkPos);
        }
        ++processed;
    }
}

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
