/* Auto generated file ( Jan  2 2024 09:38:59 ) */
#define MULTILINE_STRING(a) #a
#define MULTILINE_STRING_V(v, a) "#" #v "\n" #a


/****************************************************************/
// Instanced from /home/felipe/Documents/Cody/shaders/icon.f.glsl
/****************************************************************/
const char *shader_icon_f = MULTILINE_STRING_V(version 450,
uniform sampler2D image0;
uniform sampler2D image1;
uniform sampler2D image2;
uniform sampler2D image3;
uniform sampler2D image4;
uniform sampler2D image5;
uniform sampler2D image6;
uniform sampler2D image7;
in vec2 texCoords;
out vec4 fragColor;
flat in int mid;
void main(){
    // I swear to god nvidia fix your godamn switch, this is freaking terrible
    float fmid = float(mid);
    if(fmid < 0.5) fragColor = texture2D(image0, texCoords);
    else if(fmid > 0.5 && fmid < 1.5) fragColor = texture2D(image1, texCoords);
    else if(fmid > 1.5 && fmid < 2.5) fragColor = texture2D(image2, texCoords);
    else if(fmid > 2.5 && fmid < 3.5) fragColor = texture2D(image3, texCoords);
    else if(fmid > 3.5 && fmid < 4.5) fragColor = texture2D(image4, texCoords);
    else if(fmid > 4.5 && fmid < 5.5) fragColor = texture2D(image5, texCoords);
    else if(fmid > 5.5 && fmid < 6.5) fragColor = texture2D(image6, texCoords);
    else if(fmid > 6.5 && fmid < 7.5) fragColor = texture2D(image7, texCoords);
    else fragColor = vec4(vec3(fmid), 1.0);
    //float f = mid;
    //float ff = f / 2.0;
    //fragColor = fragColor * 0.01 + vec4(vec3(1.0 - float(mid)), 1.0);
    //fragColor = fragColor * 0.01 + vec4(vec3(ff), 1.0);
}
);

/******************************************************************/
// Instanced from /home/felipe/Documents/Cody/shaders/cursor.f.glsl
/******************************************************************/
const char *shader_cursor_f = MULTILINE_STRING_V(version 120,
varying vec4 interpolatedColor;
void main(){
    gl_FragColor = interpolatedColor;
}
);

/****************************************************************/
// Instanced from /home/felipe/Documents/Cody/shaders/text.f.glsl
/****************************************************************/
const char *shader_text_f = MULTILINE_STRING_V(version 120,
uniform sampler2D diffuse;
varying vec2 interpolatedTexCoord;
varying vec4 interpolatedColor;
uniform float brightness = 0.15;
uniform float contrast = 0.8;
uniform float saturation = 1.5;
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
void main() {
    float alpha = texture2D(diffuse, interpolatedTexCoord).a;
    vec4 textColor = clamp(interpolatedColor, 0.0, 1.0);
    // Premultiplied alpha.
    vec4 col = vec4(textColor.rgb * textColor.a, textColor.a) * alpha;
    gl_FragColor = brightnessMatrix( brightness ) *
                   contrastMatrix( contrast ) *
                   saturationMatrix( saturation ) * col;
}
);

/****************************************************************/
// Instanced from /home/felipe/Documents/Cody/shaders/text.v.glsl
/****************************************************************/
const char *shader_text_v = MULTILINE_STRING(
uniform mat4 modelView;
uniform mat4 projection;
attribute vec4 vertexPosition;
attribute vec2 vertexTexCoord;
attribute vec4 vertexColor;
varying vec2 interpolatedTexCoord;
varying vec4 interpolatedColor;
void main() {
    interpolatedColor = vertexColor;
    interpolatedTexCoord = vertexTexCoord;
    gl_Position = projection * modelView * vertexPosition;
}
);

/******************************************************************/
// Instanced from /home/felipe/Documents/Cody/shaders/button.v.glsl
/******************************************************************/
const char *shader_button_v = MULTILINE_STRING(
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
);

/******************************************************************/
// Instanced from /home/felipe/Documents/Cody/shaders/cursor.v.glsl
/******************************************************************/
const char *shader_cursor_v = MULTILINE_STRING(
uniform mat4 modelView;
uniform mat4 projection;
attribute vec4 vertexPosition;
attribute vec4 vertexColor;
varying vec4 interpolatedColor;
void main() {
    interpolatedColor = vertexColor;
    gl_Position = projection * modelView * vertexPosition;
}
);

/******************************************************************/
// Instanced from /home/felipe/Documents/Cody/shaders/button.f.glsl
/******************************************************************/
const char *shader_button_f = MULTILINE_STRING_V(version 120,
varying vec4 interpolatedColor;
void main(){
    vec3 color = interpolatedColor.rgb;
    gl_FragColor = vec4(color, interpolatedColor.a);
}
);

/****************************************************************/
// Instanced from /home/felipe/Documents/Cody/shaders/icon.v.glsl
/****************************************************************/
const char *shader_icon_v = MULTILINE_STRING_V(version 450,
layout(location=0) in vec3 position;
layout(location=1) in vec2 tex;
uniform mat4 modelView;
uniform mat4 projection;
out vec2 texCoords;
flat out int mid;
void main(){
    mid = int(position.z);
    texCoords = tex;
    gl_Position = projection * modelView * vec4(vec2(position.xy), 0.0, 1.0);
}
);

