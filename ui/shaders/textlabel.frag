uniform sampler2D labelTexture;
uniform vec4 textColor;

noperspective in vec2 interpTexcoord;

out vec4 outputColor;

void main()
{
  float shadow = texelFetch(labelTexture, ivec2(interpTexcoord), 0).r;
  vec4 text = texelFetchOffset(labelTexture, ivec2(interpTexcoord), 0, ivec2(1, 1));
  float alpha = max(text.r, shadow);

  outputColor = textColor * vec4(text.rrr, alpha);
}
