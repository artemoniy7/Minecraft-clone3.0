// Состояния игры, основной gameplay, рендер игрока, неба, облаков, дождя и меню паузы.
// Этот файл подключается из main2.cpp и рассчитан на сборку в одном translation unit.

enum class GameState {
    MAIN_MENU,
    WORLD_SELECT_MENU,
    LOADING_GAME,
    LANGUAGE_MENU,
    IN_GAME,
    MAIN_MENU_OPTIONS,
    PAUSE_MENU,
    CREATIVE_INVENTORY,
    DELETE_WORLD_CONFIRM_MENU
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
const float STEP_GROUND_GRACE_TIME = 0.12f;

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
            bool foliageTint = false;
            const bool tintFace = isBiomeTintedGuiBlockFace(blockType, faceIdx, foliageTint);
            const glm::vec3 faceTintMask = tintFace ? glm::vec3(1.0f) : glm::vec3(0.0f);
            
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
                vertices.push_back(faceTintMask.r);
                vertices.push_back(faceTintMask.g);
                vertices.push_back(faceTintMask.b);
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
        
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, WORLD_VERTEX_FLOATS*sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, WORLD_VERTEX_FLOATS*sizeof(float), (void*)(3*sizeof(float)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, WORLD_VERTEX_FLOATS*sizeof(float), (void*)(5*sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, WORLD_VERTEX_FLOATS*sizeof(float), (void*)(8*sizeof(float)));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, WORLD_VERTEX_FLOATS*sizeof(float), (void*)(9*sizeof(float)));
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(5, 3, GL_FLOAT, GL_FALSE, WORLD_VERTEX_FLOATS*sizeof(float), (void*)(10*sizeof(float)));
        glEnableVertexAttribArray(5);
        
