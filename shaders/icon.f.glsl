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
