#version 150

uniform mat4 modelToCameraMatrix;
uniform mat4 cameraToClipMatrix;

const mat3x2 texShear = mat3x2( 0.474773,  -0.00168634,
                                0.0146367,  0.0145917,
                               -0.0012365, -0.474773);
in vec3 position;
in vec3 normal;

smooth out vec3 interpPosition;
smooth out vec3 interpNormal;
smooth out vec2 interpTexcoord;

void main()
{
  vec4 posCamSpace  = modelToCameraMatrix * vec4(position, 1.0);
  vec4 normCamSpace = modelToCameraMatrix * vec4(normal, 0.0);

  interpPosition = posCamSpace.xyz;
  interpNormal   = normCamSpace.xyz;
  interpTexcoord = texShear * position + 0.74;

  gl_Position = cameraToClipMatrix * posCamSpace;
}
