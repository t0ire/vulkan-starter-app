#version 450

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragColor;

layout(location = 0) out vec4 outColor;

//Set 0, Binding 0: Scene UBO
layout(set = 0, binding = 0) uniform SceneUBO {
    mat4 view;
    mat4 proj;
    vec4 cameraPos;
    vec4 lightDir;
    vec4 lightColor;
    vec4 ambientColor;
} scene;

//Set 0, Binding 1: Point Lights Storage Buffer
struct PointLight {
    vec4 position;  //xyz = позиция, w = padding
    vec4 color;     //xyz = цвет, w = интенсивность
};
layout(set = 0, binding = 1) readonly buffer PointLights {
    PointLight lights[];
} point_lights;

//Set 1, Binding 0: Model UBO
layout(set = 1, binding = 0) uniform ModelUBO {
    mat4 model;
    vec4 albedo;
    vec4 specularColor;
} model_ubo;

void main() {
    vec3 N = normalize(fragNormal);
    vec3 V = normalize(scene.cameraPos.xyz - fragPos);

    //если нормаль смотрит от камеры, переворачиваем
    if (dot(N, V) < 0.0) {
        N = -N;
    }

    //направленный свет
    vec3 L = normalize(-scene.lightDir.xyz);
    vec3 H = normalize(L + V);
    vec3 ambient = scene.ambientColor.xyz * model_ubo.albedo.xyz;
    float diff = max(dot(N, L), 0.0);
    vec3 diffuse = diff * scene.lightColor.xyz * scene.lightColor.w * model_ubo.albedo.xyz;
    float shininess = model_ubo.albedo.w;
    float spec = pow(max(dot(N, H), 0.0), shininess);
    vec3 specular = spec * scene.lightColor.xyz * scene.lightColor.w * model_ubo.specularColor.xyz;
    vec3 result = ambient + diffuse + specular;

    //точечные источники
    for (int i = 0; i < point_lights.lights.length(); ++i) {
        vec3 lightPos = point_lights.lights[i].position.xyz;
        vec3 lightColor = point_lights.lights[i].color.xyz;
        float intensity = point_lights.lights[i].color.w;

        vec3 L_point = normalize(lightPos - fragPos);
        float distance = length(lightPos - fragPos);
        float attenuation = 1.0 / (distance * distance); // Закон обратных квадратов

        // Diffuse
        float diff_point = max(dot(N, L_point), 0.0);
        vec3 diffuse_point = diff_point * lightColor * intensity * attenuation * model_ubo.albedo.xyz;

        // Specular
        vec3 H_point = normalize(L_point + V);
        float spec_point = pow(max(dot(N, H_point), 0.0), shininess);
        vec3 specular_point = spec_point * lightColor * intensity * attenuation * model_ubo.specularColor.xyz;

        result += diffuse_point + specular_point;
    }

    result *= fragColor;
    outColor = vec4(result, 1.0);
}