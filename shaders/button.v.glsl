#version 330
uniform mat4 modelView;
uniform mat4 projection;

layout(location=0) in vec4 vertexPosition;
layout(location=1) in vec2 vertexTexCoord;
layout(location=2) in vec4 vertexColor;

out vec4 interpolatedColor;
out vec2 coords;

void main(){
    coords = vertexPosition.xy;
    interpolatedColor = vertexColor;
    gl_Position = projection * modelView * vertexPosition;
}
