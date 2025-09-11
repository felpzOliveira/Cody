#version 330
uniform mat4 modelView;
uniform mat4 projection;

layout(location=0) in vec2 vertexPosition;

out vec2 fragCoord;

void main() {
    fragCoord = vertexPosition;
    gl_Position = projection * modelView * vec4(vertexPosition, 0.0, 1.0);
}