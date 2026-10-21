#version 450

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragColor;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform SceneUBO {
    mat4 view;
    mat4 proj;
    vec4 cameraPos;
    vec4 lightDir;
    vec4 lightColor;
    vec4 ambientColor;
} scene;

layout(set = 1, binding = 0) uniform ModelUBO {
    mat4 model;
    vec4 albedo;
    vec4 specularColor;
} model_ubo;

void main() {
    vec3 N = normalize(fragNormal);
    vec3 V = normalize(scene.cameraPos.xyz - fragPos);

    if (dot(N, V) < 0.0) {
        N = -N;
    }

    vec3 L = normalize(-scene.lightDir.xyz);
    vec3 H = normalize(L + V);

    vec3 ambient = scene.ambientColor.xyz * model_ubo.albedo.xyz;
    float diff = max(dot(N, L), 0.0);
    vec3 diffuse = diff * scene.lightColor.xyz * scene.lightColor.w * model_ubo.albedo.xyz;
    float shininess = model_ubo.albedo.w;
    float spec = pow(max(dot(N, H), 0.0), shininess);
    vec3 specular = spec * scene.lightColor.xyz * scene.lightColor.w * model_ubo.specularColor.xyz;

    vec3 result = ambient + diffuse + specular;
    result *= fragColor;

    outColor = vec4(result, 1.0);
}