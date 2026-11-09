#version 330 core
in vec3 SurfaceNormal;
out vec4 FragColor;
uniform vec3 viewerDirection;
float hash(vec3 p){return fract(sin(dot(p,vec3(127.1,311.7,74.7)))*43758.5453);}
float noise(vec3 p)
{
    vec3 c=floor(p),f=fract(p);f=f*f*(3.0-2.0*f);
    return mix(mix(mix(hash(c),hash(c+vec3(1,0,0)),f.x),mix(hash(c+vec3(0,1,0)),hash(c+vec3(1,1,0)),f.x),f.y),
               mix(mix(hash(c+vec3(0,0,1)),hash(c+vec3(1,0,1)),f.x),mix(hash(c+vec3(0,1,1)),hash(c+vec3(1,1,1)),f.x),f.y),f.z);
}
void main()
{
    vec3 n=normalize(SurfaceNormal);
    float maria=smoothstep(.32,.73,noise(n*5.2));
    vec3 albedo=mix(vec3(.94,.95,.98),vec3(.51,.56,.64),maria*.55);
    albedo*=.92+.08*noise(n*64.0);
    // Crateres fijos sobre la esfera, sin textura externa ni animacion orbital.
    for(int i=0;i<18;++i)
    {
        float seed=float(i)+1.0;
        vec3 center=normalize(vec3(hash(vec3(seed,2,3)),hash(vec3(3,seed,8)),hash(vec3(9,4,seed)))*2.0-1.0);
        float r=.035+.105*hash(vec3(seed,5,7)),d=length(n-center);
        float pit=1.0-smoothstep(0.0,r,d);
        float rim=smoothstep(r*.72,r*.94,d)*(1.0-smoothstep(r,r*1.18,d));
        albedo*=1.0-.25*pit;albedo+=vec3(.11)*rim;
    }
    float face=max(dot(n,normalize(viewerDirection)),0.0);
    float limb=.48+.52*sqrt(face);
    // Emision propia: la Luna nunca depende de las farolas de la plaza.
    FragColor=vec4(albedo*limb*1.13,1.0);
}