        singleBlockVAO[blockType] = vao;
        singleBlockVBO[blockType] = vbo;
        singleBlockVertexCount[blockType] = vertices.size() / WORLD_VERTEX_FLOATS;
    }
    
    auto it = blockTypes.find(blockType);
    if (it != blockTypes.end()) {
        const bool usesGuiBiomeTint = (blockType == BLOCK_GRASS || blockType == BLOCK_OAK_LEAVES);
        if (usesGuiBiomeTint) {
            const glm::vec3 guiTint = getGuiBiomeTintColor(blockType == BLOCK_OAK_LEAVES);
            glUniform3fv(u_guiBiomeTint_location, 1, glm::value_ptr(guiTint));
        }
        glUniform1i(u_useBiomeTint_location, usesGuiBiomeTint ? 2 : 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, it->second.textureID);
        glBindVertexArray(singleBlockVAO[blockType]);
        glDrawArrays(GL_TRIANGLES, 0, singleBlockVertexCount[blockType]);
        glUniform1i(u_useBiomeTint_location, 0);
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
    pendingLightMeshRebuildChunks = std::queue<glm::ivec2>();
    queuedLightMeshRebuildChunks.clear();
    pendingInteractionMeshRebuildChunks = std::queue<glm::ivec2>();
    queuedInteractionMeshRebuildChunks.clear();
    lastInteractionMeshRebuildTime = 0.0;
    pendingSkyLightColumnUpdates = std::queue<glm::ivec2>();
    queuedSkyLightColumnUpdates.clear();
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

void renderDeleteWorldConfirmMenu(int screenW, int screenH);
void handleDeleteWorldConfirmMenuClick(GLFWwindow* window, int button);

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

void renderDeleteWorldConfirmMenu(int screenW, int screenH) {
    glClearColor(0,0,0,1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const unsigned int backgroundTexture = menuBackgroundLightTexture != 0 ? menuBackgroundLightTexture : menuBackgroundTexture;
    if (backgroundTexture) {
        drawTiledBackground(backgroundTexture, screenW, screenH);
    }

    const char* question = tr("Are you sure you want to delete this world forever?",
                              "Вы точно хотите удалить этот мир навсегда?",
                              "このワールドを完全に削除しますか？");
    drawMinecraftTextCentered(question, screenW * 0.5f, screenH * 0.30f, 2.0f, screenW, screenH,
                              glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

    for (int i = 0; i < DELETE_CONFIRM_BUTTON_COUNT; ++i) {
        deleteConfirmButtons[i].updateAbsolute(screenW, screenH);

        const bool hovered = isMouseOverButton(deleteConfirmButtons[i], mouseX, mouseY);
        unsigned int tex = (hovered && menuButtonHighlightTexture) ? menuButtonHighlightTexture : menuButtonTexture;
        drawRectangle(deleteConfirmButtons[i].absX, deleteConfirmButtons[i].absY,
                      deleteConfirmButtons[i].absW, deleteConfirmButtons[i].absH, tex, screenW, screenH);
        drawMinecraftTextCentered(deleteConfirmButtons[i].label,
                                  deleteConfirmButtons[i].absX + deleteConfirmButtons[i].absW * 0.5f,
                                  deleteConfirmButtons[i].absY + deleteConfirmButtons[i].absH * 0.52f,
                                  fitMinecraftButtonTextScale(deleteConfirmButtons[i].label,
                                                              deleteConfirmButtons[i].absW,
                                                              deleteConfirmButtons[i].absH),
                                  screenW, screenH, getMenuTextColor(hovered));
    }

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}

void handleDeleteWorldConfirmMenuClick(GLFWwindow* window, int button) {
    if (button != GLFW_MOUSE_BUTTON_LEFT) return;

    double mx, my;
    glfwGetCursorPos(window, &mx, &my);
    for (int i = 0; i < DELETE_CONFIRM_BUTTON_COUNT; ++i) {
        if (mx < deleteConfirmButtons[i].absX || mx > deleteConfirmButtons[i].absX + deleteConfirmButtons[i].absW ||
            my < deleteConfirmButtons[i].absY || my > deleteConfirmButtons[i].absY + deleteConfirmButtons[i].absH) {
            continue;
        }

        deleteConfirmButtons[i].clicked = true;
        soundManager.playUISound("ui");
        if (i == 0) {
            deleteSelectedWorld();
        }
        currentState = GameState::WORLD_SELECT_MENU;
        deleteConfirmButtons[i].clicked = false;
        break;
    }
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
    optionsDifficultyButton.setAbsolute(rightX, topY - buttonH * 0.5f + yOffset, buttonW, buttonH);

    // Структура как на скриншоте
    // Одиночная кнопка справа под Difficulty
    optionsRow2Buttons[4].setAbsolute(rightX, optionsDifficultyButton.absY + buttonH + rowGap, buttonW, buttonH, "Super Secret Settings...");

    // Две колонки ниже
    const float startRowsY = optionsRow2Buttons[4].absY + buttonH + rowGap;
    const char* leftLabels[4] = { "Music & Sounds...", "Video Settings...", "Language...", "Resource Packs..." };
    const char* rightLabels[4] = { "Broadcast Settings...", "Controls...", "Multiplayer Settings...", "Snooper Settings..." };
    for (int i = 0; i < 4; ++i) {
        const float rowY = startRowsY + i * (buttonH + rowGap * 0.55f);
        optionsRow1Buttons[i].setAbsolute(leftX, rowY, buttonW, buttonH, leftLabels[i]);
        optionsRow2Buttons[i].setAbsolute(rightX, rowY, buttonW, buttonH, rightLabels[i]);
    }

    const float doneW = 560.0f * OPTIONS_UI_SCALE;
    const float doneH = 50.0f * OPTIONS_UI_SCALE;
    optionsDoneButton.setAbsolute((screenW - doneW) * 0.5f, optionsRow1Buttons[3].absY + buttonH + 46.0f * OPTIONS_UI_SCALE, doneW, doneH, tr("Done", "Готово", "完了"));
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
        } else if (i == 2) {
            currentState = GameState::DELETE_WORLD_CONFIRM_MENU;
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
        invulnerabilityTimer = std::max(0.0f, invulnerabilityTimer - deltaTime);
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
        buildChunkMeshesNearCamera(1);
        processInputInGame(window, deltaTime);
        updateMusic();
        updateMood(deltaTime);
    }
    else if (currentState == GameState::CREATIVE_INVENTORY) {
        // В инвентаре мир продолжает обновляться (чанки, музыка, настроение)
        // Но игрок не двигается (движение отключено в processInputInGame)
        updateChunksAroundCamera(cameraPos, isLoadingGame);
        buildChunkMeshesNearCamera(1);
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
                currentState = GameState::CREATIVE_INVENTORY;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            } else if (currentState == GameState::CREATIVE_INVENTORY) {
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

    const bool inventoryOpen = (currentState == GameState::CREATIVE_INVENTORY);
    if (currentState != GameState::IN_GAME && !inventoryOpen) return;
    if (!movementEnabled) return;

    static bool pWasPressed = false;
    bool pPressed = glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS;
    if (pPressed && !pWasPressed) {
        isRaining = !isRaining;
    }
    pWasPressed = pPressed;

    // Определение состояния воды
    glm::vec3 feetPosCheck = cameraPos - glm::vec3(0.0f, EYE_HEIGHT, 0.0f);
    
    bool headInWater = getBlockAt(floor(cameraPos.x), floor(cameraPos.y), floor(cameraPos.z)) == 5;
    bool chestInWater = getBlockAt(floor(cameraPos.x), floor(cameraPos.y - 0.6f), floor(cameraPos.z)) == 5;
    bool waistInWater = getBlockAt(floor(cameraPos.x), floor(cameraPos.y - 1.1f), floor(cameraPos.z)) == 5;
    bool feetInWater = getBlockAt(floor(feetPosCheck.x), floor(feetPosCheck.y + 0.1f), floor(feetPosCheck.z)) == 5;
    
    bool inWater = feetInWater || waistInWater || chestInWater || headInWater;
    bool fullySubmerged = headInWater && chestInWater;
    bool atWaterSurface = (waistInWater || chestInWater) && !headInWater;
    bool touchingWater = feetInWater && !waistInWater;
    
    static bool wasInWaterLastFrame = false;

    // Параметры воды
    const float WATER_DRAG = 0.8f;           // Сопротивление воды горизонтальное
    const float WATER_GRAVITY = -8.0f;       // Гравитация в воде (слабее)
    const float MAX_WATER_SPEED_UP = 1.5f;   // Макс. скорость всплытия
    const float MAX_WATER_SPEED_DOWN = -1.5f; // Макс. скорость погружения
    
    // Горизонтальное движение
    glm::vec3 moveDir(0.0f);
    if (!inventoryOpen) {
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) moveDir += cameraFront;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) moveDir -= cameraFront;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) moveDir -= glm::normalize(glm::cross(cameraFront, cameraUp));
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) moveDir += glm::normalize(glm::cross(cameraFront, cameraUp));
    }
    
    bool moving = glm::length(moveDir) > 0.1f;
    if (moving) moveDir = glm::normalize(moveDir);
    
    // Базовая скорость ходьбы
    float walkSpeed = WALK_SPEED;
    
    // В воде скорость снижена
    if (inWater) {
        walkSpeed = WALK_SPEED * 0.4f;  // Скорость в воде ~40%
    } else if (touchingWater) {
        walkSpeed = WALK_SPEED * 0.7f;  // Касание воды
    }
    
    glm::vec3 desiredMove = moveDir * walkSpeed * deltaTime;
    
    // Прыжок/всплытие/ныряние
    const bool wantsJump = !inventoryOpen && glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
    const bool jumpPressedThisFrame = wantsJump && !jumpKeyWasPressed;
    jumpKeyWasPressed = wantsJump;

    if (wantsJump && isOnGround) {
        heldJumpRepeatTimer = std::max(0.0f, heldJumpRepeatTimer - deltaTime);
    } else if (!wantsJump) {
        heldJumpRepeatTimer = 0.0f;
    }

    const bool wantsDive = !inventoryOpen && glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;
    
    // =========================================================
    // ВЕРТИКАЛЬНАЯ ФИЗИКА (ВОДА) - БЕЗ АВТОМАТИЧЕСКОГО ВСПЛЫТИЯ
    // =========================================================
    if (inWater) {
        // Применяем сопротивление воды к горизонтальному движению
        desiredMove *= 0.6f;
        
        if (fullySubmerged) {
            // Полностью под водой
            if (wantsJump) {
                // Всплытие при нажатии Space
                playerVelocity.y += 10.0f * deltaTime;
                if (playerVelocity.y > MAX_WATER_SPEED_UP) playerVelocity.y = MAX_WATER_SPEED_UP;
            } else if (wantsDive) {
                // Ныряние вниз при нажатии Shift
                playerVelocity.y -= 10.0f * deltaTime;
                if (playerVelocity.y < MAX_WATER_SPEED_DOWN) playerVelocity.y = MAX_WATER_SPEED_DOWN;
            } else {
                // НЕТ АВТОМАТИЧЕСКОГО ВСПЛЫТИЯ!
                // Применяем только гравитацию и сопротивление
                playerVelocity.y += WATER_GRAVITY * deltaTime;
                playerVelocity.y *= (1.0f - WATER_DRAG * deltaTime);
            }
            
        } else if (atWaterSurface) {
            // На поверхности воды (голова над водой)
            if (wantsJump) {
                // Прыжок на поверхности (как ластами)
                if (playerVelocity.y <= 0.1f) {
                    playerVelocity.y = JUMP_POWER * 0.6f;
                } else {
                    playerVelocity.y += 8.0f * deltaTime;
                }
                if (playerVelocity.y > MAX_WATER_SPEED_UP) playerVelocity.y = MAX_WATER_SPEED_UP;
            } else if (wantsDive) {
                // Ныряние под воду
                playerVelocity.y -= 8.0f * deltaTime;
                if (playerVelocity.y < MAX_WATER_SPEED_DOWN) playerVelocity.y = MAX_WATER_SPEED_DOWN;
            } else {
                // На поверхности без действий - тонем или стоим на месте
                // Применяем слабую гравитацию
                playerVelocity.y += WATER_GRAVITY * 0.5f * deltaTime;
                playerVelocity.y *= (1.0f - WATER_DRAG * deltaTime);
                
                // Ограничиваем, чтобы не уйти слишком глубоко
                if (playerVelocity.y < -0.8f) playerVelocity.y = -0.8f;
            }
            
        } else if (waistInWater || chestInWater) {
            // Частично в воде (пояс или грудь)
            if (wantsJump) {
                // Всплытие при прыжке
                playerVelocity.y += 7.0f * deltaTime;
                if (playerVelocity.y > MAX_WATER_SPEED_UP) playerVelocity.y = MAX_WATER_SPEED_UP;
            } else if (wantsDive && !isOnGround) {
                // Ныряние
                playerVelocity.y -= 7.0f * deltaTime;
                if (playerVelocity.y < MAX_WATER_SPEED_DOWN) playerVelocity.y = MAX_WATER_SPEED_DOWN;
            } else {
                // НЕТ АВТОМАТИЧЕСКОГО ВСПЛЫТИЯ
                // Применяем только гравитацию
                playerVelocity.y += WATER_GRAVITY * 0.7f * deltaTime;
                
                // Ограничиваем скорость падения в воде
                if (playerVelocity.y < MAX_WATER_SPEED_DOWN) playerVelocity.y = MAX_WATER_SPEED_DOWN;
            }
        }
        
        // Ограничение вертикальной скорости в воде
        playerVelocity.y = glm::clamp(playerVelocity.y, MAX_WATER_SPEED_DOWN, MAX_WATER_SPEED_UP);
        
        // В воде нет урона от падения
        fallDistance = 0.0f;
        
    } else {
        // =========================================================
        // ВЕРТИКАЛЬНАЯ ФИЗИКА (НА СУШЕ)
        // =========================================================
        
        const bool canStartJump = jumpPressedThisFrame || heldJumpRepeatTimer <= 0.0f;
        if (wantsJump && canStartJump && isOnGround) {
            playerVelocity.y = JUMP_POWER;
            isOnGround = false;
            heldJumpRepeatTimer = HELD_JUMP_REPEAT_DELAY;
            soundManager.playPlayerSound("step", gameStarted);
        }
        
        playerVelocity.y += GRAVITY * deltaTime;
        
        // Терминальная скорость падения
        const float MAX_FALL_SPEED = -54.0f;
        if (playerVelocity.y < MAX_FALL_SPEED) playerVelocity.y = MAX_FALL_SPEED;
    }
    
    // =========================================================
    // ПРИМЕНЕНИЕ КОЛЛИЗИЙ
    // =========================================================
    glm::vec3 delta = desiredMove;
    delta.y = playerVelocity.y * deltaTime;
    
    glm::vec3 feetPos = cameraPos - glm::vec3(0.0f, EYE_HEIGHT, 0.0f);
    glm::vec3 actualDelta = applyCollision(feetPos, delta);

    // Сохраняем горизонтальную скорость
    if (deltaTime > 0.00001f) {
        playerVelocity.x = actualDelta.x / deltaTime;
        playerVelocity.z = actualDelta.z / deltaTime;
    } else {
        playerVelocity.x = 0.0f;
        playerVelocity.z = 0.0f;
    }

    feetPos += actualDelta;
    cameraPos = feetPos + glm::vec3(0.0f, EYE_HEIGHT, 0.0f);
    
    // Защита от падения сквозь мир
    if (feetPos.y < -10.0f) {
        feetPos.y = 0.0f;
        cameraPos = feetPos + glm::vec3(0, EYE_HEIGHT, 0);
        playerVelocity = glm::vec3(0.0f);
        isOnGround = true;
        applyDamage(10);
    }

    // =========================================================
    // ЗВУКИ ШАГОВ
    // =========================================================
    static bool wasStepSoundActive = false;
    static float stepGroundGraceTimer = 0.0f;

    if (isOnGround) {
        stepGroundGraceTimer = STEP_GROUND_GRACE_TIME;
    } else {
        stepGroundGraceTimer = std::max(0.0f, stepGroundGraceTimer - deltaTime);
    }

    bool hasHorizontalStepMovement = glm::length(glm::vec2(actualDelta.x, actualDelta.z)) > 0.0001f;
    bool canPlayStepSound = (isOnGround || stepGroundGraceTimer > 0.0f) && moving && hasHorizontalStepMovement && !inWater;

    if (canPlayStepSound) {
        if (!wasStepSoundActive) {
            stepSoundTimer = STEP_SOUND_INTERVAL;
        }

        stepSoundTimer += deltaTime;
        if (stepSoundTimer >= STEP_SOUND_INTERVAL) {
            stepSoundTimer -= STEP_SOUND_INTERVAL;
            int blockBelow = getBlockUnderCamera(cameraPos, feetPos);
            if (blockBelow != 0 && blockBelow != 5) {
                soundManager.play(blockBelow, "step", gameStarted);
            }
        }
    } else if (!moving || inWater) {
        stepSoundTimer = 0.0f;
    }

    wasStepSoundActive = canPlayStepSound;
    
    // Звуки входа/выхода из воды
    static bool wasInWaterSound = false;
    if (inWater && !wasInWaterSound) {
        soundManager.play(5, "step", gameStarted);
    } else if (!inWater && wasInWaterSound) {
        soundManager.play(5, "step", gameStarted);
    }
    wasInWaterSound = inWater;
    wasInWaterLastFrame = inWater;

    // =========================================================
    // ВЫБОР СЛОТА ХОТБАРА
    // =========================================================
    if (!inventoryOpen) {
        for (int i = 0; i < 9; ++i) {
            if (glfwGetKey(window, GLFW_KEY_1 + i) == GLFW_PRESS) {
                currentHotbarSlot = i;
                int blockFromHotbar = playerInventoryItems[27 + currentHotbarSlot].blockType;
                if (blockFromHotbar != 0) {
                    currentBlockType = blockFromHotbar;
                }
            }
        }
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
    underwaterOverlayTexture = loadTextureStrip("textures/environment/underwater.png", true);
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
    if (underwaterOverlayTexture) {
        glBindTexture(GL_TEXTURE_2D, underwaterOverlayTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    glGenVertexArrays(1, &playerVAO);
    glGenBuffers(1, &playerVBO);
}

void renderPlayerModel(const glm::vec3& feetPos, const glm::vec3& lookDir, float currentTime) {
    (void)currentTime;
    glUniform1i(u_useBiomeTint_location, 0);
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
    glUniform1i(u_isRain_location, 0);
    renderSkyHorizonGlow(sunDir, view, proj);
    renderStars(currentTime, sunDir, view, proj);
    glUseProgram(shaderProgram);
    renderCelestialBodies(currentTime, sunDir);
    glUniform3fv(u_sunDir_location, 1, glm::value_ptr(sunDir));
    glUniform1f(u_sunIntensity_location, sunIntensity);
    glUniform1f(u_ambientBase_location, ambientBase);
    renderCloudLayer(currentTime);
    renderRainLayer(currentTime);

    // Рендер всех чанков
    for (auto& p : loadedChunks)
        p.second.render();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-1.0f, -1.0f);
    for (auto& p : loadedChunks)
        p.second.renderGrassOverlay();
    glDisable(GL_POLYGON_OFFSET_FILL);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    // Листва и другие code-alpha блоки — cutout: рисуем с записью в depth-buffer.
    // Так вода/дальние блоки не просвечивают и не накладываются поверх ближней листвы.
    updateAlphaChunksCache();
    for (Chunk* ch : alphaChunksCache)
        ch->renderAlphaBlocks(true);

    if (glm::distance(safeRenderCameraPos, lastCameraPosForAlphaSort) > 0.75f) {
        std::sort(alphaChunksCache.begin(), alphaChunksCache.end(), [&](Chunk* a, Chunk* b) {
            glm::vec3 ca(a->pos.x * CHUNK_SIZE_X + CHUNK_SIZE_X / 2, 30, a->pos.y * CHUNK_SIZE_Z + CHUNK_SIZE_Z / 2);
            glm::vec3 cb(b->pos.x * CHUNK_SIZE_X + CHUNK_SIZE_X / 2, 30, b->pos.y * CHUNK_SIZE_Z + CHUNK_SIZE_Z / 2);
            return glm::distance(safeRenderCameraPos, ca) > glm::distance(safeRenderCameraPos, cb);
        });
        lastCameraPosForAlphaSort = safeRenderCameraPos;
    }
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    for (Chunk* ch : alphaChunksCache)
        ch->renderAlphaBlocks(false);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

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

    if (isCameraUnderwater()) {
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        renderUnderwaterOverlay(screenW, screenH, currentTime);
        drawDimOverlay(screenW, screenH, 0.16f);
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
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
        -halfSize, y, -halfSize,   0.0f,     0.0f,    0.0f, -1.0f, 0.0f, 0.92f, 0.0f,

        -halfSize, y, -halfSize,   0.0f,     0.0f,    0.0f,  1.0f, 0.0f, 0.92f, 0.0f,
        -halfSize, y,  halfSize,   0.0f,     uvScale, 0.0f,  1.0f, 0.0f, 0.92f, 0.0f,
         halfSize, y,  halfSize,   uvScale,  uvScale, 0.0f,  1.0f, 0.0f, 0.92f, 0.0f,
         halfSize, y,  halfSize,   uvScale,  uvScale, 0.0f,  1.0f, 0.0f, 0.92f, 0.0f,
         halfSize, y, -halfSize,   uvScale,  0.0f,    0.0f,  1.0f, 0.0f, 0.92f, 0.0f,
        -halfSize, y, -halfSize,   0.0f,     0.0f,    0.0f,  1.0f, 0.0f, 0.92f, 0.0f
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

void initCelestialBodies() {
    sunTexture = loadUITexture("textures/environment/sun.png");
    moonPhasesTexture = loadUITexture("textures/environment/moon_phases.png");

    glGenVertexArrays(1, &celestialVAO);
    glGenBuffers(1, &celestialVBO);
    glBindVertexArray(celestialVAO);
    glBindBuffer(GL_ARRAY_BUFFER, celestialVBO);
    glBufferData(GL_ARRAY_BUFFER, 6 * 10 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 10 * sizeof(float), (void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 10 * sizeof(float), (void*)(3 * sizeof(float))); glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 10 * sizeof(float), (void*)(5 * sizeof(float))); glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, 10 * sizeof(float), (void*)(8 * sizeof(float))); glEnableVertexAttribArray(3);
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, 10 * sizeof(float), (void*)(9 * sizeof(float))); glEnableVertexAttribArray(4);
    glBindVertexArray(0);
}


const char* skyGlowVertexShaderSrc = R"(#version 330 core
layout (location = 0) in vec3 aDirection;

uniform mat4 view;
uniform mat4 projection;
uniform vec3 u_cameraPos;
uniform float u_pointSize;

void main() {
    vec3 dir = normalize(aDirection);
    vec3 worldPos = u_cameraPos + dir * 700.0;
    gl_Position = projection * view * vec4(worldPos, 1.0);
    gl_PointSize = u_pointSize;
}
)";

