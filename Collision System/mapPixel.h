#ifndef MAP_PIXEL_H
#define MAP_PIXEL_H

#include <array>
#include <map>
#include <vector>
#include <string>
#include <algorithm>
#include <stdexcept>
#include <iostream>
#include <stb_image.h>
#include "particle.h"

using PixelData = std::array<float, 3>;

struct MapPixel {
    std::map<int, PixelData> idToColor;

    void addParticles(const std::vector<Particle>& particles, const std::string& imagePath, 
                     float worldWidth, float worldHeight) {
        idToColor.clear();
        int imgWidth, imgHeight, channels;
        stbi_set_flip_vertically_on_load(true);
        unsigned char* imgData = stbi_load(imagePath.c_str(), &imgWidth, &imgHeight, &channels, 3);
        if (!imgData) throw std::runtime_error("Failed to load image");
        
        std::cout << "Image: " << imgWidth << "x" << imgHeight << ", World: " << worldWidth << "x" << worldHeight << std::endl;
        std::cout << "Mapping " << particles.size() << " particles spatially..." << std::endl;
        
        // Map each particle directly based on its world position to image position
        for (const auto& p : particles) {
            // Normalize particle position to [0, 1] range
            float normX = (p.position.x - (-worldWidth/2.0f)) / worldWidth;
            float normY = (p.position.y - (-worldHeight/2.0f)) / worldHeight;
            
            // Clamp to [0, 1]
            normX = std::max(0.0f, std::min(1.0f, normX));
            normY = std::max(0.0f, std::min(1.0f, normY));
            
            // Convert to pixel coordinates
            int pixelX = std::min((int)(normX * imgWidth), imgWidth - 1);
            int pixelY = std::min((int)(normY * imgHeight), imgHeight - 1);
            
            // Sample image
            int index = (pixelY * imgWidth + pixelX) * 3;
            PixelData color = {
                imgData[index] / 255.0f,
                imgData[index + 1] / 255.0f,
                imgData[index + 2] / 255.0f
            };
            
            // Store by particle ID
            idToColor[p.id] = color;
        }
        
        stbi_image_free(imgData);
        std::cout << "Mapped " << idToColor.size() << " particles spatially by ID" << std::endl;
    }

    PixelData getColorById(int id) const {
        auto it = idToColor.find(id);
        return (it != idToColor.end()) ? it->second : PixelData{0.5f, 0.5f, 0.5f};
    }
    
    bool hasColors() const { return !idToColor.empty(); }
    size_t size() const { return idToColor.size(); }
};

#endif
