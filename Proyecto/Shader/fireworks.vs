#version 330 core
layout(location=0) in vec3 position;
layout(location=1) in vec4 color;
layout(location=2) in float size;
uniform mat4 view;
uniform mat4 projection;
uniform float viewportHeight;
out vec4 particleColor;
void main(){vec4 eye=view*vec4(position,1);gl_Position=projection*eye;gl_PointSize=clamp(size*viewportHeight*projection[1][1]/max(.1,-eye.z),1,64);particleColor=color;}
