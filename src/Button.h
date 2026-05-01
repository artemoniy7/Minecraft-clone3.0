#pragma once
#include <glm/glm.hpp>
#include <functional>
#include <string>

class Button {
public:
    // Конструктор: положение (x,y) и размер (w,h) – относительные (0..1)
    Button(float relX, float relY, float relW, float relH,
           unsigned int normalTex, unsigned int pressedTex = 0);

    // Установка текста, шрифта и выравнивания
    void setText(const std::string& text, const char* fontPath, int fontSize,
                 unsigned int colorRGBA = 0xFFFFFFFF);
    void setTextAlignment(const std::string& align); // "left", "center", "right"

    // Действие при клике
    void setCallback(std::function<void()> cb);

    // Обновление (вызывается каждый кадр с координатами мыши и состоянием кнопок)
    void update(float mouseX, float mouseY, bool leftClick, bool leftRelease, int screenW, int screenH);

    // Отрисовка
    void render(int screenW, int screenH);

    // Изменение позиции/размера после создания
    void setPosition(float relX, float relY);
    void setSize(float relW, float relH);

private:
    float relX, relY, relW, relH;   // относительные
    float absX, absY, absW, absH;   // абсолютные пиксели

    unsigned int texNormal, texPressed;
    bool usePressedTex;

    std::string text;
    unsigned int textTexture = 0;
    int textWidth = 0, textHeight = 0;
    std::string textAlign; // "left", "center", "right"
    unsigned int textColor;

    std::function<void()> callback;

    bool isHovered = false;
    bool isPressed = false;

    void updateAbsolute(int screenW, int screenH);
    void generateTextTexture(const char* fontPath, int fontSize);
    void drawText(int screenW, int screenH);
};