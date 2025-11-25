#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;
layout (location = 3) in ivec4 aBoneIDs;
layout (location = 4) in vec4 aWeights;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 gBones[100];
uniform bool useAnimation;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;

void main()
{
    vec4 totalPosition = vec4(0.0);
    vec3 totalNormal = vec3(0.0);
    
    if(useAnimation && aWeights[0] > 0.0)
    {
        // Apply bone transformations
        for(int i = 0; i < 4; i++)
        {
            if(aBoneIDs[i] >= 0)
            {
                mat4 boneTransform = gBones[aBoneIDs[i]];
                totalPosition += boneTransform * vec4(aPos, 1.0) * aWeights[i];
                totalNormal += mat3(boneTransform) * aNormal * aWeights[i];
            }
        }
    }
    else
    {
        totalPosition = vec4(aPos, 1.0);
        totalNormal = aNormal;
    }
    
    FragPos = vec3(model * totalPosition);
    Normal = mat3(transpose(inverse(model))) * normalize(totalNormal);
    TexCoord = aTexCoord;
    
    gl_Position = projection * view * model * totalPosition;
}
