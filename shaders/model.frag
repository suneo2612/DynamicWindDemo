#version 430 core
in vec3 worldPosition;
in vec3 worldNormal;
in vec2 texcoord;
uniform vec3 baseColor;
uniform sampler2D baseTexture;
uniform bool hasTexture;
uniform bool lightingEnabled;
uniform vec3 lightDirection;
uniform vec3 cameraPosition;
uniform float roughness;
uniform float metallic;
out vec4 color;
void main()
{
    vec3 base = baseColor * (hasTexture ? texture(baseTexture, texcoord).rgb : vec3(1.0));
    if (!lightingEnabled) {
        color = vec4(base, 1.0);
        return;
    }
    float normalLength = length(worldNormal);
    if (normalLength < 1e-8) {
        color = vec4(base * 0.2, 1.0);
        return;
    }
    vec3 n = worldNormal / normalLength;
    vec3 l = -normalize(lightDirection);
    vec3 delta = cameraPosition - worldPosition;
    vec3 v = length(delta) > 1e-8 ? normalize(delta) : n;
    vec3 halfVector = l + v;
    vec3 h = length(halfVector) > 1e-8 ? normalize(halfVector) : n;
    float ndotl = max(dot(n, l), 0.0);
    float r = clamp(roughness, 0.04, 1.0);
    float m = clamp(metallic, 0.0, 1.0);
    float power = max(2.0 / (r * r) - 2.0, 1.0);
    vec3 specularColor = mix(vec3(0.04), base, m);
    vec3 specular = specularColor * pow(max(dot(n, h), 0.0), power) * ndotl;
    color = vec4(base * (0.2 + (1.0 - m) * ndotl) + specular, 1.0);
}
