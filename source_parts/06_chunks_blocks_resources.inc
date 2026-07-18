// Музыка, структура чанка, доступ к блокам, загрузка ресурсов и raycast.
// Этот файл подключается из main2.cpp и рассчитан на сборку в одном translation unit.

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
    unsigned int grassOverlayVAO = 0, grassOverlayVBO = 0;
    size_t grassOverlayVertexCount = 0;
    bool meshReady = false, dirty = false;
    bool hasCutoutAlpha = false, hasBlendedAlpha = false;

    Chunk(int cx, int cz, bool loadFromFile) : pos(cx, cz) {
        std::lock_guard<std::mutex> lock(chunkMutex);
        if (loadFromFile) {
            std::string filename = (getCurrentChunksPath() / ("chunk_" + std::to_string(cx) + "_" + std::to_string(cz) + ".bin")).string();
            if (fs::exists(filename)) pendingLoad.insert(pos);
            else pendingGen.insert(pos);
        } else pendingGen.insert(pos);
    }
    ~Chunk() {
        for (int i=0; i<256; ++i) if (vao[i]) { glDeleteVertexArrays(1, &vao[i]); glDeleteBuffers(1, &vbo[i]); }
        if (grassOverlayVAO) glDeleteVertexArrays(1, &grassOverlayVAO);
        if (grassOverlayVBO) glDeleteBuffers(1, &grassOverlayVBO);
    }

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
        dirty = true;
        // Не сбрасываем meshReady сразу: старый меш остаётся на экране,
        // а новая геометрия планируется в очередь и пересобирается по бюджету кадра.
        // Это убирает фриз и исчезновение чанка при обычной установке/ломании блока.
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

    float getWaterDepthFactor(int lx, int ly, int lz) const {
        int wx = pos.x * CHUNK_SIZE_X + lx;
        int wz = pos.y * CHUNK_SIZE_Z + lz;
        int depth = 0;
        for (int sy = ly; sy >= 0 && depth < 12; --sy) {
            if (getBlockAtForMesh(wx, sy, wz) != 5) break;
            ++depth;
        }
        return glm::clamp(static_cast<float>(depth) / 12.0f, 0.0f, 1.0f);
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
        hasCutoutAlpha = false;
        hasBlendedAlpha = false;
        for (int i=0; i<256; ++i) if (vao[i]) { glDeleteVertexArrays(1, &vao[i]); glDeleteBuffers(1, &vbo[i]); vao[i]=vbo[i]=0; vertexCount[i]=0; }
        if (grassOverlayVAO) { glDeleteVertexArrays(1, &grassOverlayVAO); grassOverlayVAO = 0; }
        if (grassOverlayVBO) { glDeleteBuffers(1, &grassOverlayVBO); grassOverlayVBO = 0; }
        grassOverlayVertexCount = 0;
        std::unordered_map<int, std::vector<float>> verticesPerType;
        std::vector<float> grassOverlayVertices;
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

                const bool isGrassTop = (type == BLOCK_GRASS && faceIdx == 3);
                const bool isGrassSide = (type == BLOCK_GRASS && faceIdx != 2 && faceIdx != 3);
                const bool isFoliage = (type == BLOCK_OAK_LEAVES);
                const glm::vec3 biomeColor = (isGrassTop || isGrassSide || isFoliage)
                    ? getBiomeTintColor(static_cast<int>(ox), static_cast<int>(oz), isFoliage)
                    : glm::vec3(1.0f);
                const glm::vec3 faceTint = (isGrassTop || isFoliage) ? biomeColor : glm::vec3(1.0f);

                auto pushVertex = [&](std::vector<float>& target, int i, float texU, float texV, const glm::vec3& tint, float normalOffset) {
                    glm::vec3 vertexOffset(face[i], face[i+1], face[i+2]);
                    target.push_back(face[i] + ox + normal.x * normalOffset);
                    target.push_back(face[i+1] + oy + normal.y * normalOffset);
                    target.push_back(face[i+2] + oz + normal.z * normalOffset);
                    target.push_back(texU);
                    target.push_back(texV);
                    target.push_back(normal.x);
                    target.push_back(normal.y);
                    target.push_back(normal.z);

                    float light = getVertexLight(x, y, z, normal, vertexOffset, faceNeighborOffsets[faceIdx]);
                    float blockLight = (type == 5)
                        ? getWaterDepthFactor(x, y, z)
                        : getVertexBlockLight(x, y, z, normal, vertexOffset, faceNeighborOffsets[faceIdx]);
                    target.push_back(light);
                    target.push_back(blockLight);
                    target.push_back(tint.r);
                    target.push_back(tint.g);
                    target.push_back(tint.b);
                };
                
                for (int i=0; i<18; i+=3) {
                    int uvIdx = (i/3)*2; 
                    float u = baseUV[uvIdx], v = baseUV[uvIdx+1];
                    
                    // Для боковых граней в 3-частевой текстуре переворачиваем V
                    if (textureParts == 3 && faceIdx != 2 && faceIdx != 3) {
                        v = 1.0f - v;
                    }
                    
                    pushVertex(out, i, uOff + u * uScale, v, faceTint, 0.0f);
                    if (isGrassSide && grassSideOverlayTexture != 0) {
                        pushVertex(grassOverlayVertices, i, u, v, biomeColor, 0.006f);
                    }
                }
            };
            
            std::vector<float>& verts = verticesPerType[type];
            auto shouldRenderFace = [&](int neighbor) {
                if (neighbor == BLOCK_UNKNOWN) return false;
                if (type == 5) return neighbor != 5 && !isOpaque(neighbor);
                if (neighbor == 0 || neighbor == 5) return true;

                bool currentAlpha = isAlphaBlock(type);
                bool neighborAlpha = isAlphaBlock(neighbor);
                if (currentAlpha) {
                    // Листва/alpha рядом с непрозрачным блоком не должна рисовать спрятанную
                    // внутреннюю сторону: это убирает лишнюю геометрию и мерцание на контактах.
                    if (isOpaque(neighbor) && !neighborAlpha) return false;
                    // Одинаковые alpha-блоки используют свой режим: листва рисует внутренние грани,
                    // стекло скрывает их. Разные alpha-блоки (например листва рядом со стеклом)
                    // должны видеть грани друг друга, иначе часть текстуры пропадает.
                    if (neighbor == type) return blockDrawsSameAlphaFaces[type];
                    return true;
                }

                // Твёрдый блок должен быть виден за стеклом/листвой, поэтому его грань
                // рисуется рядом с alpha-блоком, но не рядом с другим твёрдым блоком.
                return neighborAlpha;
            };

            int neighbor;
            neighbor=getBlockAtForMesh(ox-1,oy,oz); if(shouldRenderFace(neighbor)) addFace(leftFace,0,verts);
            neighbor=getBlockAtForMesh(ox+1,oy,oz); if(shouldRenderFace(neighbor)) addFace(rightFace,1,verts);
            neighbor=getBlockAtForMesh(ox,oy,oz+1); if(shouldRenderFace(neighbor)) addFace(frontFace,4,verts);
            neighbor=getBlockAtForMesh(ox,oy,oz-1); if(shouldRenderFace(neighbor)) addFace(backFace,5,verts);
            neighbor=getBlockAtForMesh(ox,oy+1,oz); if(shouldRenderFace(neighbor)) addFace(topFace,3,verts);
            neighbor=getBlockAtForMesh(ox,oy-1,oz); if(shouldRenderFace(neighbor)) addFace(bottomFace,2,verts);
        }
        for (auto& [type, verts] : verticesPerType) {
            if (verts.empty()) continue;
            glGenVertexArrays(1, &vao[type]); glGenBuffers(1, &vbo[type]);
            glBindVertexArray(vao[type]); glBindBuffer(GL_ARRAY_BUFFER, vbo[type]);
            glBufferData(GL_ARRAY_BUFFER, verts.size()*sizeof(float), verts.data(), GL_STATIC_DRAW);
            glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,WORLD_VERTEX_FLOATS*sizeof(float),(void*)0); glEnableVertexAttribArray(0);
            glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,WORLD_VERTEX_FLOATS*sizeof(float),(void*)(3*sizeof(float))); glEnableVertexAttribArray(1);
            glVertexAttribPointer(2,3,GL_FLOAT,GL_FALSE,WORLD_VERTEX_FLOATS*sizeof(float),(void*)(5*sizeof(float))); glEnableVertexAttribArray(2);
            glVertexAttribPointer(3,1,GL_FLOAT,GL_FALSE,WORLD_VERTEX_FLOATS*sizeof(float),(void*)(8*sizeof(float))); glEnableVertexAttribArray(3);
            glVertexAttribPointer(4,1,GL_FLOAT,GL_FALSE,WORLD_VERTEX_FLOATS*sizeof(float),(void*)(9*sizeof(float))); glEnableVertexAttribArray(4);
            glVertexAttribPointer(5,3,GL_FLOAT,GL_FALSE,WORLD_VERTEX_FLOATS*sizeof(float),(void*)(10*sizeof(float))); glEnableVertexAttribArray(5);
            vertexCount[type] = verts.size() / WORLD_VERTEX_FLOATS;
            if (isAlphaBlock(type)) {
                if (blockDrawsSameAlphaFaces[type]) hasCutoutAlpha = true;
                else hasBlendedAlpha = true;
            }
        }
        if (!grassOverlayVertices.empty()) {
            glGenVertexArrays(1, &grassOverlayVAO);
            glGenBuffers(1, &grassOverlayVBO);
            glBindVertexArray(grassOverlayVAO);
            glBindBuffer(GL_ARRAY_BUFFER, grassOverlayVBO);
            glBufferData(GL_ARRAY_BUFFER, grassOverlayVertices.size()*sizeof(float), grassOverlayVertices.data(), GL_STATIC_DRAW);
            glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,WORLD_VERTEX_FLOATS*sizeof(float),(void*)0); glEnableVertexAttribArray(0);
            glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,WORLD_VERTEX_FLOATS*sizeof(float),(void*)(3*sizeof(float))); glEnableVertexAttribArray(1);
            glVertexAttribPointer(2,3,GL_FLOAT,GL_FALSE,WORLD_VERTEX_FLOATS*sizeof(float),(void*)(5*sizeof(float))); glEnableVertexAttribArray(2);
            glVertexAttribPointer(3,1,GL_FLOAT,GL_FALSE,WORLD_VERTEX_FLOATS*sizeof(float),(void*)(8*sizeof(float))); glEnableVertexAttribArray(3);
            glVertexAttribPointer(4,1,GL_FLOAT,GL_FALSE,WORLD_VERTEX_FLOATS*sizeof(float),(void*)(9*sizeof(float))); glEnableVertexAttribArray(4);
            glVertexAttribPointer(5,3,GL_FLOAT,GL_FALSE,WORLD_VERTEX_FLOATS*sizeof(float),(void*)(10*sizeof(float))); glEnableVertexAttribArray(5);
            grassOverlayVertexCount = grassOverlayVertices.size() / WORLD_VERTEX_FLOATS;
        }
        meshReady = true;
        alphaChunksCacheValid = false;
        waterChunksCacheValid = false;
    }

    void render() {
        if (!data || !meshReady) return;
        glUniform1i(u_useBiomeTint_location, 1);
        for (int type=0; type<256; ++type) {
            if (type==5 || !vao[type] || isAlphaBlock(type)) continue;
            auto it = blockTypes.find(type); if (it==blockTypes.end()) continue;
            glUniform1i(u_isWater_location, 0);
            glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, it->second.textureID);
            glBindVertexArray(vao[type]); glDrawArrays(GL_TRIANGLES, 0, vertexCount[type]);
        }
    }

    void renderGrassOverlay() {
        if (!meshReady || !grassOverlayVAO || !grassSideOverlayTexture || grassOverlayVertexCount == 0) return;
        glUniform1i(u_useBiomeTint_location, 1);
        glUniform1i(u_isWater_location, 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, grassSideOverlayTexture);
        glBindVertexArray(grassOverlayVAO);
        glDrawArrays(GL_TRIANGLES, 0, grassOverlayVertexCount);
    }
    void renderAlphaBlocks(bool drawSameTypeFacesPass) {
        if (!data || !meshReady) return;
        glUniform1i(u_useBiomeTint_location, 1);
        for (int type=0; type<256; ++type) {
            if (type==5 || !vao[type] || !isAlphaBlock(type)) continue;
            if (blockDrawsSameAlphaFaces[type] != drawSameTypeFacesPass) continue;
            auto it = blockTypes.find(type); if (it==blockTypes.end()) continue;
            glUniform1i(u_isWater_location, 0);
            glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, it->second.textureID);
            glBindVertexArray(vao[type]); glDrawArrays(GL_TRIANGLES, 0, vertexCount[type]);
        }
    }
    void renderWater() {
        if (!meshReady || !vao[5]) return;
        GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);
        if (cullWasEnabled) glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); glDepthMask(GL_FALSE);
        glUniform1i(u_useBiomeTint_location, 0);
        glUniform1i(u_isWater_location, 1);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, blockTypes[5].textureID);
        glBindVertexArray(vao[5]); glDrawArrays(GL_TRIANGLES, 0, vertexCount[5]);
        glDepthMask(GL_TRUE); glDisable(GL_BLEND);
        if (cullWasEnabled) glEnable(GL_CULL_FACE);
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

