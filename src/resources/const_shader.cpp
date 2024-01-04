/* Auto generated file ( Jan  4 2024 14:52:07 ) */
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
ivec2 sample_size(float fmid){
    ivec2 iSize;
    if(fmid < 0.5) iSize = textureSize(image0, 0);
    else if(fmid > 0.5 && fmid < 1.5) iSize = textureSize(image1, 0);
    else if(fmid > 1.5 && fmid < 2.5) iSize = textureSize(image2, 0);
    else if(fmid > 2.5 && fmid < 3.5) iSize = textureSize(image3, 0);
    else if(fmid > 3.5 && fmid < 4.5) iSize = textureSize(image4, 0);
    else if(fmid > 4.5 && fmid < 5.5) iSize = textureSize(image5, 0);
    else if(fmid > 5.5 && fmid < 6.5) iSize = textureSize(image6, 0);
    else if(fmid > 6.5 && fmid < 7.5) iSize = textureSize(image7, 0);
    else iSize = ivec2(1,1);
    return iSize;
}
vec4 sample_texture(vec2 uv, float fmid){
    vec4 color;
    if(fmid < 0.5) color = texture2D(image0, uv);
    else if(fmid > 0.5 && fmid < 1.5) color = texture2D(image1, uv);
    else if(fmid > 1.5 && fmid < 2.5) color = texture2D(image2, uv);
    else if(fmid > 2.5 && fmid < 3.5) color = texture2D(image3, uv);
    else if(fmid > 3.5 && fmid < 4.5) color = texture2D(image4, uv);
    else if(fmid > 4.5 && fmid < 5.5) color = texture2D(image5, uv);
    else if(fmid > 5.5 && fmid < 6.5) color = texture2D(image6, uv);
    else if(fmid > 6.5 && fmid < 7.5) color = texture2D(image7, uv);
    else color = vec4(vec3(fmid), 1.0);
    return color;
}
void main(){
    // I swear to god nvidia fix your godamn switch, this is freaking terrible
    float fmid = float(mid);
    ivec2 size = sample_size(fmid);
    vec2 invSize = vec2(1.0 / float(size.x), 1.0 / float(size.y));
    float blurAmount = 0.0;
    vec4 sum = vec4(0.0);
    vec4 val = sample_texture(texCoords, fmid);
    for(float i = -blurAmount; i <= blurAmount; i += 1.0){
        for(float j = -blurAmount; j <= blurAmount; j += 1.0){
            vec2 offset = vec2(i, j) * invSize;
            sum += sample_texture(texCoords + offset, fmid);
        }
    }
    vec4 blurredColor = sum / ((2.0 * blurAmount + 1.0) * (2.0 * blurAmount + 1.0));
    blurredColor.a = val.a;
    fragColor = blurredColor;
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

