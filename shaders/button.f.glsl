#version 330
in vec4 interpolatedColor;
out vec4 fragColor;
void main(){
    vec3 color = interpolatedColor.rgb;
    fragColor = vec4(color, interpolatedColor.a);
}

