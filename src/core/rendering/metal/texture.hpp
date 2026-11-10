#pragma once
#include <stb_image.h>

#ifdef MOLE_PLATFORM_APPLE
#include <Metal/Metal.hpp>

class Texture2D {
public:
    Texture2D(const char* filepath, MTL::Device* metalDevice);
    ~Texture2D();
    MTL::Texture* texture;
    int width, height, channels;

private:
    MTL::Device* device;
};
#endif