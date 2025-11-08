#pragma once

#include <cstdint>
#include <vector>
#include <algorithm>
#include <array> 
#include <cstdint>

struct CollisionCell {
    static constexpr size_t CELL_CAPACITY = 16; 

    std::array<uint32_t, CELL_CAPACITY> objects;
    size_t count = 0;

    void addParticle(uint32_t id) {
        if (count < CELL_CAPACITY) {
            objects[count] = id;
            count++;
        }
    }

    void clear() {
        count = 0;
    }

    size_t size() const {
        return count;
    }

    // Iterators
    uint32_t* begin() { return objects.data(); }
    uint32_t* end() { return objects.data() + count; }
    const uint32_t* begin() const { return objects.data(); }
    const uint32_t* end() const { return objects.data() + count; }
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

    bool cellContainsParticle(int32_t x, int32_t y, uint32_t particleID) const {
        if (x >= 0 && x < width && y >= 0 && y < height) {
            const CollisionCell &cell = cells[y * width + x];
            return std::find(cell.objects.begin(), cell.objects.end(), particleID) != cell.objects.end();
        }
        return false;
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
