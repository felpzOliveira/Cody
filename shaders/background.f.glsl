#version 330

uniform vec4 primaryColor;
uniform vec4 secondaryColor;

in vec2 fragCoord;
out vec4 fragColor;

void main() {
    // Convert from normalized device coordinates [-1,1] to [0,1] range
    vec2 uv = (fragCoord + 1.0) * 0.5;
    
    // Create diagonal split from top-left to bottom-right
    // This creates two triangles: upper-left and lower-right
    float diagonal = uv.x + uv.y; // Ranges from 0 to 2
    
    // Use smoothstep to create a smooth but sharp transition between the two triangles
    // The transition occurs along the diagonal where x + y = 1
    // Use a much narrower range for a sharper edge
    float mixFactor = smoothstep(0.998, 1.002, diagonal);
    
    // Interpolate between the two colors
    fragColor = mix(primaryColor, secondaryColor, mixFactor);
}
