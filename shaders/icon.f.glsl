#version 450

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
uniform float blurIntensity = 0.0;

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
    float blurAmount = blurIntensity;
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
