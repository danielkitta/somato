#version 150

uniform sampler2DRect labelTexture;
uniform vec4 textColor;

smooth in vec2 interpTexcoord;

out vec4 outputColor;

void main()
{
  float text = textureOffset(labelTexture, interpTexcoord, ivec2(1, -1)).r;
  float shadow = texture(labelTexture, interpTexcoord).r;
  float alpha = max(text, shadow);

  outputColor = textColor * vec4(text, text, text, alpha);
}
