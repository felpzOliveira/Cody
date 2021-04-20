#version 450

layout(location=0) in vec3 position;
layout(location=1) in vec2 tex;

uniform mat4 modelView;
uniform mat4 projection;

out vec2 texCoords;
flat out int mid;

void main(){
    mid = int(position.z);
    texCoords = tex;
    gl_Position = projection * modelView * vec4(vec2(position.xy), 0.0, 1.0);
}
