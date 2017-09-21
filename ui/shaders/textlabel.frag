uniform sampler2D labelTexture;
uniform vec4 textColor;

noperspective in vec2 interpTexcoord;

out vec4 outputColor;

void main()
{
  ivec2 texPos = ivec2(interpTexcoord);
  vec4 text = texelFetchOffset(labelTexture, texPos, 0, ivec2(1, 1));
  vec4 shadow = texelFetch(labelTexture, texPos, 0);
  float alpha = max(text.r, shadow.r);

  outputColor = textColor * vec4(text.rrr, alpha);
}
