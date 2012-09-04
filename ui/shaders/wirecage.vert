#version 150

const vec4 wireframeColor = vec4(0.3, 0.3, 0.3, 1.0);

uniform mat4 modelToCameraMatrix;
uniform mat4 cameraToClipMatrix;

in vec3 position;

smooth out vec4 interpColor;

void main()
{
  vec4 posCamSpace = modelToCameraMatrix * vec4(position, 1.0);

  gl_Position = cameraToClipMatrix * posCamSpace;
  interpColor = clamp(0.1 * (posCamSpace.z + 11.5), 0.0, 1.0) * wireframeColor;
}