const char* skyGlowFragmentShaderSrc = R"(#version 330 core
uniform vec3 u_glowColor;
uniform float u_alpha;

out vec4 FragColor;

void main() {
    vec2 centered = gl_PointCoord - vec2(0.5);
    vec2 stretched = vec2(centered.x * 0.80, centered.y * 1.80);
    float dist = length(stretched);
    if (dist > 0.5 || u_alpha <= 0.002) discard;

    float core = 1.0 - smoothstep(0.00, 0.16, dist);
    float halo = 1.0 - smoothstep(0.08, 0.50, dist);
    float alpha = u_alpha * (core * 0.28 + halo * 0.72);
    FragColor = vec4(u_glowColor, alpha);
}
)";

void initSkyHorizonGlow() {
    unsigned int vs = glCreateShader(GL_VERTEX_SHADER), fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(vs, 1, &skyGlowVertexShaderSrc, NULL); glCompileShader(vs); checkShaderErrors(vs, "sky glow vertex");
    glShaderSource(fs, 1, &skyGlowFragmentShaderSrc, NULL); glCompileShader(fs); checkShaderErrors(fs, "sky glow fragment");
    skyGlowProgram = glCreateProgram();
    glAttachShader(skyGlowProgram, vs);
    glAttachShader(skyGlowProgram, fs);
    glLinkProgram(skyGlowProgram); checkProgramErrors(skyGlowProgram);
    glDeleteShader(vs); glDeleteShader(fs);

    glGenVertexArrays(1, &skyGlowVAO);
    glGenBuffers(1, &skyGlowVBO);
    glBindVertexArray(skyGlowVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyGlowVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}


const char* starVertexShaderSrc = R"(#version 330 core
layout (location = 0) in vec3 aDirection;
layout (location = 1) in float aBrightness;
layout (location = 2) in float aSize;
layout (location = 3) in float aTwinkle;

uniform mat4 view;
uniform mat4 projection;
uniform vec3 u_cameraPos;
uniform mat3 u_starRotation;
uniform float u_nightAlpha;

out float vAlpha;
out vec3 vColor;

void main() {
    vec3 dir = normalize(u_starRotation * aDirection);
    float horizonFade = smoothstep(0.015, 0.18, dir.y);
    float twinkle = 0.86 + 0.14 * aTwinkle;
    vAlpha = u_nightAlpha * horizonFade * aBrightness * twinkle;
    vColor = mix(vec3(0.76, 0.84, 1.0), vec3(1.0, 0.96, 0.84), aBrightness);

    vec3 worldPos = u_cameraPos + dir * 720.0;
    gl_Position = projection * view * vec4(worldPos, 1.0);
    gl_PointSize = aSize * (0.88 + 0.12 * aTwinkle);
}
)";

