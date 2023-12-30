#version 120
varying vec4 interpolatedColor;

void main(){
    vec3 color = interpolatedColor.rgb;
    gl_FragColor = vec4(color, interpolatedColor.a);
}

