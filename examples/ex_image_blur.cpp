// MIT License
//
// Copyright (c) 2020 Mechatronics and Haptic Interfaces Lab - Rice University
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// Author(s): Evan Pezent (epezent@rice.edu)

#include <Mahi/Gui.hpp>
#include <Mahi/Util.hpp>
#include "stb_image.h"

using namespace mahi::gui;
using namespace mahi::util;


class Texture 
{
public:

    Texture() = default;

    ~Texture() 
    { 
        releaseTexture();
        freeImageData();
    }

    bool initTextureFromData(int width, int height, uint8_t* image_data, bool own_data) 
    {
        releaseTexture();
        freeImageData();

        if (!image_data)
            return false;

        texture_width = width;
        texture_height = height;

        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texture_width, texture_height, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, image_data);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);

        if (own_data) {
            texture_data_len  = texture_width * texture_height * 4;
            texture_data = image_data;
        }

        gl_texture = texture;
        return true;
    }

    bool initTextureFromFile(const char* filename, bool clone_data) 
    {
        int            width, height;
        unsigned char* image_data = stbi_load(filename, &width, &height, NULL, 4);
        if (image_data == NULL)
            return false;

        bool ret = false;
        
        if (clone_data) 
        {
            size_t data_len = width * height * 4;
            auto data = (uint8_t*)malloc(data_len);
            ::memcpy(data, image_data, data_len);
            ret = initTextureFromData(width, height, data, true);
        } else {
            ret = initTextureFromData(width, height, image_data, false);
        }

        stbi_image_free(image_data);

        return ret;
    }

    void updateSelf()
    {
        if (texture_data)
            updateTexture(texture_width, texture_height, texture_data, true);
    }

    void updateTexture(int width, int height, uint8_t *data, bool own_data) 
    {
        assert(data != nullptr);
        assert(gl_texture > 0);

        texture_width  = width;
        texture_height = height;

        glBindTexture(GL_TEXTURE_2D, gl_texture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data);
        glBindTexture(GL_TEXTURE_2D, 0);

        if (own_data) {
            if (texture_data != data) {
                freeImageData();
                texture_data_len = texture_width * texture_height * 4;
                texture_data     = data;
            }
        } else {
            freeImageData();
        }
    }

    void freeImageData() 
    {
        if (texture_data) 
        {
            free(texture_data);
            texture_data = nullptr;
            texture_data_len  = 0;
        }
    }

    void releaseTexture()
    {
        if (gl_texture != 0) {
            glDeleteTextures(1, &gl_texture);
            gl_texture = 0;
        }
    }

    bool isOk() { return gl_texture != 0; }

    GLuint gl_texture = 0;
    int    texture_width   = 0;
    int    texture_height = 0;
    uint8_t* texture_data    = nullptr;
    size_t texture_data_len    = 0;
};

