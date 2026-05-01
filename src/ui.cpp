#include "ui.h"
#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// Статические переменные для UI-рендеринга (шейдер, VAO, VBO, EBO)
static unsigned int uiShader = 0;
static unsigned int uiVAO = 0, uiVBO = 0, uiEBO = 0;

void initUIRendering() {
    // Создайте шейдер (ваш uiVertexShaderSrc, uiFragmentShaderSrc) и VAO/VBO/EBO
    // ... (код из вашей функции initUI)
}

void drawRectangle(float x, float y, float w, float h, unsigned int tex, int screenW, int screenH) {
    // Ваша реализация drawRectangle (использует uiShader, ортографическую проекцию)
}

// Реализации методов Button, Slider, Image и UIManager...