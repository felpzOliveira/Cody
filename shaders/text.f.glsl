#version 120
uniform sampler2D diffuse;

varying vec2 interpolatedTexCoord;
varying vec4 interpolatedColor;

uniform int enable_contrast = 0;

mat4 brightnessMatrix(float brightness){
    return mat4(1, 0, 0, 0,
                0, 1, 0, 0,
                0, 0, 1, 0,
                brightness, brightness, brightness, 1);
}

mat4 contrastMatrix(float contrast){
	float t = (1.0 - contrast) / 2.0;
    return mat4(contrast, 0, 0, 0,
                0, contrast, 0, 0,
                0, 0, contrast, 0,
                t, t, t, 1);
}

mat4 saturationMatrix(float saturation){
    vec3 luminance = vec3( 0.3086, 0.6094, 0.0820 );
    float oneMinusSat = 1.0 - saturation;
    vec3 red = vec3( luminance.x * oneMinusSat );
    red+= vec3( saturation, 0, 0 );
    vec3 green = vec3( luminance.y * oneMinusSat );
    green += vec3( 0, saturation, 0 );
    vec3 blue = vec3( luminance.z * oneMinusSat );
    blue += vec3( 0, 0, saturation );
    return mat4(red,     0,
                green,   0,
                blue,    0,
                0, 0, 0, 1 );
}


const float brightness = 0.15;
const float contrast = 0.8;
const float saturation = 1.5;

void main() {
    float alpha = texture2D(diffuse, interpolatedTexCoord).a;
    vec4 textColor = clamp(interpolatedColor, 0.0, 1.0);
    // Premultiplied alpha.
    vec4 col = vec4(textColor.rgb * textColor.a, textColor.a) * alpha;
    if(enable_contrast < -100){
        gl_FragColor = brightnessMatrix( brightness ) *
                       contrastMatrix( contrast ) * 
                       saturationMatrix( saturation ) * col;
    }else{
        alpha = col.a;
        //col *= 1.3;
        gl_FragColor = vec4(col.rgb, alpha);
    }
}
