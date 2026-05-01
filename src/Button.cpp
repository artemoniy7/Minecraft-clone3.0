#include "Button.h"
#include <GL/glew.h>   // или glad
#include "stb_truetype.h"
#include <cmath>
#include <iostream>
#include <cstring>

extern void drawRectangle(float x, float y, float w, float h, unsigned int tex, int screenW, int screenH);

Button::Button(float relX, float relY, float relW, float relH,
               unsigned int normalTex, unsigned int pressedTex)
    : relX(relX), relY(relY), relW(relW), relH(relH),
      texNormal(normalTex), texPressed(pressedTex),
      usePressedTex(pressedTex != 0), textAlign("center"), textColor(0xFFFFFFFF)
{}

void Button::updateAbsolute(int screenW, int screenH) {
    absW = relW * screenW;
    absH = relH * screenH;
    absX = relX * screenW - absW * 0.5f;
    absY = relY * screenH - absH * 0.5f;
}

void Button::setText(const std::string& txt, const char* fontPath, int fontSize, unsigned int colorRGBA) {
    text = txt;
    textColor = colorRGBA;
    if (textTexture) glDeleteTextures(1, &textTexture);
    if (!text.empty()) generateTextTexture(fontPath, fontSize);
}

void Button::setTextAlignment(const std::string& align) {
    if (align == "left" || align == "center" || align == "right")
        textAlign = align;
}

void Button::setCallback(std::function<void()> cb) {
    callback = cb;
}

void Button::setPosition(float x, float y) {
    relX = x; relY = y;
}

void Button::setSize(float w, float h) {
    relW = w; relH = h;
}

void Button::generateTextTexture(const char* fontPath, int fontSize) {
    // Загружаем шрифт
    FILE* fp = fopen(fontPath, "rb");
    if (!fp) {
        std::cerr << "Failed to load font: " << fontPath << std::endl;
        return;
    }
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    unsigned char* fontBuffer = new unsigned char[size];
    fread(fontBuffer, 1, size, fp);
    fclose(fp);

    stbtt_fontinfo font;
    stbtt_InitFont(&font, fontBuffer, 0);

    // Рассчитываем размеры текста
    float scale = stbtt_ScaleForPixelHeight(&font, (float)fontSize);
    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(&font, &ascent, &descent, &lineGap);
    int textHeightPx = (int)((ascent - descent) * scale);

    // Получаем ширину каждой буквы
    int totalWidth = 0;
    for (char c : text) {
        int advance, lsb;
        stbtt_GetCodepointHMetrics(&font, c, &advance, &lsb);
        totalWidth += (int)(advance * scale);
    }

    // Создаём временное изображение
    int padding = 2;
    int imgW = totalWidth + padding * 2;
    int imgH = textHeightPx + padding * 2;
    unsigned char* bitmap = new unsigned char[imgW * imgH];
    memset(bitmap, 0, imgW * imgH);

    // Рендерим каждую букву
    int xPos = padding;
    for (char c : text) {
        int glyph;
        stbtt_GetCodepointHMetrics(&font, c, &glyph, nullptr);
        int gW, gH, gX, gY;
        unsigned char* glyphBitmap = stbtt_GetCodepointBitmap(&font, 0, scale, c, &gW, &gH, &gX, &gY);
        if (glyphBitmap) {
            for (int row = 0; row < gH; ++row) {
                for (int col = 0; col < gW; ++col) {
                    int imgIdx = (row + gY + padding) * imgW + (xPos + col + gX);
                    if (imgIdx >= 0 && imgIdx < imgW * imgH) {
                        bitmap[imgIdx] = glyphBitmap[row * gW + col];
                    }
                }
            }
            stbtt_FreeBitmap(glyphBitmap, nullptr);
        }
        xPos += (int)(glyph * scale);
    }

    // Генерируем текстуру
    glGenTextures(1, &textTexture);
    glBindTexture(GL_TEXTURE_2D, textTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, imgW, imgH, 0, GL_RED, GL_UNSIGNED_BYTE, bitmap);
    // Превращаем красный канал в RGBA с заданным цветом (будет использоваться в шейдере)
    // Для простоты будем рисовать текст через отдельный прямоугольник с текстурой и шейдером,
    // который интерпретирует красный канал как альфу и умножает на цвет.
    // Но в вашем текущем шейдере uiShader этого нет. Поэтому я предложу упрощённую версию:
    // можно при рендеринге текста использовать отдельный шейдер или изменить drawRectangle.
    // Для экономии времени я далее покажу, как отрисовать текст через стандартный drawRectangle с цветом.

    delete[] bitmap;
    delete[] fontBuffer;

    textWidth = imgW;
    textHeight = imgH;
}

void Button::drawText(int screenW, int screenH) {
    if (!textTexture) return;

    // Позиция текста относительно кнопки
    float textAbsX = absX;
    float textAbsY = absY + (absH - textHeight) / 2.0f; // вертикальный центр

    if (textAlign == "center") {
        textAbsX = absX + (absW - textWidth) / 2.0f;
    } else if (textAlign == "right") {
        textAbsX = absX + absW - textWidth;
    } // left – оставляем как есть

    // Используем отдельный шейдер для текста (поддерживающий цвет и альфа-маску)
    // Но для упрощения: можно нарисовать прямоугольник с текстурой, где текст белый,
    // а затем смешать с цветом через uniform. Либо временно использовать текущий uiShader,
    // но он не умеет подмешивать цвет. Поэтому я предлагаю расширить drawRectangle,
    // добавив параметр цвета (или использовать отдельный шейдер). В рамках примера
    // я покажу вызов drawRectangle, который рисует белый текст – вы потом легко добавите цвет.

    // Вы должны реализовать drawRectangleColored или изменить свой drawRectangle.
    // Здесь для краткости – просто drawRectangle (текст будет белым).
    drawRectangle(textAbsX, textAbsY, textWidth, textHeight, textTexture, screenW, screenH);
}

void Button::update(float mouseX, float mouseY, bool leftClick, bool leftRelease, int screenW, int screenH) {
    updateAbsolute(screenW, screenH);

    bool inside = (mouseX >= absX && mouseX <= absX + absW &&
                   mouseY >= absY && mouseY <= absY + absH);
    if (inside && leftClick) {
        isPressed = true;
    }
    if (isPressed && leftRelease && inside) {
        if (callback) callback();
        isPressed = false;
    }
    if (leftRelease) isPressed = false;
    isHovered = inside;
}

void Button::render(int screenW, int screenH) {
    updateAbsolute(screenW, screenH);

    // Выбираем текстуру
    unsigned int tex = texNormal;
    if (isPressed && usePressedTex) tex = texPressed;
    else if (isPressed) {
        // Если нет текстуры нажатия – затемняем обычную (рисуем поверх полупрозрачный чёрный)
        drawRectangle(absX, absY, absW, absH, texNormal, screenW, screenH);
        // Затемняющий прямоугольник
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        // временный шейдер для цвета (или используйте drawRectangle с отдельной текстурой)
        // Упрощённо: рисуем чёрный квадрат с альфой 0.5 поверх
        // Здесь нужна отдельная функция drawColoredRectangle. Для простоты опускаю.
        glDisable(GL_BLEND);
    } else {
        drawRectangle(absX, absY, absW, absH, tex, screenW, screenH);
    }

    // Рисуем текст
    if (!text.empty()) drawText(screenW, screenH);
}