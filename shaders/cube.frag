#version 450

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragColor;

layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec4 cameraPos;
    vec4 lightDir;
    vec4 lightColor;
    vec4 ambientColor;
    vec4 albedo;
    vec4 specularColor;
} ubo;

void main() {
    vec3 N = normalize(fragNormal);
    vec3 V = normalize(ubo.cameraPos.xyz - fragPos);

    // Если нормаль смотрит от камеры (мы видим "заднюю" сторону), переворачиваем её
    if (dot(N, V) < 0.0) {
        N = -N; 
    }

    vec3 L = normalize(-ubo.lightDir.xyz);
    vec3 H = normalize(L + V);

    // 1. Ambient
    vec3 ambient = ubo.ambientColor.xyz * ubo.albedo.xyz;

    // 2. Diffuse
    float diff = max(dot(N, L), 0.0);
    vec3 diffuse = diff * ubo.lightColor.xyz * ubo.lightColor.w * ubo.albedo.xyz;

    // 3. Specular
    float shininess = ubo.albedo.w;
    float spec = pow(max(dot(N, H), 0.0), shininess);
    vec3 specular = spec * ubo.lightColor.xyz * ubo.lightColor.w * ubo.specularColor.xyz;

    vec3 result = ambient + diffuse + specular;
    result *= fragColor; 

    outColor = vec4(result, 1.0);
}