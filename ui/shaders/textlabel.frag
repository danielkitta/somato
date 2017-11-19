#ifdef GL_ARB_texture_gather
# extension GL_ARB_texture_gather : require
#endif

uniform sampler2D labelTexture;

noperspective in vec2 interpTexcoord;
flat          in vec3 interpColor;

out vec4 outputColor;

void main()
{
#ifdef GL_ARB_texture_gather
  vec4 textQuad = textureGather(labelTexture, interpTexcoord);
  vec2 shadow = max(textQuad.rg, textQuad.ba);
  float alpha = max(shadow.r, shadow.g);
  float text  = textQuad.g;
#else
  float shadow = texture(labelTexture, interpTexcoord).r;
  float text   = textureOffset(labelTexture, interpTexcoord, ivec2(1, 1)).r;
  float alpha  = max(text, shadow);
#endif
  outputColor = vec4(interpColor * text, alpha);
}