void updateBlockLightIncremental(int x, int y, int z, int oldEmission, int newEmission) {
    const int dirs[6][3] = {
        {1,0,0}, {-1,0,0},
        {0,1,0}, {0,-1,0},
        {0,0,1}, {0,0,-1}
    };

    struct RemovalNode {
        int x, y, z;
        uint8_t light;
    };

    std::queue<RemovalNode> removalQueue;
    std::queue<LightNode> addQueue;
    LightRegion changedRegion{x, x, y, y, z, z};
    bool hasLightChanges = false;
    auto markLightChanged = [&](int cx, int cy, int cz) {
        hasLightChanges = true;
        changedRegion.minX = std::min(changedRegion.minX, cx);
        changedRegion.maxX = std::max(changedRegion.maxX, cx);
        changedRegion.minY = std::min(changedRegion.minY, cy);
        changedRegion.maxY = std::max(changedRegion.maxY, cy);
        changedRegion.minZ = std::min(changedRegion.minZ, cz);
        changedRegion.maxZ = std::max(changedRegion.maxZ, cz);
    };

    if (oldEmission > 0) {
        uint8_t startLight = std::max<uint8_t>(getBlockLightAt(x, y, z), static_cast<uint8_t>(std::min(oldEmission, MAX_LIGHT)));
        setBlockLightAt(x, y, z, 0);
        markLightChanged(x, y, z);
        removalQueue.push({x, y, z, startLight});
    }

    const int MAX_REMOVAL_ITERATIONS = 50000;
    int removalIterations = 0;
    while (!removalQueue.empty() && removalIterations < MAX_REMOVAL_ITERATIONS) {
        ++removalIterations;
        RemovalNode node = removalQueue.front();
        removalQueue.pop();

        for (int i = 0; i < 6; ++i) {
            int nx = node.x + dirs[i][0];
            int ny = node.y + dirs[i][1];
            int nz = node.z + dirs[i][2];
            if (ny < 0 || ny >= CHUNK_SIZE_Y) continue;

            uint8_t neighborLight = getBlockLightAt(nx, ny, nz);
            if (neighborLight == 0) continue;

            int blockId = getBlockAt(nx, ny, nz);
            int emission = (blockId >= 0 && blockId < 256) ? blockLightEmission[blockId] : 0;
            if (emission > 0 && neighborLight <= emission) {
                addQueue.push({nx, ny, nz, nx, ny, nz, emission, static_cast<uint8_t>(std::min(emission, MAX_LIGHT))});
                continue;
            }

            if (neighborLight < node.light) {
                setBlockLightAt(nx, ny, nz, 0);
                markLightChanged(nx, ny, nz);
                removalQueue.push({nx, ny, nz, neighborLight});
            } else {
                addQueue.push({nx, ny, nz, nx, ny, nz, neighborLight, neighborLight});
            }
        }
    }

    if (newEmission > 0) {
        uint8_t sourceLight = static_cast<uint8_t>(std::min(newEmission, MAX_LIGHT));
        setBlockLightAt(x, y, z, sourceLight);
        markLightChanged(x, y, z);
        addQueue.push({x, y, z, x, y, z, newEmission, sourceLight});
    }

    const int MAX_ADD_ITERATIONS = 50000;
    int addIterations = 0;
    while (!addQueue.empty() && addIterations < MAX_ADD_ITERATIONS) {
        ++addIterations;
        LightNode node = addQueue.front();
        addQueue.pop();

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
            if (propagatedLight > current) {
                setBlockLightAt(nx, ny, nz, propagatedLight);
                markLightChanged(nx, ny, nz);
                addQueue.push({nx, ny, nz, node.sourceX, node.sourceY, node.sourceZ, node.radius, propagatedLight});
            }
        }
    }

    if (hasLightChanges) {
        changedRegion.minY = 0;
        changedRegion.maxY = CHUNK_SIZE_Y - 1;
        scheduleChunkMeshRebuildRegion(changedRegion);
    }
}

