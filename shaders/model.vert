#version 430 core
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;
uniform mat4 world;
uniform mat4 node;
uniform mat4 view;
uniform mat4 projection;
out vec3 worldPosition;
out vec3 worldNormal;
out vec2 texcoord;
void main()
{
    mat4 model = world * node;
    vec4 p = model * vec4(position, 1.0);
    mat3 basis = mat3(model);
    float det = determinant(basis);
    worldNormal = abs(det) > 1e-10 ? transpose(inverse(basis)) * normal : vec3(0.0);
    worldPosition = p.xyz;
    texcoord = vec2(uv.x, 1.0 - uv.y);
    gl_Position = projection * view * p;
}
