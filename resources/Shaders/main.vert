#version 450

layout(set = 0, binding = 0) uniform UniformBufferObject {
    vec2 foo;
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(set = 0, binding = 1) uniform Test {
    vec3 col;
} colorTest;



// layout(set = 0, binding = 2) uniform Test2 {
//     float ok;
// } testfloat;

layout( push_constant ) uniform constants
{
    float time;
} PushConstants;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition * cos(PushConstants.time), 1.0);
    fragColor = colorTest.col;
    fragTexCoord = inTexCoord;
}
