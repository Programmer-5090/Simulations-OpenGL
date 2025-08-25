#version 330 core
layout (triangles) in;
layout (line_strip, max_vertices = 6) out;

in vec3 Normal[];
in vec3 Position[];

uniform mat4 view;
uniform mat4 projection;
uniform float normalLength;

void GenerateNormal(int index)
{
    // Start point (vertex position)
    gl_Position = projection * view * vec4(Position[index], 1.0);
    EmitVertex();
    
    // End point (vertex position + normal vector)
    vec3 normalEnd = Position[index] + Normal[index] * normalLength;
    gl_Position = projection * view * vec4(normalEnd, 1.0);
    EmitVertex();
    
    EndPrimitive();
}

void main()
{
    // Generate a normal line for each vertex of the triangle
    GenerateNormal(0);
    GenerateNormal(1);
    GenerateNormal(2);
}
