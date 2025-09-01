#pragma once
#include <glad/glad.h>
#include <vector>

class GPUSort {
public:
    GPUSort();
    ~GPUSort();
    
    bool LoadShaders();
    void SetBuffers(GLuint indexBuffer, GLuint offsetBuffer);
    void SortData(GLuint buffer, int numElements);
    void SortAndCalculateOffsets(GLuint buffer, int numElements);

private:
    void BitonicSort(GLuint buffer, int numElements);
    
    GLuint bitonicSortProgram;
    GLuint indexBuffer;
    GLuint offsetBuffer;
};
