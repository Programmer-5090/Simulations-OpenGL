#include "GPUSort.h"
#include "ComputeHelper.h"
#include <iostream>
#include <algorithm>
#include <cmath>

// GPUSort: wrapper around bitonic sort compute shader.

GPUSort::GPUSort() : bitonicSortProgram(0) {
    LoadShaders();
}

GPUSort::~GPUSort() {
    ComputeHelper::ReleaseProgram(bitonicSortProgram);
}

bool GPUSort::LoadShaders() {
    bitonicSortProgram = ComputeHelper::LoadComputeShader("SPHFluid/shaders/BitonicSort.compute");
    return bitonicSortProgram != 0;
}

void GPUSort::SetBuffers(GLuint indexBuffer, GLuint offsetBuffer) {
    this->indexBuffer = indexBuffer;
    this->offsetBuffer = offsetBuffer;
}

void GPUSort::SortData(GLuint buffer, int numElements) {
    if (bitonicSortProgram == 0) {
        std::cerr << "Bitonic sort program not loaded!" << std::endl;
        return;
    }
    
    BitonicSort(buffer, numElements);
}

// Helper function to find the next power of 2
int NextPowerOfTwo(int n) {
    int p = 1;
    while (p < n) {
        p *= 2;
    }
    return p;
}

// Returns smallest power of two >= n.

void GPUSort::BitonicSort(GLuint buffer, int numElements) {
    ComputeHelper::BindBuffer(buffer, 0);

    glUseProgram(bitonicSortProgram);
    glUniform1i(glGetUniformLocation(bitonicSortProgram, "numElements"), numElements);
    glUniform1i(glGetUniformLocation(bitonicSortProgram, "currentKernel"), 0); // Sort kernel
    
    
    int numElementsPowerOf2 = NextPowerOfTwo(numElements);
    int numStages = static_cast<int>(std::log2(numElementsPowerOf2));

    for (int stageIndex = 0; stageIndex < numStages; stageIndex++) {
        for (int stepIndex = 0; stepIndex < stageIndex + 1; stepIndex++) {
            // Calculate uniformss
            int groupWidth = 1 << (stageIndex - stepIndex);
            int groupHeight = 2 * groupWidth - 1;
            
            glUniform1i(glGetUniformLocation(bitonicSortProgram, "groupWidth"), groupWidth);
            glUniform1i(glGetUniformLocation(bitonicSortProgram, "groupHeight"), groupHeight);
            glUniform1i(glGetUniformLocation(bitonicSortProgram, "stepIndex"), stepIndex);

            // The dispatch size
            int numThreadsToDispatch = numElementsPowerOf2 / 2;
            int numGroups = (numThreadsToDispatch + 127) / 128; // 128 threads per group
            
            ComputeHelper::Dispatch(bitonicSortProgram, numGroups);
            
            // Wait for completion before next iteration
            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        }
    }
}

void GPUSort::SortAndCalculateOffsets(GLuint buffer, int numElements) {
    if (bitonicSortProgram == 0) {
        std::cerr << "Bitonic sort program not loaded!" << std::endl;
        return;
    }
    
    // First sort the data
    BitonicSort(buffer, numElements);
    
    // Then calculate offsets
    ComputeHelper::BindBuffer(buffer, 0);
    ComputeHelper::BindBuffer(offsetBuffer, 1);
    
    glUseProgram(bitonicSortProgram);
    glUniform1i(glGetUniformLocation(bitonicSortProgram, "numElements"), numElements);
    glUniform1i(glGetUniformLocation(bitonicSortProgram, "currentKernel"), 1); // CalculateOffsets kernel
    
    int numGroups = (numElements + 127) / 128; // 128 threads per group
    ComputeHelper::Dispatch(bitonicSortProgram, numGroups);
    // Wait for completion before next iteration
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}