void rebuildSkyLightRegion(const LightRegion& rawRegion) {
    LightRegion region = rawRegion;
    region.minY = std::max(0, region.minY);
    region.maxY = std::min(CHUNK_SIZE_Y - 1, region.maxY);
    if (region.minY > region.maxY) return;

    for (int x = region.minX; x <= region.maxX; ++x) {
        for (int z = region.minZ; z <= region.maxZ; ++z) {
            for (int y = region.minY; y <= region.maxY; ++y) {
                setSkyLightAt(x, y, z, 0);
            }
        }
    }

    std::queue<LightNode> queue;
    for (int x = region.minX; x <= region.maxX; ++x) {
        for (int z = region.minZ; z <= region.maxZ; ++z) {
            bool blockedAbove = false;
            for (int y = CHUNK_SIZE_Y - 1; y >= 0; --y) {
                int blockId = getBlockAt(x, y, z);
                if (isOpaque(blockId)) {
                    blockedAbove = true;
                    if (isInsideLightRegion(region, x, y, z)) {
                        setSkyLightAt(x, y, z, 0);
                    }
                    continue;
                }

                if (!blockedAbove && isInsideLightRegion(region, x, y, z)) {
                    setSkyLightAt(x, y, z, static_cast<uint8_t>(MAX_LIGHT));
                    queue.push({x, y, z, x, y, z, MAX_LIGHT, static_cast<uint8_t>(MAX_LIGHT)});
                }
            }
        }
    }

    const int dirs[6][3] = {
        {1,0,0}, {-1,0,0},
        {0,1,0}, {0,-1,0},
        {0,0,1}, {0,0,-1}
    };

    const int MAX_ITERATIONS = 250000;
    int iterations = 0;
    while (!queue.empty() && iterations < MAX_ITERATIONS) {
        ++iterations;
        LightNode node = queue.front();
        queue.pop();

        for (int i = 0; i < 6; ++i) {
            int nx = node.x + dirs[i][0];
            int ny = node.y + dirs[i][1];
            int nz = node.z + dirs[i][2];

            if (!isInsideLightRegion(region, nx, ny, nz)) continue;
            if (ny < 0 || ny >= CHUNK_SIZE_Y) continue;

            int blockId = getBlockAt(nx, ny, nz);
            int opacity = getLightOpacity(blockId);
            if (opacity > MAX_LIGHT) continue;

            int attenuation = opacity;
            if (dirs[i][1] == -1 && dirs[i][0] == 0 && dirs[i][2] == 0 && opacity == 0 && node.light == MAX_LIGHT) {
                attenuation = 0;
            } else {
                attenuation = std::max(1, attenuation);
            }

            if (node.light <= attenuation) continue;
            uint8_t newLight = static_cast<uint8_t>(node.light - attenuation);
            if (newLight > getSkyLightAt(nx, ny, nz)) {
                setSkyLightAt(nx, ny, nz, newLight);
                queue.push({nx, ny, nz, node.sourceX, node.sourceY, node.sourceZ, node.radius, newLight});
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
        // Не скрываем весь чанк на кадр при пересчёте света: ближайшие меши
        // перестраиваются явно после изменения блока.
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
        // Не сбрасываем meshReady здесь, иначе при установке/ломании блока
        // на время пропадают целые чанки, затронутые областью освещения.
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

int getBlockAtPoint(const glm::vec3& point) {
    // Blocks are centered on integer coordinates and occupy
    // [coord - 0.5; coord + 0.5]. floor(worldCoord) can therefore select
    // a neighboring wall block when the player is pressed against a side.
    return getBlockAt(
        static_cast<int>(std::floor(point.x + 0.5f)),
        static_cast<int>(std::floor(point.y + 0.5f)),
        static_cast<int>(std::floor(point.z + 0.5f))
    );
}

int getBlockUnderCamera(const glm::vec3& cameraPosition, const glm::vec3& feetPosition) {
    glm::vec3 samplePoint(cameraPosition.x, feetPosition.y - 0.1f, cameraPosition.z);
    return getBlockAtPoint(samplePoint);
}

bool chunkDataNeedsBlockLightRebuild(const std::shared_ptr<ChunkData>& data) {
    if (!data || maxBlockLightRadius <= 0) return false;

    bool hasEmitter = false;
    bool hasSpreadLight = false;
    for (int idx = 0; idx < CHUNK_VOLUME; ++idx) {
        int blockId = data->blocks[idx];
        int emission = (blockId >= 0 && blockId < 256) ? blockLightEmission[blockId] : 0;
        if (emission > 0) hasEmitter = true;
        if (data->blockLight[idx] > emission) {
            hasSpreadLight = true;
            break;
        }
    }

    return hasEmitter && !hasSpreadLight;
}

void scheduleBlockLightRebuild(const glm::ivec2& chunkPos) {
    if (queuedBlockLightRebuildChunks.insert(chunkPos).second) {
        pendingBlockLightRebuildChunks.push(chunkPos);
    }
}

void processPendingBlockLightRebuilds(int maxPerFrame) {
    int processed = 0;
    while (processed < maxPerFrame && !pendingBlockLightRebuildChunks.empty()) {
        glm::ivec2 chunkPos = pendingBlockLightRebuildChunks.front();
        pendingBlockLightRebuildChunks.pop();
        queuedBlockLightRebuildChunks.erase(chunkPos);

        auto loadedIt = loadedChunks.find(chunkPos);
        if (loadedIt == loadedChunks.end() || !loadedIt->second.data) continue;

        loadedIt->second.meshReady = false;
        loadedIt->second.invalidateNeighbors();
        rebuildBlockLightAroundChunk(chunkPos.x, chunkPos.y);
        ++processed;
    }
}
void setBlockAt(int wx, int wy, int wz, int type) {
    if (wy<0||wy>=CHUNK_SIZE_Y) return;
    int cx = (wx>=0) ? wx/CHUNK_SIZE_X : (wx-CHUNK_SIZE_X+1)/CHUNK_SIZE_X;
    int cz = (wz>=0) ? wz/CHUNK_SIZE_Z : (wz-CHUNK_SIZE_Z+1)/CHUNK_SIZE_Z;
    auto it = loadedChunks.find({cx,cz}); if (it==loadedChunks.end()) return;
    int lx = wx - cx*CHUNK_SIZE_X, lz = wz - cz*CHUNK_SIZE_Z;
    int oldType = it->second.getLocalBlock(lx, wy, lz);
    const int oldOpacity = getLightOpacity(oldType);
    const int newOpacity = getLightOpacity(type);
    const int oldEmission = (oldType >= 0 && oldType < 256) ? blockLightEmission[oldType] : 0;
    const int newEmission = (type >= 0 && type < 256) ? blockLightEmission[type] : 0;
    bool skyLightRelevant = oldOpacity != newOpacity && (
        getSkyLightAt(wx, wy, wz) > 0 ||
        getSkyLightAt(wx + 1, wy, wz) > 0 || getSkyLightAt(wx - 1, wy, wz) > 0 ||
        getSkyLightAt(wx, wy + 1, wz) > 0 || getSkyLightAt(wx, wy - 1, wz) > 0 ||
        getSkyLightAt(wx, wy, wz + 1) > 0 || getSkyLightAt(wx, wy, wz - 1) > 0
    );
    bool blockLightRelevant = oldOpacity != newOpacity && (
        getBlockLightAt(wx, wy, wz) > 0 ||
        getBlockLightAt(wx + 1, wy, wz) > 0 || getBlockLightAt(wx - 1, wy, wz) > 0 ||
        getBlockLightAt(wx, wy + 1, wz) > 0 || getBlockLightAt(wx, wy - 1, wz) > 0 ||
        getBlockLightAt(wx, wy, wz + 1) > 0 || getBlockLightAt(wx, wy, wz - 1) > 0
    );
    it->second.setLocalBlock(lx,wy,lz,type);
    if (!skyLightRelevant && oldOpacity != newOpacity) {
        skyLightRelevant = getSkyLightAt(wx, wy + 1, wz) > 0;
    }
    if (!blockLightRelevant && oldOpacity != newOpacity) {
        blockLightRelevant = getBlockLightAt(wx, wy + 1, wz) > 0;
    }
    // Соседние чанки больше не инвалидируются синхронно: если блок стоит на границе,
    // rebuildChunkMeshesImmediatelyAround ниже поставит нужные чанки в очередь.
    waterChunksCacheValid = false;

    const bool lightEmitterChanged = oldEmission != newEmission;
    if (lightEmitterChanged) {
        updateBlockLightIncremental(wx, wy, wz, oldEmission, newEmission);
        if (oldOpacity != newOpacity) {
            scheduleSkyLightColumnsAroundAsync(wx, wz);
        }
    }

    // Не запускаем полный rebuildSkyLightRegion из пути клика: он обходит большую область
    // по всей высоте мира и именно на обычных блоках даёт заметный фриз. Для смены
    // непрозрачности достаточно дешёвого вертикального обновления ближайших колонок.
    if (!lightEmitterChanged && skyLightRelevant) {
        scheduleSkyLightColumnsAroundAsync(wx, wz);
    }

    // Полную перестройку блочного света оставляем только когда рядом реально был
    // распространяющийся блочный свет. В обычном случае это false, поэтому постановка
    // камня/земли/досок больше не запускает тяжёлый пересчёт освещения региона.
    if (blockLightRelevant && !lightEmitterChanged) {
        onBlockChangedGlobal(wx, wy, wz, false, true);
    }
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

    std::vector<glm::ivec2> integratedChunks;
    integratedChunks.reserve(readyChunks.size());

    for (auto& [chunkPos, chunkData] : readyChunks) {
        auto loadedIt = loadedChunks.find(chunkPos);
        if (loadedIt == loadedChunks.end()) continue;

        loadedIt->second.data = chunkData;
        loadedIt->second.meshReady = false;
        loadedIt->second.dirty = false;
        loadedIt->second.invalidateNeighbors();

        if (chunkDataNeedsBlockLightRebuild(chunkData)) {
            scheduleBlockLightRebuild(chunkPos);
        }
    }
}

void updateChunksAroundCamera(const glm::vec3& cameraPos, bool loadFromFile) {
    int centerCX = (int)std::floor(cameraPos.x / CHUNK_SIZE_X);
    int centerCZ = (int)std::floor(cameraPos.z / CHUNK_SIZE_Z);
    static glm::ivec2 lastRequestedCenter(std::numeric_limits<int>::min(), std::numeric_limits<int>::min());

    const int integrateBudget = fastChunkLoadingMode ? 24 : 2;
    integratePendingChunkData(integrateBudget);
    processPendingBlockLightRebuilds(fastChunkLoadingMode ? 2 : 1);
    processPendingSkyLightColumnUpdates(fastChunkLoadingMode ? 24 : 2);
    processPendingInteractionMeshRebuilds(1);
    processPendingLightMeshRebuilds(fastChunkLoadingMode ? 6 : 4);

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
            alphaChunksCacheValid = false;
            lastChunkForMesh = nullptr;  // ДОБАВЬТЕ ЭТУ СТРОКУ
        }
}

void buildChunkMeshesNearCamera(int maxPerFrame) {
    struct Candidate {
        float dist2;
        Chunk* chunk;
    };

    static std::vector<Candidate> candidates;
    candidates.clear();
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

    if (candidates.size() > 1) {
        std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
            return a.dist2 < b.dist2;
        });
    }

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

    lastInteractionMeshRebuildTime = glfwGetTime();

    for (int i = 0; i < rebuildCount; ++i) {
        auto it = loadedChunks.find(rebuildList[i]);
        if (it == loadedChunks.end() || !it->second.data) continue;

        // Обычная постановка/ломание не должна попадать в световую очередь,
        // иначе следующий кадр всё равно ловит buildMesh()-спайк. Держим
        // отдельную, медленную очередь пользовательских правок.
        if (queuedInteractionMeshRebuildChunks.insert(rebuildList[i]).second) {
            pendingInteractionMeshRebuildChunks.push(rebuildList[i]);
        }
    }
}

void processPendingInteractionMeshRebuilds(int maxPerFrame) {
    if (pendingInteractionMeshRebuildChunks.empty()) return;

    const double now = glfwGetTime();
    constexpr double MIN_SECONDS_BETWEEN_INTERACTION_MESH_BUILDS = 0.045;
    if (now - lastInteractionMeshRebuildTime < MIN_SECONDS_BETWEEN_INTERACTION_MESH_BUILDS) return;

    int processed = 0;
    while (processed < maxPerFrame && !pendingInteractionMeshRebuildChunks.empty()) {
        glm::ivec2 chunkPos = pendingInteractionMeshRebuildChunks.front();
        pendingInteractionMeshRebuildChunks.pop();
        queuedInteractionMeshRebuildChunks.erase(chunkPos);

        auto it = loadedChunks.find(chunkPos);
        if (it == loadedChunks.end() || !it->second.data) continue;
        it->second.buildMesh();
        lastInteractionMeshRebuildTime = now;
        ++processed;
    }
}

void scheduleChunkMeshRebuildRegion(const LightRegion& rawRegion) {
    auto floorDiv = [](int value, int divisor) {
        return (value >= 0) ? value / divisor : (value - divisor + 1) / divisor;
    };

    int minCX = floorDiv(rawRegion.minX, CHUNK_SIZE_X);
    int maxCX = floorDiv(rawRegion.maxX, CHUNK_SIZE_X);
    int minCZ = floorDiv(rawRegion.minZ, CHUNK_SIZE_Z);
    int maxCZ = floorDiv(rawRegion.maxZ, CHUNK_SIZE_Z);

    for (int cx = minCX; cx <= maxCX; ++cx) {
        for (int cz = minCZ; cz <= maxCZ; ++cz) {
            glm::ivec2 chunkPos(cx, cz);
            if (queuedLightMeshRebuildChunks.insert(chunkPos).second) {
                pendingLightMeshRebuildChunks.push(chunkPos);
            }
        }
    }
}

void processPendingLightMeshRebuilds(int maxPerFrame) {
    int processed = 0;
    while (processed < maxPerFrame && !pendingLightMeshRebuildChunks.empty()) {
        glm::ivec2 chunkPos = pendingLightMeshRebuildChunks.front();
        pendingLightMeshRebuildChunks.pop();
        queuedLightMeshRebuildChunks.erase(chunkPos);

        auto it = loadedChunks.find(chunkPos);
        if (it == loadedChunks.end() || !it->second.data) continue;
        it->second.buildMesh();
        ++processed;
    }
}

void updateWaterChunksCache() {
    if (waterChunksCacheValid) return;
    waterChunksCache.clear();
    for (auto& pair : loadedChunks) if (pair.second.vao[5]) waterChunksCache.push_back(&pair.second);
    waterChunksCacheValid = true;
}

void updateAlphaChunksCache() {
    if (alphaChunksCacheValid) return;
    alphaChunksCache.clear();
    alphaChunksCache.reserve(loadedChunks.size());
    for (auto& pair : loadedChunks) {
        if (pair.second.meshReady && (pair.second.hasCutoutAlpha || pair.second.hasBlendedAlpha)) {
            alphaChunksCache.push_back(&pair.second);
        }
    }
    lastCameraPosForAlphaSort = glm::vec3(std::numeric_limits<float>::infinity());
    alphaChunksCacheValid = true;
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
        bt.transparent = item.value("transparent", false);
        bool codeAlpha = codeAlphaBlocks.find(bt.id) != codeAlphaBlocks.end();
        if (codeAlpha) {
            bt.transparent = true;
        }
        bt.textureID = loadTextureStrip(texPath.c_str(), bt.id == 5 || bt.transparent);
        bt.lightEmission = item.value("light", 0);
        bt.textureParts = item.value("texture_parts", 3);  // По умолчанию 3 части
        
        blockTypes[bt.id] = bt;
        isSolidBlockFast[bt.id] = (bt.id!=0 && bt.id!=5);
        blockHasAlpha[bt.id] = (bt.id != 5 && bt.transparent);
        blockDrawsSameAlphaFaces[bt.id] = codeAlpha;
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

bool isWaterBlockAt(float x, float y, float z) {
    int bx = static_cast<int>(std::floor(x));
    int by = static_cast<int>(std::floor(y));
    int bz = static_cast<int>(std::floor(z));
    return getBlockAt(bx, by, bz) == 5;
}

bool isCameraUnderwater() {
    return isWaterBlockAt(renderCameraPos.x, renderCameraPos.y, renderCameraPos.z);
}

void renderUnderwaterOverlay(int screenW, int screenH, float currentTime) {
    if (!underwaterOverlayTexture) return;

    const float tile = 128.0f;
    const float uOffset = std::fmod(currentTime * 0.03f + yaw * 0.0025f, 1.0f);
    const float vOffset = std::fmod(currentTime * 0.02f - pitch * 0.0035f, 1.0f);
    const float uScale = static_cast<float>(screenW) / tile;
    const float vScale = static_cast<float>(screenH) / tile;

    drawRectangleUV(0.0f, 0.0f, static_cast<float>(screenW), static_cast<float>(screenH),
        underwaterOverlayTexture, screenW, screenH,
        uOffset, vOffset, uOffset + uScale, vOffset + vScale);
}

bool isPlayerInWater() {
    glm::vec3 feetPos = cameraPos - glm::vec3(0.0f, EYE_HEIGHT, 0.0f);
    
    // Проверяем несколько точек для более точного определения
    glm::vec3 checkPoints[] = {
        feetPos,                                    // Ноги
        feetPos + glm::vec3(0.0f, 0.5f, 0.0f),     // Колени
        feetPos + glm::vec3(0.0f, 1.0f, 0.0f),     // Пояс
        cameraPos                                   // Голова
    };
    
    for (const auto& point : checkPoints) {
        int blockX = static_cast<int>(std::floor(point.x));
        int blockY = static_cast<int>(std::floor(point.y));
        int blockZ = static_cast<int>(std::floor(point.z));
        
        if (getBlockAt(blockX, blockY, blockZ) == 5) {
            return true;
        }
    }
    return false;
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