const char* starFragmentShaderSrc = R"(#version 330 core
in float vAlpha;
in vec3 vColor;
out vec4 FragColor;

void main() {
    vec2 centered = gl_PointCoord - vec2(0.5);
    float dist = length(centered);
    if (dist > 0.5 || vAlpha <= 0.002) discard;
    float softCircle = 1.0 - smoothstep(0.18, 0.5, dist);
    FragColor = vec4(vColor, vAlpha * softCircle);
}
)";

void initStarField() {
    starDistributionNoise.SetSeed(424242);
    starDistributionNoise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    starDistributionNoise.SetFrequency(0.72f);
    starTwinkleNoise.SetSeed(919191);
    starTwinkleNoise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    starTwinkleNoise.SetFrequency(0.55f);

    std::mt19937 rng(20260514u);
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    // Keep a dense base layer so the night sky does not get large empty gaps,
    // then let Perlin noise add brighter clusters on top of it.
    constexpr int targetStars = 1800;
    constexpr int maxAttempts = targetStars * 28;
    starPoints.clear();
    starPoints.reserve(targetStars);

    for (int attempt = 0; attempt < maxAttempts && static_cast<int>(starPoints.size()) < targetStars; ++attempt) {
        const float z = unit(rng) * 2.0f - 1.0f;
        const float angle = unit(rng) * glm::two_pi<float>();
        const float radius = std::sqrt(std::max(0.0f, 1.0f - z * z));
        glm::vec3 dir(std::cos(angle) * radius, z, std::sin(angle) * radius);

        const float cloudiness = starDistributionNoise.GetNoise(dir.x * 3.1f + 17.0f,
                                                                dir.y * 3.1f - 9.0f,
                                                                dir.z * 3.1f + 4.0f);
        const float sparkle = starDistributionNoise.GetNoise(dir.x * 18.0f - 5.0f,
                                                             dir.y * 18.0f + 2.0f,
                                                             dir.z * 18.0f + 11.0f);
        const float density = cloudiness * 0.55f + sparkle * 0.25f + unit(rng) * 0.20f;
        const float baseStarChance = 0.18f;
        const bool inPerlinCluster = density > 0.12f + 0.08f * unit(rng);
        const bool inBackgroundLayer = unit(rng) < baseStarChance;
        if (!inPerlinCluster && !inBackgroundLayer) continue;

        StarPoint star;
        star.direction = glm::normalize(dir);
        const float clusterBoost = inPerlinCluster ? glm::clamp((density + 1.0f) * 0.28f, 0.0f, 0.42f) : 0.0f;
        star.brightness = glm::clamp(0.38f + clusterBoost + unit(rng) * 0.34f, 0.34f, 1.0f);
        star.size = glm::mix(1.15f, 3.1f, std::pow(star.brightness, 2.0f));
        star.twinkleSeed = unit(rng) * 1000.0f + static_cast<float>(attempt) * 0.137f;
        starPoints.push_back(star);
    }

    starVertexData.assign(starPoints.size() * 6, 0.0f);
    for (size_t i = 0; i < starPoints.size(); ++i) {
        starVertexData[i * 6 + 0] = starPoints[i].direction.x;
        starVertexData[i * 6 + 1] = starPoints[i].direction.y;
        starVertexData[i * 6 + 2] = starPoints[i].direction.z;
        starVertexData[i * 6 + 3] = starPoints[i].brightness;
        starVertexData[i * 6 + 4] = starPoints[i].size;
        starVertexData[i * 6 + 5] = 1.0f;
    }

    unsigned int vs = glCreateShader(GL_VERTEX_SHADER), fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(vs, 1, &starVertexShaderSrc, NULL); glCompileShader(vs); checkShaderErrors(vs, "star vertex");
    glShaderSource(fs, 1, &starFragmentShaderSrc, NULL); glCompileShader(fs); checkShaderErrors(fs, "star fragment");
    starProgram = glCreateProgram();
    glAttachShader(starProgram, vs); glAttachShader(starProgram, fs);
    glLinkProgram(starProgram); checkProgramErrors(starProgram);
    glDeleteShader(vs); glDeleteShader(fs);

    glGenVertexArrays(1, &starVAO);
    glGenBuffers(1, &starVBO);
    glBindVertexArray(starVAO);
    glBindBuffer(GL_ARRAY_BUFFER, starVBO);
    glBufferData(GL_ARRAY_BUFFER, starVertexData.size() * sizeof(float), starVertexData.data(), GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float))); glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(4 * sizeof(float))); glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(5 * sizeof(float))); glEnableVertexAttribArray(3);
    glBindVertexArray(0);
}

