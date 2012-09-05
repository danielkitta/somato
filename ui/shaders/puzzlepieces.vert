#version 150

uniform mat4 modelToCameraMatrix;
uniform mat4 cameraToClipMatrix;

in vec3 position;
in vec3 normal;

smooth out vec3 interpPosition;
smooth out vec3 interpNormal;
smooth out vec2 interpTexcoord;

void main()
{
  vec4 posCamSpace  = modelToCameraMatrix * vec4(position, 1.0);
  vec4 normCamSpace = modelToCameraMatrix * vec4(normal, 0.0);

  interpTexcoord = vec2(0.5, -0.5) * position.xz + 0.75;
  interpPosition = posCamSpace.xyz;
  interpNormal   = normCamSpace.xyz;

  gl_Position = cameraToClipMatrix * posCamSpace;
}
