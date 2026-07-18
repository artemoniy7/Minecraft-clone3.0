// Callbacks ввода, shader sources, utility shader checks, ретикл и main().
// Этот файл подключается из main2.cpp и рассчитан на сборку в одном translation unit.

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (currentState == GameState::MAIN_MENU) {
        if (action != GLFW_PRESS) return;
        handleMainMenuClick(window, button);
    } else if (currentState == GameState::WORLD_SELECT_MENU) {
        if (action != GLFW_PRESS) return;
        handleWorldSelectMenuClick(window, button);
    } else if (currentState == GameState::DELETE_WORLD_CONFIRM_MENU) {
        if (action != GLFW_PRESS) return;
        handleDeleteWorldConfirmMenuClick(window, button);
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
layout (location = 5) in vec3 aBiomeTint;
out vec2 TexCoord; out vec3 FragPos; out vec3 Normal; out float LightLevel; out float BlockLightLevel; out vec3 BiomeTint;
uniform mat4 model; uniform mat4 view; uniform mat4 projection; uniform int u_useBiomeTint; uniform vec3 u_guiBiomeTint;
void main() {
    vec4 worldPos = model * vec4(aPos, 1.0);
    gl_Position = projection * view * worldPos;
    TexCoord = aTexCoord; FragPos = worldPos.xyz;
    Normal = mat3(transpose(inverse(model))) * aNormal;
    LightLevel = aLight;
    BlockLightLevel = aBlockLight;
    if (u_useBiomeTint == 1) {
        BiomeTint = aBiomeTint;
    } else if (u_useBiomeTint == 2) {
        BiomeTint = mix(vec3(1.0), u_guiBiomeTint, aBiomeTint);
    } else {
        BiomeTint = vec3(1.0);
    }
}
)";
const char *fragmentShaderSource = R"(
#version 330 core
in vec2 TexCoord; out vec4 FragColor;
uniform sampler2D ourTexture; uniform float u_time; uniform int u_isWater; uniform int u_isRain;
uniform vec3 u_sunDir; uniform float u_sunIntensity; uniform float u_ambientBase;
in vec3 FragPos; in vec3 Normal; in float LightLevel; in float BlockLightLevel; in vec3 BiomeTint;
void main() {
    vec2 uv = TexCoord;
    if (u_isWater == 1) {
        float frames = 32.0, speed = 0.7;
        float frame = fract(u_time * speed) * frames;
        uv.y = uv.y / frames + floor(frame) / frames;
    }
    vec4 color = texture(ourTexture, uv);
    color.rgb *= BiomeTint;
    if (u_isWater==1) {
        float depthFactor = clamp(BlockLightLevel, 0.0, 1.0);
        vec3 shallowTint = vec3(0.28, 0.52, 0.72);
        vec3 deepTint = vec3(0.005, 0.055, 0.12);
        float depthFog = pow(depthFactor, 0.42);
        color.rgb = mix(color.rgb * shallowTint, deepTint, depthFog);
        color.a = mix(0.66, 0.985, depthFog);
    }
    if (u_isRain != 1 && color.a < 0.08) discard;
    if (u_isRain==1) {
        float rainMask = max(color.r, max(color.g, color.b));
        float sourceAlpha = (color.a > 0.01) ? color.a : rainMask;
        float rainAlpha = pow(clamp(sourceAlpha, 0.0, 1.0), 1.15) * 0.62;
        if (rainAlpha < 0.035) discard;
        vec3 rainTint = vec3(0.78, 0.80, 0.82);
        color = vec4(rainTint, rainAlpha);
    }
    vec3 n = normalize(Normal);
    float vertexLight = clamp(LightLevel, 0.0, 1.0);
    float blockLightOnly = (u_isWater == 1) ? 0.0 : clamp(BlockLightLevel, 0.0, 1.0);

    // Minecraft-подобное постоянное затенение граней: без "солнца сбоку".
    float faceShade = 0.80;
    if (n.y > 0.5) faceShade = 1.0;
    else if (n.y < -0.5) faceShade = 0.50;
    else if (abs(n.x) > 0.5) faceShade = 0.60;

    float ambientFactor = mix(0.10, 1.0, pow(vertexLight, 1.15));
    float dayFactor = mix(0.55, 1.0, clamp(u_sunIntensity, 0.0, 1.0));
    float sunLighting = u_ambientBase * ambientFactor * faceShade * dayFactor;

    // Блочный свет (факелы/лампы) не должен полностью гаснуть ночью.
    // Оставляем мягкую кривую, чтобы в темноте источники света выглядели ярко,
    // а днём не пересвечивали поверхность.
    float emissiveLighting = pow(blockLightOnly, 1.35) * 0.95;
    float lighting = max(sunLighting, emissiveLighting);

    if (u_isWater==1) lighting = max(lighting, mix(0.16, 0.045, pow(clamp(BlockLightLevel, 0.0, 1.0), 0.42)));
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
    // Vertical east/west sky arc: no constant Z offset, so the sun and moon pass
    // directly over the world instead of sliding along the side of the sky.
    sunDir = glm::normalize(glm::vec3(cos(angle), sin(angle), 0.0f));
    // Keep daylight strong until the sun is actually near/below the horizon;
    // otherwise sunset gets dark while the sun is still visibly above ground.
    float daylight = glm::smoothstep(-0.18f, 0.22f, sunDir.y);
    sunInt = 0.05f + daylight * 0.95f;
    amb = 0.02f + daylight * 0.36f;

    const glm::vec3 nightSky(0.01f, 0.015f, 0.045f);
    const glm::vec3 daySky(0.53f, 0.81f, 0.92f);
    sky = glm::mix(nightSky, daySky, daylight);

    // Keep the clear color mostly neutral. Dawn/dusk warmth is rendered later
    // as a localized glow around the sun, so the moon side of the sky stays calm.
    float horizonGlow = 1.0f - glm::smoothstep(0.04f, 0.36f, std::abs(sunDir.y));
    float aboveHorizon = glm::smoothstep(-0.10f, 0.12f, sunDir.y);
    float warmthBoost = horizonGlow * (0.25f + 0.20f * aboveHorizon);
    sunInt = glm::clamp(sunInt + warmthBoost * 0.08f, 0.05f, 1.0f);
    amb = glm::clamp(amb + warmthBoost * 0.025f, 0.02f, 0.40f);
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
    u_isRain_location = glGetUniformLocation(shaderProgram,"u_isRain");
    u_sunDir_location = glGetUniformLocation(shaderProgram,"u_sunDir");
    u_sunIntensity_location = glGetUniformLocation(shaderProgram,"u_sunIntensity");
    u_ambientBase_location = glGetUniformLocation(shaderProgram,"u_ambientBase");
    u_useBiomeTint_location = glGetUniformLocation(shaderProgram,"u_useBiomeTint");
    u_guiBiomeTint_location = glGetUniformLocation(shaderProgram,"u_guiBiomeTint");
    glUniform1i(u_useBiomeTint_location, 0);
    glUniform3f(u_guiBiomeTint_location, 1.0f, 1.0f, 1.0f);
    u_modelLoc = glGetUniformLocation(shaderProgram,"model");
    u_viewLoc = glGetUniformLocation(shaderProgram,"view");
    u_projLoc = glGetUniformLocation(shaderProgram,"projection");

    if (!loadBlockConfig("blocks.json")) return -1;
    if (!loadItemConfig("items.json")) return -1;
    initBiomeColorAssets();
    initUI(); loadMenuTextures(); loadHUDTextures(); initLanguageMenu();
    loadSliderTextures();
    initFOVSlider(optionsSliders);
    initCloudLayer();
    initCelestialBodies();
    initSkyHorizonGlow();
    initStarField();
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
            case GameState::DELETE_WORLD_CONFIRM_MENU:
                renderDeleteWorldConfirmMenu(screenW, screenH);
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
    if (sunTexture) glDeleteTextures(1, &sunTexture);
    if (moonPhasesTexture) glDeleteTextures(1, &moonPhasesTexture);
    if (skyGlowVBO) glDeleteBuffers(1, &skyGlowVBO);
    if (skyGlowVAO) glDeleteVertexArrays(1, &skyGlowVAO);
    if (skyGlowProgram) glDeleteProgram(skyGlowProgram);
    if (starVBO) glDeleteBuffers(1, &starVBO);
    if (starVAO) glDeleteVertexArrays(1, &starVAO);
    if (starProgram) glDeleteProgram(starProgram);
    if (grassSideOverlayTexture) glDeleteTextures(1, &grassSideOverlayTexture);
    if (rainVAO) glDeleteVertexArrays(1, &rainVAO);
    if (rainVBO) glDeleteBuffers(1, &rainVBO);
    if (celestialVAO) glDeleteVertexArrays(1, &celestialVAO);
    if (celestialVBO) glDeleteBuffers(1, &celestialVBO);
    glDeleteProgram(shaderProgram);
    glDeleteVertexArrays(1, &reticleVAO);
    glDeleteProgram(reticleProgram);
    glfwTerminate();
    return 0;
}