void renderStars(float currentTime, const glm::vec3& sunDir, const glm::mat4& view, const glm::mat4& proj) {
    if (!starProgram || !starVAO || starPoints.empty()) return;

    float nightAlpha = 1.0f - glm::smoothstep(-0.24f, 0.04f, sunDir.y);
    if (isRaining) nightAlpha *= 0.18f;
    if (nightAlpha <= 0.002f) return;

    for (size_t i = 0; i < starPoints.size(); ++i) {
        const float twinkleNoise = starTwinkleNoise.GetNoise(starPoints[i].twinkleSeed,
                                                             currentTime * 0.18f,
                                                             starPoints[i].brightness * 8.0f);
        starVertexData[i * 6 + 5] = glm::clamp(twinkleNoise * 0.5f + 0.5f, 0.0f, 1.0f);
    }
    glBindBuffer(GL_ARRAY_BUFFER, starVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, starVertexData.size() * sizeof(float), starVertexData.data());

    const float angle = std::atan2(sunDir.y, sunDir.x) + glm::half_pi<float>();
    const float c = std::cos(angle), s = std::sin(angle);
    const glm::mat3 starRotation(
        c,  s, 0.0f,
       -s,  c, 0.0f,
        0.0f, 0.0f, 1.0f
    );

    GLboolean depthEnabled = glIsEnabled(GL_DEPTH_TEST);
    GLboolean cullEnabled = glIsEnabled(GL_CULL_FACE);
    GLboolean blendEnabled = glIsEnabled(GL_BLEND);
    GLboolean previousDepthMask = GL_TRUE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &previousDepthMask);

    if (depthEnabled) glDisable(GL_DEPTH_TEST);
    if (cullEnabled) glDisable(GL_CULL_FACE);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glEnable(GL_PROGRAM_POINT_SIZE);

    glUseProgram(starProgram);
    glUniformMatrix4fv(glGetUniformLocation(starProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(starProgram, "projection"), 1, GL_FALSE, glm::value_ptr(proj));
    glUniform3fv(glGetUniformLocation(starProgram, "u_cameraPos"), 1, glm::value_ptr(renderCameraPos));
    glUniformMatrix3fv(glGetUniformLocation(starProgram, "u_starRotation"), 1, GL_FALSE, glm::value_ptr(starRotation));
    glUniform1f(glGetUniformLocation(starProgram, "u_nightAlpha"), nightAlpha);

    glBindVertexArray(starVAO);
    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(starPoints.size()));
    glBindVertexArray(0);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    if (!blendEnabled) glDisable(GL_BLEND);
    glDepthMask(previousDepthMask);
    if (depthEnabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (cullEnabled) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
}

