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

struct PointLight {
    vec4 position;
    vec4 color;
};
layout(set = 0, binding = 1) readonly buffer PointLights {
    PointLight lights[];
} point_lights;

struct SpotLight {
    vec4 position;
    vec4 direction;
    vec4 color;
    vec4 params;   // x = cos(inner), y = cos(outer)
};
layout(set = 0, binding = 2) readonly buffer SpotLights {
    SpotLight spotLights[];
} spot_lights;

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

    float shininess = model_ubo.albedo.w;

    //направленный свет
    vec3 L = normalize(-scene.lightDir.xyz);
    vec3 H = normalize(L + V);
    vec3 ambient = scene.ambientColor.xyz * model_ubo.albedo.xyz;
    float diff = max(dot(N, L), 0.0);
    vec3 diffuse = diff * scene.lightColor.xyz * scene.lightColor.w * model_ubo.albedo.xyz;
    float spec = pow(max(dot(N, H), 0.0), shininess);
    vec3 specular = spec * scene.lightColor.xyz * scene.lightColor.w * model_ubo.specularColor.xyz;
    vec3 result = ambient + diffuse + specular;

    //точечные источники
    for (int i = 0; i < point_lights.lights.length(); ++i) {
        vec3 lightPos = point_lights.lights[i].position.xyz;
        vec3 lightColor = point_lights.lights[i].color.xyz;
        float intensity = point_lights.lights[i].color.w;

        vec3 Lp = normalize(lightPos - fragPos);
        float distance = length(lightPos - fragPos);
        float attenuation = 1.0 / (distance * distance);

        float diffp = max(dot(N, Lp), 0.0);
        vec3 diffuseP = diffp * lightColor * intensity * attenuation * model_ubo.albedo.xyz;

        vec3 Hp = normalize(Lp + V);
        float specp = pow(max(dot(N, Hp), 0.0), shininess);
        vec3 specularP = specp * lightColor * intensity * attenuation * model_ubo.specularColor.xyz;

        result += diffuseP + specularP;
    }

    //прожекторы
    for (int i = 0; i < spot_lights.spotLights.length(); ++i) {
        vec3 spotPos = spot_lights.spotLights[i].position.xyz;
        vec3 spotDir = normalize(spot_lights.spotLights[i].direction.xyz);
        vec3 spotColor = spot_lights.spotLights[i].color.xyz;
        float intensity = spot_lights.spotLights[i].color.w;
        float cosInner = spot_lights.spotLights[i].params.x;
        float cosOuter = spot_lights.spotLights[i].params.y;

        vec3 Ls = normalize(spotPos - fragPos);
        float distance = length(spotPos - fragPos);
        float attenuation = 1.0 / (distance * distance);

        float theta = dot(-Ls, spotDir);
        float spotFactor = clamp((theta - cosOuter) / (cosInner - cosOuter), 0.0, 1.0);

        if (spotFactor > 0.0) {
            float diffs = max(dot(N, Ls), 0.0);
            vec3 diffuseS = diffs * spotFactor * spotColor * intensity * attenuation * model_ubo.albedo.xyz;

            vec3 Hs = normalize(Ls + V);
            float specs = pow(max(dot(N, Hs), 0.0), shininess);
            vec3 specularS = specs * spotFactor * spotColor * intensity * attenuation * model_ubo.specularColor.xyz;

            result += diffuseS + specularS;
        }
    }

    result *= fragColor;
    outColor = vec4(result, 1.0);
}