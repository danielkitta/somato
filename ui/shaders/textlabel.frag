uniform sampler2D labelTexture;
uniform vec4 textColor;

noperspective in vec2 interpTexcoord;

out vec4 outputColor;

void main()
{
  float shadow = texture(labelTexture, interpTexcoord).r;
  vec4  text   = textureOffset(labelTexture, interpTexcoord, ivec2(1, 1));
  float alpha  = max(text.r, shadow);

  outputColor = textColor * vec4(text.rrr, alpha);
}
