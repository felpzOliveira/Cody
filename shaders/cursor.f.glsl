#version 330

in vec4 interpolatedColor;
out vec4 fragColor;
void main(){
    fragColor = interpolatedColor;
}
