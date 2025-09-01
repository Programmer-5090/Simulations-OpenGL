#pragma once

#include <cstdint>
#include <vector>

struct CollisionCell {
    std::vector<uint32_t> objects;

    void addParticle(uint32_t id) {
        objects.push_back(id);
    }

    void clear() {
        objects.clear();
    }

    size_t size() const {
        return objects.size();
    }
};


struct CollisionGrid {
    int32_t width, height;
    float cell_size;
    std::vector<CollisionCell> cells;
    
    CollisionGrid(int32_t w, int32_t h, float cs) 
        : width(w), height(h), cell_size(cs) {
        cells.resize(w * h);
    }
    
    void clear() {
        for (auto& cell : cells) cell.clear();
    }

    void addParticle(int32_t x, int32_t y, uint32_t particleID) {
        if (x >= 0 && x < width && y >= 0 && y < height) {
            cells[y * width + x].addParticle(particleID);
        }
    }
    
    CollisionCell& getCell(int32_t x, int32_t y) {
        return cells[y * width + x];
    }
    
    bool isValidCell(int32_t x, int32_t y) const {
        return x >= 0 && x < width && y >= 0 && y < height;
    }
};
