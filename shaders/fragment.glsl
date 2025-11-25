#version 330 core
// REFERENCE: Phong lighting model (ambient, diffuse, specular) adapted from:
// LearnOpenGL - Basic Lighting tutorial: https://learnopengl.com/Lighting/Basic-Lighting
// YouTube tutorial: https://www.youtube.com/watch?v=LKXAIuCaKAQ
// Starting point for Phong lighting, reflection calculations, and normalization

out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;

uniform vec3 objectColor;
uniform sampler2D texture_diffuse1;
uniform bool useTexture;
uniform vec3 sunDirection;  // Direction TO the sun (normalized)
uniform vec3 sunColor;
uniform vec3 viewPos;
uniform vec3 sunPos;        // Actual position of sun in world

void main()
{
    // === DIRECTIONAL LIGHT (Global sun illumination) ===
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(sunDirection);
    
    // Ambient lighting (base illumination)
    float ambientStrength = 0.3;
    vec3 ambient = ambientStrength * sunColor;
    
    // Diffuse lighting (sun shining on surfaces)
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * sunColor;
    
    // Specular lighting (shiny highlights)
    float specularStrength = 0.5;
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = specularStrength * spec * sunColor;
    
    // === POINT LIGHT (Proximity to sun makes things brighter) ===
    vec3 sunLightDir = normalize(sunPos - FragPos);
    float distanceToSun = length(sunPos - FragPos);
    
    // Attenuation: closer to sun = much brighter (quadratic falloff)
    float constant = 1.0;
    float linear = 0.09;
    float quadratic = 0.032;
    float attenuation = 1.0 / (constant + linear * distanceToSun + quadratic * (distanceToSun * distanceToSun));
    
    // Point light diffuse
    float pointDiff = max(dot(norm, sunLightDir), 0.0);
    vec3 pointDiffuse = pointDiff * sunColor * attenuation * 2.0; // Boost intensity
    
    // Point light specular
    vec3 pointReflectDir = reflect(-sunLightDir, norm);
    float pointSpec = pow(max(dot(viewDir, pointReflectDir), 0.0), 32);
    vec3 pointSpecular = pointSpec * sunColor * attenuation;
    
    // === COMBINE ALL LIGHTING ===
    vec3 baseColor = objectColor;
    if(useTexture) {
        baseColor = texture(texture_diffuse1, TexCoord).rgb;
    }
    vec3 result = (ambient + diffuse + specular + pointDiffuse + pointSpecular) * baseColor;
    FragColor = vec4(result, 1.0);
}
