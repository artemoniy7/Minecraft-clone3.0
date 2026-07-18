// UI, меню, тексты, слайдеры, инвентарь и HUD.
// Этот файл подключается из main2.cpp и рассчитан на сборку в одном translation unit.

struct Button {
    float relX = 0.0f, relY = 0.0f, relW = 0.0f, relH = 0.0f;
    float absX = 0.0f, absY = 0.0f, absW = 0.0f, absH = 0.0f;
    bool clicked = false;
    const char* label = "";

    void updateAbsolute(int screenW, int screenH) {
        absW = relW * screenW;
        absH = relH * screenH;
        absX = relX * screenW - absW * 0.5f;
        absY = relY * screenH - absH * 0.5f;
    }

    void setAbsolute(float x, float y, float w, float h, const char* text = nullptr) {
        absX = x; absY = y; absW = w; absH = h;
        if (text) label = text;
    }
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
bool deleteSelectedWorld() {
    if (worldListState.selectedIndex < 0 ||
        worldListState.selectedIndex >= static_cast<int>(availableWorlds.size())) {
        return false;
    }

    const std::string folderName = availableWorlds[worldListState.selectedIndex].folderName;
    fs::path worldPath = getWorldPath(folderName);
    if (worldPath.empty() || !fs::exists(worldPath)) return false;

    std::error_code ec;
    fs::remove_all(worldPath, ec);
    if (ec) return false;

    if (currentWorldFolderName == folderName) {
        currentWorldFolderName.clear();
        currentWorldDisplayName.clear();
        currentWorldSeed = 0;
    }
    if (selectedWorldFolderName == folderName) {
        selectedWorldFolderName.clear();
    }

    refreshWorldMenuEntries();
    return true;
}

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
Button deleteConfirmButtons[DELETE_CONFIRM_BUTTON_COUNT] = {
    {0.39f, 0.62f, 0.2025f, 0.0486f, 0,0,0,0, false, "yes"},
    {0.61f, 0.62f, 0.2025f, 0.0486f, 0,0,0,0, false, "no"}
};

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

bool loadBiomeColorMap(const char* path, BiomeColorMap& map, const glm::vec3& fallback) {
    map = BiomeColorMap{};
    map.fallback = fallback;

    int w = 0, h = 0, ch = 0;
    unsigned char* data = stbi_load(path, &w, &h, &ch, 4);
    if (!data) {
        std::cerr << "Failed to load biome colormap: " << path << std::endl;
        return false;
    }

    map.width = w;
    map.height = h;
    map.pixels.assign(data, data + static_cast<size_t>(w) * h * 4);
    stbi_image_free(data);
    return true;
}

void initBiomeColorAssets() {
    loadBiomeColorMap("textures/colormap/grass.png", grassColorMap, glm::vec3(0.68f, 0.86f, 0.42f));
    loadBiomeColorMap("textures/colormap/foliage.png", foliageColorMap, glm::vec3(0.55f, 0.78f, 0.34f));
    grassSideOverlayTexture = loadUITexture("textures/blocks/grass_t.png");
}

glm::vec3 sampleColorMap(const BiomeColorMap& map, float temperature, float humidity) {
    if (!map.loaded()) return map.fallback;

    temperature = glm::clamp(temperature, 0.0f, 1.0f);
    humidity = glm::clamp(humidity, 0.0f, 1.0f) * temperature;

    const int x = std::clamp(static_cast<int>((1.0f - temperature) * (map.width - 1)), 0, map.width - 1);
    const int y = std::clamp(static_cast<int>((1.0f - humidity) * (map.height - 1)), 0, map.height - 1);
    const size_t idx = (static_cast<size_t>(y) * map.width + x) * 4;

    return glm::vec3(
        map.pixels[idx] / 255.0f,
        map.pixels[idx + 1] / 255.0f,
        map.pixels[idx + 2] / 255.0f
    );
}

glm::vec3 getBiomeTintColor(int wx, int wz, bool foliage) {
    float temp = (biomeNoiseA.GetNoise(static_cast<float>(wx), static_cast<float>(wz)) + 1.0f) * 0.5f;
    float humid = (biomeNoiseB.GetNoise(static_cast<float>(wx), static_cast<float>(wz)) + 1.0f) * 0.5f;

    // Bias ordinary terrain toward Minecraft's warmer plains/forest range.
    // Without this, the low-frequency noise often samples cold colormap areas,
    // making regular grass look like taiga or snowy-biome grass.
    float sampleTemp = glm::clamp(temp * 0.72f + 0.24f, 0.0f, 1.0f);
    float sampleHumid = glm::clamp(humid * 0.82f + 0.10f, 0.0f, 1.0f);
    glm::vec3 tint = sampleColorMap(foliage ? foliageColorMap : grassColorMap, sampleTemp, sampleHumid);

    const glm::vec3 vanillaLikeGreen = foliage ? glm::vec3(0.58f, 0.79f, 0.34f)
                                               : glm::vec3(0.72f, 0.86f, 0.36f);
    tint = glm::mix(tint, vanillaLikeGreen, foliage ? 0.28f : 0.55f);
    tint = glm::mix(tint, glm::vec3(1.0f), foliage ? 0.06f : 0.10f);
    if (!foliage) {
        tint.b *= 0.88f;
        tint.r = std::max(tint.r, tint.g * 0.58f);
    }
    return glm::clamp(tint, glm::vec3(0.0f), glm::vec3(1.0f));
}

bool isBiomeTintedGuiBlockFace(int blockType, int faceIdx, bool& foliage) {
    foliage = (blockType == BLOCK_OAK_LEAVES);
    if (foliage) return true;

    // Match world rendering: grass uses the colormap on its top face.
    // Side grass is handled in-world by a separate overlay texture, so the
    // inventory block model only tints faces that have tintable pixels here.
    return blockType == BLOCK_GRASS && faceIdx == 3;
}

glm::vec3 getGuiBiomeTintColor(bool foliage) {
    glm::vec3 samplePos = gameStarted ? cameraPos : glm::vec3(0.0f);
    if (!std::isfinite(samplePos.x) || !std::isfinite(samplePos.z)) {
        samplePos = glm::vec3(0.0f);
    }

    return getBiomeTintColor(static_cast<int>(std::floor(samplePos.x)),
                             static_cast<int>(std::floor(samplePos.z)),
                             foliage);
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
    deleteConfirmButtons[0].label = tr("Delete", "Удалить", "削除");
    deleteConfirmButtons[1].label = tr("Cancel", "Отмена", "キャンセル");
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
    for (auto& btn : buttons) btn.updateAbsolute(screenW, screenH);
    for (auto& btn : worldButtons) btn.updateAbsolute(screenW, screenH);
    for (auto& btn : languageButtons) btn.updateAbsolute(screenW, screenH);
    pauseResumeButton.updateAbsolute(screenW, screenH);
    pauseExitButton.updateAbsolute(screenW, screenH);
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
                slotX + SLOT_SIZE - measureMinecraftTextWidth(countStr, 1.5f) - 14,
                slotY + SLOT_SIZE - 25,
                2.4f, screenW, screenH,
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
            slotX + SLOT_SIZE - measureMinecraftTextWidth(countStr, 1.5f) - 19,
            slotY + SLOT_SIZE - 25,
            2.4f, screenW, screenH,
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
