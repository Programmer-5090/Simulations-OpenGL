#version 330 core
out vec4 FragColor;

in vec3 Normal;
in vec2 TexCoords;
in vec3 FragPos;
in vec3 LocalPos;
in vec3 LocalNormal;

// Simple lighting uniforms
uniform vec3 viewPos;

struct Material {
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float shininess;
    bool useTexture;
};
uniform Material material;

struct DirLight {
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

const int MAX_DIR_LIGHTS = 6;
uniform DirLight dirLights[MAX_DIR_LIGHTS];
uniform int dirLightCount;

uniform sampler2D texture_diffuse1;
uniform sampler2D faceTextures[6];
uniform bool useFaceTextures;
uniform int debugDisplayMode; // 0 = normal lighting, 1 = show baseColor (unlit)
uniform int cubieID;
uniform int selectedID;

// NEW: Direct face color indices (set per cubie from CPU)
// Each element is the color index for that geometric face, or -1 for black/internal
// Face indices: 0=+X, 1=-X, 2=+Y, 3=-Y, 4=+Z, 5=-Z
uniform int faceColorIndex[6];

// Standard Rubik's Cube face colors
// Color indices: 0=Red, 1=Orange, 2=White, 3=Yellow, 4=Green, 5=Blue
const vec3 kFaceColors[6] = vec3[6](
    vec3(1.0, 0.0, 0.0),   // 0: red
    vec3(1.0, 0.5, 0.0),   // 1: orange
    vec3(1.0, 1.0, 1.0),   // 2: white
    vec3(1.0, 1.0, 0.0),   // 3: yellow
    vec3(0.0, 1.0, 0.0),   // 4: green
    vec3(0.0, 0.2, 1.0)    // 5: blue
);

// Determine which geometric face this fragment is on and get UV coordinates
// Returns the face index (0-5), or -1 if near an edge/seam (black plastic)
int getGeometricFace(vec3 localPos, vec3 normal, out vec2 uv)
{
    vec3 n = normalize(normal);
    vec3 p = localPos;
    vec3 ap = abs(p);
    float maxAxis = max(ap.x, max(ap.y, ap.z));

    if (maxAxis < 1e-4)
    {
        p = n;
        ap = abs(p);
        maxAxis = max(ap.x, max(ap.y, ap.z));
    }

    vec3 projected = p / maxAxis;
    vec3 aprRaw = abs(projected);
    vec3 apr = aprRaw + vec3(0.0003, 0.0002, 0.0001); // tie-breaker

    // Determine current geometric face
    int currentFace;
    if (apr.x > apr.y && apr.x > apr.z)
    {
        if (projected.x > 0.0)
        {
            currentFace = 0; // +X
            uv = vec2(-projected.z, projected.y);
        }
        else
        {
            currentFace = 1; // -X
            uv = vec2(projected.z, projected.y);
        }
    }
    else if (apr.y > apr.z)
    {
        if (projected.y > 0.0)
        {
            currentFace = 2; // +Y (top)
            uv = vec2(projected.x, -projected.z);
        }
        else
        {
            currentFace = 3; // -Y (bottom)
            uv = vec2(projected.x, projected.z);
        }
    }
    else
    {
        if (projected.z > 0.0)
        {
            currentFace = 4; // +Z
            uv = vec2(projected.x, projected.y);
        }
        else
        {
            currentFace = 5; // -Z
            uv = vec2(-projected.x, projected.y);
        }
    }

    // Detect if we're near an edge where two axes compete -> black plastic seam
    float largest = max(aprRaw.x, max(aprRaw.y, aprRaw.z));
    float secondLargest = 0.0;
    if (largest == aprRaw.x)
        secondLargest = max(aprRaw.y, aprRaw.z);
    else if (largest == aprRaw.y)
        secondLargest = max(aprRaw.x, aprRaw.z);
    else
        secondLargest = max(aprRaw.x, aprRaw.y);

    if (largest - secondLargest < 0.08)
    {
        return -1; // edge/corner -> black plastic
    }

    // Map UV from [-1,1] to [0,1]
    uv = uv * 0.5 + 0.5;
    uv = clamp(uv, vec2(0.0), vec2(1.0));

    return currentFace;
}

// Get the color for this fragment based on face mapping
vec3 getCubieColor(vec3 localPos, vec3 normal)
{
    vec2 uv;
    int geometricFace = getGeometricFace(localPos, normal, uv);

    // Edge/seam -> black plastic
    if (geometricFace < 0)
    {
        return vec3(0.02);
    }

    // Get the color index for this geometric face
    int colorIdx = faceColorIndex[geometricFace];
    
    // -1 means internal/black
    if (colorIdx < 0)
    {
        return vec3(0.02);
    }

    // Textures ON: sample from per-face texture
    if (useFaceTextures)
    {
        return texture(faceTextures[colorIdx], uv).rgb;
    }

    // Textures OFF: use solid face colors
    return kFaceColors[colorIdx];
}

void main()
{
    // Normalize the normal vector
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);

    // Get base color for this cubie face
    vec3 baseColor = getCubieColor(LocalPos, LocalNormal);

    // Debug shortcut: render the base color directly (no lighting) to inspect textures/mapping
    if (debugDisplayMode == 1)
    {
        FragColor = vec4(baseColor, 1.0);
        return;
    }

    vec3 result = vec3(0.0);
    for (int i = 0; i < dirLightCount && i < MAX_DIR_LIGHTS; ++i)
    {
        vec3 lightDir = normalize(-dirLights[i].direction);

        vec3 ambient = dirLights[i].ambient * baseColor;

        float diff = max(dot(norm, lightDir), 0.0);
        vec3 diffuse = dirLights[i].diffuse * diff * baseColor;

        vec3 reflectDir = reflect(-lightDir, norm);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
        vec3 specular = dirLights[i].specular * spec * material.specular;

        result += ambient + diffuse + specular;
    }

    // Highlight if this cubie is selected
    if (cubieID >= 0 && cubieID == selectedID)
    {
        vec3 highlight = vec3(1.0, 0.0, 1.0);
        result = mix(result, highlight, 0.35);
    }

    FragColor = vec4(result, 1.0);
}
