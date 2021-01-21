uniform mat4 modelView;
uniform mat4 projection;

attribute vec4 vertexPosition;

varying vec2 interpolatedTexCoord;
varying vec4 interpolatedColor;

void main() {
    gl_Position = projection * modelView * vertexPosition;
}
