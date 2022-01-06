#version 120
uniform float stretchPowValue = 0;
uniform vec2 buttonResolution = vec2(600, 400);
uniform vec4 backgroundColor = vec4(0);

varying vec4 interpolatedColor;
varying vec2 coords;

void main(){
    float maxv = stretchPowValue;
    maxv = pow(.2, 4.0);
    vec2 pos = coords / buttonResolution;
    float vig = pos.x * pos.y * (1.0 - pos.x) * (1.0 - pos.y);
    float fv = smoothstep(0.0, maxv, vig);
    vec3 color = mix(interpolatedColor.rgb, backgroundColor.rgb, 1.0 - fv);
    gl_FragColor = vec4(color, 0.4 - fv);
}