void renderSkyHorizonGlow(const glm::vec3& sunDir, const glm::mat4& view, const glm::mat4& proj) {
    if (!skyGlowProgram || !skyGlowVAO || !skyGlowVBO) return;

    glm::vec3 dir = sunDir;
    if (!std::isfinite(dir.x) || !std::isfinite(dir.y) || !std::isfinite(dir.z) || glm::length(dir) < 0.001f) return;
    dir = glm::normalize(dir);

    const float horizonGlow = 1.0f - glm::smoothstep(0.035f, 0.34f, std::abs(dir.y));
    if (horizonGlow <= 0.002f) return;

    const float aboveHorizon = glm::smoothstep(-0.10f, 0.13f, dir.y);
    const float twilight = (1.0f - glm::smoothstep(0.00f, 0.22f, std::abs(dir.y))) * (1.0f - aboveHorizon);
    const glm::vec3 sunriseColor(0.98f, 0.54f, 0.30f);
    const glm::vec3 sunsetColor(1.00f, 0.33f, 0.13f);
    const glm::vec3 violetColor(0.28f, 0.16f, 0.38f);
    glm::vec3 glowColor = glm::mix(sunsetColor, sunriseColor, glm::smoothstep(-0.25f, 0.25f, dir.x));
    glowColor = glm::mix(glowColor, violetColor, twilight * 0.42f);

    float alpha = horizonGlow * (0.40f + 0.42f * aboveHorizon);
    if (isRaining) alpha *= 0.28f;

    GLboolean depthEnabled = glIsEnabled(GL_DEPTH_TEST);
    GLboolean cullEnabled = glIsEnabled(GL_CULL_FACE);
    GLboolean blendEnabled = glIsEnabled(GL_BLEND);
    GLboolean previousDepthMask = GL_TRUE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &previousDepthMask);

    if (depthEnabled) glDisable(GL_DEPTH_TEST);
    if (cullEnabled) glDisable(GL_CULL_FACE);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_PROGRAM_POINT_SIZE);

    glUseProgram(skyGlowProgram);
    glUniformMatrix4fv(glGetUniformLocation(skyGlowProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(skyGlowProgram, "projection"), 1, GL_FALSE, glm::value_ptr(proj));
    glUniform3fv(glGetUniformLocation(skyGlowProgram, "u_cameraPos"), 1, glm::value_ptr(renderCameraPos));
    glUniform3fv(glGetUniformLocation(skyGlowProgram, "u_glowColor"), 1, glm::value_ptr(glowColor));
    glUniform1f(glGetUniformLocation(skyGlowProgram, "u_alpha"), alpha);
    glUniform1f(glGetUniformLocation(skyGlowProgram, "u_pointSize"), 1100.0f);

    glBindVertexArray(skyGlowVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyGlowVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(glm::vec3), glm::value_ptr(dir));
    glDrawArrays(GL_POINTS, 0, 1);
    glBindVertexArray(0);

    glUseProgram(shaderProgram);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    if (!blendEnabled) glDisable(GL_BLEND);
    glDepthMask(previousDepthMask);
    if (depthEnabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (cullEnabled) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
}

void renderCelestialBillboard(const glm::vec3& direction, float size, unsigned int texture,
                              float u0, float v0, float u1, float v1) {
    if (!celestialVAO || !celestialVBO || !texture) return;

    glm::vec3 dir = direction;
    if (!std::isfinite(dir.x) || !std::isfinite(dir.y) || !std::isfinite(dir.z) || glm::length(dir) < 0.001f) return;
    dir = glm::normalize(dir);

    // Keep one billboard axis fixed in world space so the sun/moon texture does not roll
    // while it moves along the sky arc. The day/night direction is constrained to the X/Y
    // plane, so world Z is always perpendicular to it and keeps the quad centered overhead.
    const glm::vec3 right(0.0f, 0.0f, 1.0f);
    glm::vec3 up = glm::normalize(glm::cross(right, dir));

    const float distance = 640.0f;
    const glm::vec3 center = renderCameraPos + dir * distance;
    const glm::vec3 r = right * (size * 0.5f);
    const glm::vec3 u = up * (size * 0.5f);
    const glm::vec3 normal(0.0f, 1.0f, 0.0f);

    auto pushVertex = [](std::vector<float>& verts, const glm::vec3& pos, float uCoord, float vCoord, const glm::vec3& n) {
        verts.push_back(pos.x); verts.push_back(pos.y); verts.push_back(pos.z);
        verts.push_back(uCoord); verts.push_back(vCoord);
        verts.push_back(n.x); verts.push_back(n.y); verts.push_back(n.z);
        verts.push_back(1.0f); // full sky light: the sun/moon texture is not darkened by world lighting
        verts.push_back(0.0f);
    };

    std::vector<float> verts;
    verts.reserve(6 * 10);
    glm::vec3 a = center - r + u;
    glm::vec3 b = center + r + u;
    glm::vec3 c = center + r - u;
    glm::vec3 d = center - r - u;
    pushVertex(verts, a, u0, v0, normal);
    pushVertex(verts, b, u1, v0, normal);
    pushVertex(verts, c, u1, v1, normal);
    pushVertex(verts, c, u1, v1, normal);
    pushVertex(verts, d, u0, v1, normal);
    pushVertex(verts, a, u0, v0, normal);

    glBindTexture(GL_TEXTURE_2D, texture);
    glBindVertexArray(celestialVAO);
    glBindBuffer(GL_ARRAY_BUFFER, celestialVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, verts.size() * sizeof(float), verts.data());
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void updateMoonPhaseForCycle(const glm::vec3& moonDir) {
    const bool moonBelowWorld = moonDir.y < -0.02f;
    if (moonBelowWorld && !moonWasBelowWorld) {
        currentMoonPhase = (currentMoonPhase + 1) % 8;
    }
    moonWasBelowWorld = moonBelowWorld;
}

void renderCelestialBodies(float currentTime, const glm::vec3& sunDir) {
    (void)currentTime;
    glUniform1i(u_useBiomeTint_location, 0);
    if (!celestialVAO || (!sunTexture && !moonPhasesTexture)) return;

    glm::vec3 moonDir = -sunDir;
    updateMoonPhaseForCycle(moonDir);

    GLboolean depthEnabled = glIsEnabled(GL_DEPTH_TEST);
    GLboolean cullEnabled = glIsEnabled(GL_CULL_FACE);
    GLboolean blendEnabled = glIsEnabled(GL_BLEND);
    GLboolean previousDepthMask = GL_TRUE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &previousDepthMask);

    if (depthEnabled) glDisable(GL_DEPTH_TEST);
    if (cullEnabled) glDisable(GL_CULL_FACE);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    // Minecraft-style sun/moon textures often keep a black RGB background instead of alpha.
    // Additive blending makes that black background invisible while preserving RGBA textures too.
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    glUniformMatrix4fv(u_modelLoc, 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));
    glUniform1i(u_isWater_location, 0);
    glUniform1i(u_isRain_location, 0);
    glUniform1i(u_useBiomeTint_location, 0);
    glUniform1f(u_sunIntensity_location, 1.0f);
    glUniform1f(u_ambientBase_location, 1.0f);
    glActiveTexture(GL_TEXTURE0);

    renderCelestialBillboard(sunDir, 128.8f, sunTexture, 0.0f, 0.0f, 1.0f, 1.0f);

    const int phase = std::clamp(currentMoonPhase, 0, 7);
    const int col = phase % 4;
    const int row = phase / 4;
    const float u0 = col * 0.25f;
    const float v0 = row * 0.5f;
    const float u1 = u0 + 0.25f;
    const float v1 = v0 + 0.5f;
    renderCelestialBillboard(moonDir, 100.8f, moonPhasesTexture, u0, v0, u1, v1);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    if (!blendEnabled) glDisable(GL_BLEND);
    glDepthMask(previousDepthMask);
    if (depthEnabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (cullEnabled) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
}

void renderCloudLayer(float currentTime) {
    if (!cloudVAO || !cloudTexture) return;
    glUniform1i(u_useBiomeTint_location, 0);
    const float cloudSpeed = 0.45f;
    glm::mat4 cloudModel = glm::translate(glm::mat4(1.0f), glm::vec3(currentTime * cloudSpeed, 0.0f, 0.0f));
    glUniformMatrix4fv(u_modelLoc, 1, GL_FALSE, glm::value_ptr(cloudModel));
    glUniform1i(u_isWater_location, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, cloudTexture);
    glBindVertexArray(cloudVAO);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDrawArrays(GL_TRIANGLES, 0, 12);
    glDisable(GL_BLEND);
    glBindVertexArray(0);
    glUniformMatrix4fv(u_modelLoc, 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));
}

