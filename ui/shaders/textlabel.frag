#version 150

uniform sampler2DRect labelTexture;
uniform vec4 textColor;

smooth in vec2 interpTexcoord;

out vec4 outputColor;

void main()
{
  vec4 text = textureOffset(labelTexture, interpTexcoord, ivec2(1, -1));
  vec4 shadow = texture(labelTexture, interpTexcoord);
  float alpha = max(text.a, shadow.a);

  outputColor = textColor * vec4(text.rgb, alpha);
}
