uniform float textIntensity;

in vec4 position;
in vec2 texcoord;
in vec3 color;

noperspective out vec2 varTexcoord;
flat          out vec3 varColor;

void main()
{
  gl_Position = position;
  varTexcoord = texcoord + 0.5;
  varColor    = color * textIntensity;
}
