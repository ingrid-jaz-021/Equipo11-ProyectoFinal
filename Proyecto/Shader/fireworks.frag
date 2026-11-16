#version 330 core
in vec4 particleColor;
out vec4 FragColor;
void main(){vec2 p=gl_PointCoord*2-1;float r=dot(p,p);if(r>=1)discard;float glow=(1-r)*(1-r);vec3 color=mix(particleColor.rgb,vec3(1),.55*exp(-r*16));FragColor=vec4(color*1.6,particleColor.a*glow);}
