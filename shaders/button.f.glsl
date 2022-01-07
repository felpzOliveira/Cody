#version 120
#if 0
    uniform vec2 buttonResolution = vec2(600, 400);
    uniform vec4 backgroundColor = vec4(0);
    varying vec2 coords;
#endif

varying vec4 interpolatedColor;

void main(){
#if 0
    float maxv = pow(.2, 4.0);
    vec2 pos = coords / buttonResolution;
    float vig = pos.x * pos.y * (1.0 - pos.x) * (1.0 - pos.y);
    float fv = smoothstep(0.0, maxv, vig);
    vec3 color = mix(interpolatedColor.rgb, backgroundColor.rgb, 1.0 - fv);
    gl_FragColor = vec4(color, 0.4 - fv);
#else
    vec3 color = interpolatedColor.rgb;
    gl_FragColor = vec4(color, 1.0);
#endif
}

