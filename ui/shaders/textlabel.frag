uniform sampler2D labelTexture;

noperspective in vec2 interpTexcoord;
flat          in vec3 interpColor;

out vec4 outputColor;

void main()
{
  float shadow = texture(labelTexture, interpTexcoord).r;
  float text   = textureOffset(labelTexture, interpTexcoord, ivec2(1, 1)).r;
  float alpha  = max(text, shadow);

  outputColor = vec4(interpColor * text, alpha);
}
