
uniform mat4 modelView;
uniform mat4 projection;

attribute vec4 vertexPosition;
attribute vec2 vertexTexCoord;
attribute vec4 vertexColor;

varying vec4 interpolatedColor;
varying vec2 coords;

void main(){
    coords = vertexPosition.xy;
    interpolatedColor = vertexColor;
    gl_Position = projection * modelView * vertexPosition;
}
