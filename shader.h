#ifndef SHADER_H
#define SHADER_H

#include <string>
#include <fstream>
#include <sstream>
#include<iostream>
#include <glm/glm/glm.hpp>

class Shader{
    public:
        // the program ID
        unsigned int ID;

        // Constructor reads and builds the shader
        Shader(const char* vertexPath, const char* fragmentPath);
        // Constructor with geometry shader
        Shader(const char* vertexPath, const char* fragmentPath, const char* geometryPath);
        // use/activate the shader
        void use();
        // utility uniforms functions
        void setBool(const std::string &name, bool value) const;
        void setInt(const std::string &name, int value) const;
        void setVec3(const std::string &name, glm::vec3 value) const;
        void setVec4(const std::string &name, glm::vec4 value) const;
        void setFloat(const std::string &name, float value) const;
        void setMat4(const std::string &name, const glm::mat4 value) const;
};

#endif