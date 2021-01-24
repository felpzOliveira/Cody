uniform mat4 modelView;
uniform mat4 projection;

attribute vec4 vertexPosition;
attribute vec4 vertexColor;

varying vec4 interpolatedColor;

void main() {
    interpolatedColor = vertexColor;
    gl_Position = projection * modelView * vertexPosition;
}