inline bool ends_with(std::string const& value, std::string const& ending) {
    if (ending.size() > value.size())
        return false;
    return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

/// <summary>
/// ref https://github.com/tkzcfc/Telegram/tree/master/TMessagesProj/jni/image.cpp
/// </summary>

// default 150 * 150
#define MAX_BLUR_IMAGE_W 1000
#define MAX_BLUR_IMAGE_H 1000

static inline uint64_t getColors(const uint8_t *p) {
    return p[0] + (p[1] << 16) + ((uint64_t)p[2] << 32) + ((uint64_t)p[3] << 48);
}

static inline uint64_t getColors565(const uint8_t *p) {
    uint16_t *ps = (uint16_t *)p;
    return ((((ps[0] & 0xF800) >> 11) * 255) / 31) +
           (((((ps[0] & 0x07E0) >> 5) * 255) / 63) << 16) +
           ((uint64_t)(((ps[0] & 0x001F) * 255) / 31) << 32);
}

static void fastBlurMore(int32_t w, int32_t h, int32_t stride, uint8_t *pix, int32_t radius) {
    const int32_t r1  = radius + 1;
    const int32_t div = radius * 2 + 1;

    if (radius > 15 || div >= w || div >= h || w * h > MAX_BLUR_IMAGE_W * MAX_BLUR_IMAGE_H || stride > w * 4) {
        return;
    }

    uint64_t *rgb = new uint64_t[w * h];
    if (rgb == NULL) {
        return;
    }

    int32_t x, y, i;

    int32_t       yw = 0;
    const int32_t we = w - r1;
    for (y = 0; y < h; y++) {
        uint64_t cur       = getColors(&pix[yw]);
        uint64_t rgballsum = -radius * cur;
        uint64_t rgbsum    = cur * ((r1 * (r1 + 1)) >> 1);

        for (i = 1; i <= radius; i++) {
            cur = getColors(&pix[yw + i * 4]);
            rgbsum += cur * (r1 - i);
            rgballsum += cur;
        }

        x = 0;

#define update(start, middle, end)                                                                 \
    rgb[y * w + x] = (rgbsum >> 6) & 0x00FF00FF00FF00FF;                                           \
    rgballsum += getColors(&pix[yw + (start)*4]) - 2 * getColors(&pix[yw + (middle)*4]) +          \
                 getColors(&pix[yw + (end)*4]);                                                    \
    rgbsum += rgballsum;                                                                           \
    x++;

        while (x < r1) {
            update(0, x, x + r1)
        }
        while (x < we) {
            update(x - r1, x, x + r1)
        }
        while (x < w) {
            update(x - r1, x, w - 1)
        }
#undef update

        yw += stride;
    }

    const int32_t he = h - r1;
    for (x = 0; x < w; x++) {
        uint64_t rgballsum = -radius * rgb[x];
        uint64_t rgbsum    = rgb[x] * ((r1 * (r1 + 1)) >> 1);
        for (i = 1; i <= radius; i++) {
            rgbsum += rgb[i * w + x] * (r1 - i);
            rgballsum += rgb[i * w + x];
        }

        y          = 0;
        int32_t yi = x * 4;

#define update(start, middle, end)                                                                 \
    int64_t res = rgbsum >> 6;                                                                     \
    pix[yi]     = res;                                                                             \
    pix[yi + 1] = res >> 16;                                                                       \
    pix[yi + 2] = res >> 32;                                                                       \
    pix[yi + 3] = res >> 48;                                                                       \
    rgballsum += rgb[x + (start)*w] - 2 * rgb[x + (middle)*w] + rgb[x + (end)*w];                  \
    rgbsum += rgballsum;                                                                           \
    y++;                                                                                           \
    yi += stride;

        while (y < r1) {
            update(0, y, y + r1)
        }
        while (y < he) {
            update(y - r1, y, y + r1)
        }
        while (y < h) {
            update(y - r1, y, h - 1)
        }
#undef update
    }

    delete[] rgb;
}

static void fastBlur(int32_t w, int32_t h, int32_t stride, uint8_t *pix, int32_t radius) {
    if (pix == nullptr) {
        return;
    }
    const int32_t r1  = radius + 1;
    const int32_t div = radius * 2 + 1;
    int32_t       shift;
    if (radius == 1) {
        shift = 2;
    } else if (radius == 3) {
        shift = 4;
    } else if (radius == 7) {
        shift = 6;
    } else if (radius == 15) {
        shift = 8;
    } else {
        return;
    }

    if (radius > 15 || div >= w || div >= h || w * h > MAX_BLUR_IMAGE_W * MAX_BLUR_IMAGE_H || stride > w * 4) {
        return;
    }

    uint64_t *rgb = new uint64_t[w * h];
    if (rgb == nullptr) {
        return;
    }

    int32_t x, y, i;

    int32_t       yw = 0;
    const int32_t we = w - r1;
    for (y = 0; y < h; y++) {
        uint64_t cur       = getColors(&pix[yw]);
        uint64_t rgballsum = -radius * cur;
        uint64_t rgbsum    = cur * ((r1 * (r1 + 1)) >> 1);

        for (i = 1; i <= radius; i++) {
            cur = getColors(&pix[yw + i * 4]);
            rgbsum += cur * (r1 - i);
            rgballsum += cur;
        }

        x = 0;

#define update(start, middle, end)                                                                 \
    rgb[y * w + x] = (rgbsum >> shift) & 0x00FF00FF00FF00FFLL;                                     \
    rgballsum += getColors(&pix[yw + (start)*4]) - 2 * getColors(&pix[yw + (middle)*4]) +          \
                 getColors(&pix[yw + (end)*4]);                                                    \
    rgbsum += rgballsum;                                                                           \
    x++;

        while (x < r1) {
            update(0, x, x + r1)
        }
        while (x < we) {
            update(x - r1, x, x + r1)
        }
        while (x < w) {
            update(x - r1, x, w - 1)
        }

#undef update

        yw += stride;
    }

    const int32_t he = h - r1;
    for (x = 0; x < w; x++) {
        uint64_t rgballsum = -radius * rgb[x];
        uint64_t rgbsum    = rgb[x] * ((r1 * (r1 + 1)) >> 1);
        for (i = 1; i <= radius; i++) {
            rgbsum += rgb[i * w + x] * (r1 - i);
            rgballsum += rgb[i * w + x];
        }

        y          = 0;
        int32_t yi = x * 4;

#define update(start, middle, end)                                                                 \
    int64_t res = rgbsum >> shift;                                                                 \
    pix[yi]     = res;                                                                             \
    pix[yi + 1] = res >> 16;                                                                       \
    pix[yi + 2] = res >> 32;                                                                       \
    pix[yi + 3] = res >> 48;                                                                       \
    rgballsum += rgb[x + (start)*w] - 2 * rgb[x + (middle)*w] + rgb[x + (end)*w];                  \
    rgbsum += rgballsum;                                                                           \
    y++;                                                                                           \
    yi += stride;

        while (y < r1) {
            update(0, y, y + r1)
        }
        while (y < he) {
            update(y - r1, y, y + r1)
        }
        while (y < h) {
            update(y - r1, y, h - 1)
        }
#undef update
    }

    delete[] rgb;
}

static void fastBlurMore565(int32_t w, int32_t h, int32_t stride, uint8_t *pix, int32_t radius) {
    const int32_t r1  = radius + 1;
    const int32_t div = radius * 2 + 1;

    if (radius > 15 || div >= w || div >= h || w * h > MAX_BLUR_IMAGE_W * MAX_BLUR_IMAGE_H || stride > w * 2) {
        return;
    }

    uint64_t *rgb = new uint64_t[w * h];
    if (rgb == NULL) {
        return;
    }

    int32_t x, y, i;

    int32_t       yw = 0;
    const int32_t we = w - r1;
    for (y = 0; y < h; y++) {
        uint64_t cur       = getColors565(&pix[yw]);
        uint64_t rgballsum = -radius * cur;
        uint64_t rgbsum    = cur * ((r1 * (r1 + 1)) >> 1);

        for (i = 1; i <= radius; i++) {
            cur = getColors565(&pix[yw + i * 2]);
            rgbsum += cur * (r1 - i);
            rgballsum += cur;
        }

        x = 0;

#define update(start, middle, end)                                                                 \
    rgb[y * w + x] = (rgbsum >> 6) & 0x00FF00FF00FF00FF;                                           \
    rgballsum += getColors565(&pix[yw + (start)*2]) - 2 * getColors565(&pix[yw + (middle)*2]) +    \
                 getColors565(&pix[yw + (end)*2]);                                                 \
    rgbsum += rgballsum;                                                                           \
    x++;

        while (x < r1) {
            update(0, x, x + r1)
        }
        while (x < we) {
            update(x - r1, x, x + r1)
        }
        while (x < w) {
            update(x - r1, x, w - 1)
        }
#undef update

        yw += stride;
    }

    const int32_t he = h - r1;
    for (x = 0; x < w; x++) {
        uint64_t rgballsum = -radius * rgb[x];
        uint64_t rgbsum    = rgb[x] * ((r1 * (r1 + 1)) >> 1);
        for (i = 1; i <= radius; i++) {
            rgbsum += rgb[i * w + x] * (r1 - i);
            rgballsum += rgb[i * w + x];
        }

        y          = 0;
        int32_t yi = x * 2;

#define update(start, middle, end)                                                                 \
    int64_t res = rgbsum >> 6;                                                                     \
    pix[yi]     = ((res >> 13) & 0xe0) | ((res >> 35) & 0x1f);                                     \
    pix[yi + 1] = (res & 0xf8) | ((res >> 21) & 0x7);                                              \
    rgballsum += rgb[x + (start)*w] - 2 * rgb[x + (middle)*w] + rgb[x + (end)*w];                  \
    rgbsum += rgballsum;                                                                           \
    y++;                                                                                           \
    yi += stride;

        while (y < r1) {
            update(0, y, y + r1)
        }
        while (y < he) {
            update(y - r1, y, y + r1)
        }
        while (y < h) {
            update(y - r1, y, h - 1)
        }
#undef update
    }

    delete[] rgb;
}

static void fastBlur565(int32_t w, int32_t h, int32_t stride, uint8_t *pix, int32_t radius) {
    if (pix == NULL) {
        return;
    }
    const int32_t r1  = radius + 1;
    const int32_t div = radius * 2 + 1;
    int32_t       shift;
    if (radius == 1) {
        shift = 2;
    } else if (radius == 3) {
        shift = 4;
    } else if (radius == 7) {
        shift = 6;
    } else if (radius == 15) {
        shift = 8;
    } else {
        return;
    }

    if (radius > 15 || div >= w || div >= h || w * h > MAX_BLUR_IMAGE_W * MAX_BLUR_IMAGE_H || stride > w * 2) {
        return;
    }

    uint64_t *rgb = new uint64_t[w * h];
    if (rgb == NULL) {
        return;
    }

    int32_t x, y, i;

    int32_t       yw = 0;
    const int32_t we = w - r1;
    for (y = 0; y < h; y++) {
        uint64_t cur       = getColors565(&pix[yw]);
        uint64_t rgballsum = -radius * cur;
        uint64_t rgbsum    = cur * ((r1 * (r1 + 1)) >> 1);

        for (i = 1; i <= radius; i++) {
            cur = getColors565(&pix[yw + i * 2]);
            rgbsum += cur * (r1 - i);
            rgballsum += cur;
        }

        x = 0;

#define update(start, middle, end)                                                                 \
    rgb[y * w + x] = (rgbsum >> shift) & 0x00FF00FF00FF00FFLL;                                     \
    rgballsum += getColors565(&pix[yw + (start)*2]) - 2 * getColors565(&pix[yw + (middle)*2]) +    \
                 getColors565(&pix[yw + (end)*2]);                                                 \
    rgbsum += rgballsum;                                                                           \
    x++;

        while (x < r1) {
            update(0, x, x + r1)
        }
        while (x < we) {
            update(x - r1, x, x + r1)
        }
        while (x < w) {
            update(x - r1, x, w - 1)
        }

#undef update

        yw += stride;
    }

    const int32_t he = h - r1;
    for (x = 0; x < w; x++) {
        uint64_t rgballsum = -radius * rgb[x];
        uint64_t rgbsum    = rgb[x] * ((r1 * (r1 + 1)) >> 1);
        for (i = 1; i <= radius; i++) {
            rgbsum += rgb[i * w + x] * (r1 - i);
            rgballsum += rgb[i * w + x];
        }

        y          = 0;
        int32_t yi = x * 2;

#define update(start, middle, end)                                                                 \
    uint64_t res = rgbsum >> shift;                                                                \
    pix[yi]      = ((res >> 13) & 0xe0) | ((res >> 35) & 0x1f);                                    \
    pix[yi + 1]  = (res & 0xf8) | ((res >> 21) & 0x7);                                             \
    rgballsum += rgb[x + (start)*w] - 2 * rgb[x + (middle)*w] + rgb[x + (end)*w];                  \
    rgbsum += rgballsum;                                                                           \
    y++;                                                                                           \
    yi += stride;

        while (y < r1) {
            update(0, y, y + r1)
        }
        while (y < he) {
            update(y - r1, y, y + r1)
        }
        while (y < h) {
            update(y - r1, y, h - 1)
        }
#undef update
    }

    delete[] rgb;
}




class ImageBlurDemo : public Application {
public:
    ImageBlurDemo() : Application() {     
    }
    
    void update() override {
        ImGui::SetNextWindowPos({800, 600}, ImGuiCond_FirstUseEver);
        ImGui::Begin("Main Dialog", nullptr);

        if (ImGui::Button(ICON_FA_FILE "##1")) {
            std::string filename;
            if (open_dialog(filename, {{"png file", "png"}}) == DialogResult::DialogOkay) {
                resetRawTexture(filename);
            }
        }

        ImGui::Separator();

        if (rawTexture.isOk()) {
            ImGui::Text("raw image");
            renderTexture(rawTexture);

            bool ok = false;
            ok |= ImGui::InputInt("stride", &blurStride, 4);
            ok |= ImGui::InputInt("radius", &blurRadius);

            if (blurStride > rawTexture.texture_width * 4)
                blurStride = rawTexture.texture_width * 4;
            if (blurStride <= 0)
                blurStride = 0;

            if (blurRadius > 15)
                blurRadius = 15;
            if (blurRadius <= 0)
                blurRadius = 0;

            if (ok)
                updateBlurTexture();

            ImGui::Text("blur image");
            renderTexture(blurTexture);
        }
        ImGui::End();
    }

    void updateBlurTexture() {
        if (!rawTexture.isOk() || rawTexture.texture_data == nullptr)
            return;

        if (blurStride <= 0)
            return;

        auto pixels = (uint8_t*)malloc(rawTexture.texture_data_len);
        auto width  = rawTexture.texture_width;
        auto height = rawTexture.texture_height;
        memcpy(pixels, rawTexture.texture_data, rawTexture.texture_data_len);

        if (blurStride == width * 2) {
            if (blurRadius <= 3) {
                fastBlur565(width, height, blurStride, (uint8_t *)pixels, blurRadius);
            } else {
                fastBlurMore565(width, height, blurStride, (uint8_t *)pixels, blurRadius);
            }
        } else {
            if (blurRadius <= 3) {
                fastBlur(width, height, blurStride, (uint8_t *)pixels, blurRadius);
            } else {
                fastBlurMore(width, height, blurStride, (uint8_t *)pixels, blurRadius);
            }
        }

        blurTexture.initTextureFromData(width, height, pixels, true);
    }

    void resetRawTexture(const std::string& path) {
        rawTexture.initTextureFromFile(path.c_str(), true);
        // 只有  fastBlur 函数效果正常
        blurStride = rawTexture.texture_width * 4;
        blurRadius = 3;
        updateBlurTexture();
    }

    void renderTexture(Texture& texture) 
    {
        if (texture.isOk()) 
        {
            ImGui::Image((void*)(intptr_t)texture.gl_texture,
                         ImVec2(texture.texture_width, texture.texture_height));
        }
    }
    
    Texture rawTexture;
    Texture blurTexture;
    int     blurStride = 1;
    int     blurRadius = 1;
};

int main( void )
{
    ImageBlurDemo demo;
    demo.run();
    return 0;
}