void renderRainLayer(float currentTime) {
    if (!isRaining || !rainTexture || !rainVAO || !rainVBO) return;
    glUniform1i(u_useBiomeTint_location, 0);

    const float rainRadius = 30.0f;
    const float gridSize = 1.0f;
    const float dropWidth = 0.032f;
    const float dropLength = 1.0f;
    const float rainTopOffset = rainRadius;
    const float rainBottomOffset = -rainRadius;
    const float fallSpeed = 15.0f;
    const float windX = 0.18f;
    const float windZ = -0.09f;

    glm::vec3 playerRainCenter = cameraPos;
    if (!std::isfinite(playerRainCenter.x) || !std::isfinite(playerRainCenter.y) || !std::isfinite(playerRainCenter.z)) {
        playerRainCenter = renderCameraPos;
    } else {
        playerRainCenter.y = cameraPos.y - EYE_HEIGHT + PLAYER_HEIGHT * 0.5f;
    }

    glm::vec3 rainAnchorPos = playerRainCenter;
    if (cameraMode == CameraMode::ThirdPersonBack &&
        std::isfinite(renderCameraPos.x) && std::isfinite(renderCameraPos.y) && std::isfinite(renderCameraPos.z)) {
        // В третьем лице видимый центр дождя воспринимается между камерой и игроком.
        // Поэтому не привязываем объём полностью ни к вынесенной камере, ни к модели:
        // середина отрезка удерживает игрока внутри центральной плотной зоны, а не на её краю.
        rainAnchorPos.x = (playerRainCenter.x + renderCameraPos.x) * 0.5f;
        rainAnchorPos.z = (playerRainCenter.z + renderCameraPos.z) * 0.5f;
    }

    // Центрируем область без округления к мировой сетке, чтобы радиус дождя не прыгал
    // и не выталкивал игрока к краю при дробных координатах.
    const float baseX = rainAnchorPos.x;
    const float baseZ = rainAnchorPos.z;
    const int cellRadius = static_cast<int>(std::ceil(rainRadius / gridSize));

    std::vector<float> v;
    v.reserve(4096 * 6 * 10);

    const float rainTop = rainAnchorPos.y + rainTopOffset;
    const float rainBottom = rainAnchorPos.y + rainBottomOffset;
    const float heightRange = std::max(1.0f, rainTop - rainBottom);

    auto appendDropQuad = [&](const glm::vec3& center, float length, float width, float alpha) {
        glm::vec3 halfX(width * 0.5f, 0.0f, 0.0f);
        glm::vec3 top(0.0f, length * 0.5f, 0.0f);
        glm::vec3 bottom(0.0f, -length * 0.5f, 0.0f);
        glm::vec3 a = center - halfX + top;
        glm::vec3 b = center + halfX + top;
        glm::vec3 c = center + halfX + bottom;
        glm::vec3 d = center - halfX + bottom;
        float quad[] = {
            a.x,a.y,a.z,0,0,0,0,1,1,alpha, b.x,b.y,b.z,1,0,0,0,1,1,alpha, c.x,c.y,c.z,1,1,0,0,1,1,alpha,
            c.x,c.y,c.z,1,1,0,0,1,1,alpha, d.x,d.y,d.z,0,1,0,0,1,1,alpha, a.x,a.y,a.z,0,0,0,0,1,1,alpha
        };
        v.insert(v.end(), std::begin(quad), std::end(quad));
    };

    for (int gx = -cellRadius; gx <= cellRadius; ++gx) {
        for (int gz = -cellRadius; gz <= cellRadius; ++gz) {
            float worldX = baseX + gx * gridSize;
            float worldZ = baseZ + gz * gridSize;
            float dx = worldX - rainAnchorPos.x;
            float dz = worldZ - rainAnchorPos.z;
            if ((dx * dx + dz * dz) > (rainRadius * rainRadius)) continue;

            float playerDx = worldX - playerRainCenter.x;
            float playerDz = worldZ - playerRainCenter.z;
            float playerDist2 = playerDx * playerDx + playerDz * playerDz;

            const int ix = static_cast<int>(std::floor(worldX));
            const int iz = static_cast<int>(std::floor(worldZ));
            uint32_t h = static_cast<uint32_t>((ix * 73856093) ^ (iz * 19349663));
            bool forceInnerRain = playerDist2 < 100.0f; // Гарантируем дождь в зоне игрока (~10 блоков).
            float densityBias = 68.0f;
            if (playerDist2 < 64.0f) densityBias = 92.0f; // Гуще в радиусе ~8 блоков от игрока.
            if (!forceInnerRain && (h % 100) > densityBias) continue;

            int dropCount = (playerDist2 < 16.0f) ? 2 : 1; // Доп. капля совсем рядом с игроком.
            for (int i = 0; i < dropCount; ++i) {
                uint32_t hi = h ^ static_cast<uint32_t>(i * 0x9e3779b9u);
                float jitterX = ((hi & 0x3FF) / 1023.0f - 0.5f) * gridSize;
                float jitterZ = (((hi >> 10) & 0x3FF) / 1023.0f - 0.5f) * gridSize;
                float phase = ((hi >> 20) & 0xFFF) / 4095.0f;
                float baseFall = std::fmod(currentTime * fallSpeed + phase * heightRange, heightRange);

                float width = dropWidth + (((hi >> 5) & 0xFF) / 255.0f) * 0.018f;
                float length = dropLength + (((hi >> 13) & 0xFF) / 255.0f) * 0.55f;
                float alpha = 0.34f + (((hi >> 23) & 0xFF) / 255.0f) * 0.20f;

                const int streamCount = 4;
                const float streamSpacing = heightRange / static_cast<float>(streamCount);
                for (int stream = 0; stream < streamCount; ++stream) {
                    float layeredFall = std::fmod(baseFall + stream * streamSpacing, heightRange);
                    float fallProgress = layeredFall / heightRange;
                    float y = rainTop - layeredFall;

                    glm::vec3 center(worldX + jitterX, y, worldZ + jitterZ);
                    // Компенсируем среднее смещение ветра: капли всё ещё наклоняются по падению,
                    // но вся область дождя остаётся визуально центрированной вокруг игрока.
                    center.x += windX * (fallProgress - 0.5f);
                    center.z += windZ * (fallProgress - 0.5f);

                    if (center.y + length * 0.5f < rainBottom) continue;
                    appendDropQuad(center, length, width, alpha);
                }
            }
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

    glUniform1i(u_isRain_location, 1);
    glBindTexture(GL_TEXTURE_2D, rainTexture);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(v.size()/10));
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glUniform1i(u_isRain_location, 0);
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

