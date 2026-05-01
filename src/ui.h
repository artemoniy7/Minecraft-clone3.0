#pragma once
#include <glm/glm.hpp>
#include <functional>
#include <string>
#include <vector>

class Widget {
public:
    float x, y, w, h;
    bool visible = true;
    virtual void update(float mouseX, float mouseY, bool leftClick, bool leftRelease) = 0;
    virtual void render(int screenW, int screenH) = 0;
    virtual ~Widget() = default;
};

class Button : public Widget {
    std::string label;
    unsigned int texNormal, texHover, texPressed;
    bool hovered = false, pressed = false;
    std::function<void()> callback;
public:
    Button(float x, float y, float w, float h, const std::string& text,
           unsigned int texN, unsigned int texH, unsigned int texP,
           std::function<void()> cb);
    void update(float mx, float my, bool lclick, bool lrelease) override;
    void render(int screenW, int screenH) override;
};

class Slider : public Widget {
    float minVal, maxVal, curVal;
    float handleWidth = 20.0f;
    bool dragging = false;
    unsigned int bgTex, handleTex;
    std::function<void(float)> onChanged;
public:
    Slider(float x, float y, float w, float h, float minV, float maxV, float init,
           unsigned int bg, unsigned int handle, std::function<void(float)> cb);
    void update(float mx, float my, bool lclick, bool lrelease) override;
    void render(int screenW, int screenH) override;
    float getValue() const;
    void setValue(float v);
};

class Image : public Widget {
    unsigned int tex;
public:
    Image(float x, float y, float w, float h, unsigned int texture);
    void update(float, float, bool, bool) override {}
    void render(int screenW, int screenH) override;
};

class UIManager {
    std::vector<Widget*> widgets;
    bool capturingEvents = true; // можно отключать для debug-режима
public:
    void addWidget(Widget* w);
    void processInput(float mouseX, float mouseY, bool leftClick, bool leftRelease);
    void render(int screenW, int screenH);
    void clear();
    ~UIManager();
};

// Глобальные функции инициализации / завершения
void initUIRendering();  // создаёт шейдер, VAO и т.п.
void shutdownUIRendering();
void drawRectangle(float x, float y, float w, float h, unsigned int texture, int screenW, int screenH);