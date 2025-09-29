#pragma once
#include <glad/glad.h>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>

class ComputeHelper {
public:
    static GLuint LoadComputeShader(const std::string& filePath) {
        std::string computeSource = ReadFile(filePath);
        if (computeSource.empty()) {
            std::cerr << "Failed to read compute shader file: " << filePath << std::endl;
            return 0;
        }
        
        GLuint computeShader = glCreateShader(GL_COMPUTE_SHADER);
        const char* source = computeSource.c_str();
        glShaderSource(computeShader, 1, &source, nullptr);
        glCompileShader(computeShader);
        
        // Check compilation errors
        GLint success;
        glGetShaderiv(computeShader, GL_COMPILE_STATUS, &success);
        if (!success) {
            GLchar infoLog[1024];
            glGetShaderInfoLog(computeShader, 1024, nullptr, infoLog);
            std::cerr << "Compute shader compilation failed (" << filePath << "):\n" << infoLog << std::endl;
            glDeleteShader(computeShader);
            return 0;
        }
        
        GLuint program = glCreateProgram();
        glAttachShader(program, computeShader);
        glLinkProgram(program);
        
        // Check linking errors
        glGetProgramiv(program, GL_LINK_STATUS, &success);
        if (!success) {
            GLchar infoLog[1024];
            glGetProgramInfoLog(program, 1024, nullptr, infoLog);
            std::cerr << "Compute shader program linking failed (" << filePath << "):\n" << infoLog << std::endl;
            glDeleteProgram(program);
            glDeleteShader(computeShader);
            return 0;
        }
        
        glDeleteShader(computeShader);
        return program;
    }
    
    static GLuint CreateBuffer(size_t size, const void* data = nullptr, GLenum usage = GL_DYNAMIC_DRAW) {
        GLuint buffer;
        glGenBuffers(1, &buffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, size, data, usage);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
        return buffer;
    }
    
    static void BindBuffer(GLuint buffer, GLuint binding) {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, buffer);
    }
    
    static void Dispatch(GLuint program, GLuint numGroupsX, GLuint numGroupsY = 1, GLuint numGroupsZ = 1) {
        glUseProgram(program);
        glDispatchCompute(numGroupsX, numGroupsY, numGroupsZ);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }
    
    static GLuint GetThreadGroupSizes(int numThreads, int groupSize = 64) {
        return (numThreads + groupSize - 1) / groupSize;
    }
    
    template<typename T>
    static std::vector<T> ReadBuffer(GLuint buffer, size_t count) {
        std::vector<T> data(count);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
        void* ptr = glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
        if (ptr) {
            std::memcpy(data.data(), ptr, count * sizeof(T));
            glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        }
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
        return data;
    }
    
    template<typename T>
    static void WriteBuffer(GLuint buffer, const std::vector<T>& data) {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
        GLsizeiptr sizeBytes = static_cast<GLsizeiptr>(data.size() * sizeof(T));
        // Orphan the buffer to avoid GPU sync with previous users of the buffer
        glBufferData(GL_SHADER_STORAGE_BUFFER, sizeBytes, nullptr, GL_DYNAMIC_DRAW);

        // Map with invalidate to avoid waiting on GPU for previous contents
        void* ptr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, sizeBytes, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
        if (ptr) {
            std::memcpy(ptr, data.data(), sizeBytes);
            glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        } else {
            // Fallback: try glBufferSubData which may still work
            glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeBytes, data.data());
        }
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }
    
    static void Release(GLuint& buffer) {
        if (buffer != 0) {
            glDeleteBuffers(1, &buffer);
            buffer = 0;
        }
    }
    
    static void ReleaseProgram(GLuint& program) {
        if (program != 0) {
            glDeleteProgram(program);
            program = 0;
        }
    }

private:
    static std::string ReadFile(const std::string& filePath) {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            return "";
        }
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }
};
