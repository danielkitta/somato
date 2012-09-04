#version 150

uniform sampler2D pieceTexture;

in vec2 interpTexcoord;
smooth in vec4 interpColor;
smooth in vec4 interpSpecular;

out vec4 outputColor;

void main()
{
  outputColor = texture(pieceTexture, interpTexcoord) * interpColor + interpSpecular;
}
