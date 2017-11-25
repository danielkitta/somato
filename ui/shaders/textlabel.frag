#ifdef GL_ARB_texture_gather
# extension GL_ARB_texture_gather : require
#endif
#ifdef GL_ES
precision mediump float;
#endif

uniform sampler2D labelTexture;

noperspective in vec2 varTexcoord;
flat          in vec3 varColor;

out vec4 outputColor;

void main()
{
#if __VERSION__ >= 400 || (defined(GL_ES) && __VERSION__ >= 310) || defined(GL_ARB_texture_gather)
  vec4 textQuad = textureGather(labelTexture, varTexcoord);
  vec2 shadow = max(textQuad.rg, textQuad.ba);
  float alpha = max(shadow.r, shadow.g);
  float text  = textQuad.g;
#else
  float shadow = texture(labelTexture, varTexcoord).r;
  float text   = textureOffset(labelTexture, varTexcoord, ivec2(1, 1)).r;
  float alpha  = max(text, shadow);
#endif
  outputColor = vec4(varColor * text, alpha);
}
