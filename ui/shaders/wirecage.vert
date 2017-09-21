const float wirecageIntensity = 0.3;

uniform mat4 modelToCameraMatrix;
uniform mat4 cameraToClipMatrix;

in vec3 position;

smooth out float interpIntensity;

void main()
{
  vec4 posCamSpace = modelToCameraMatrix * vec4(position, 1.0);

  gl_Position = cameraToClipMatrix * posCamSpace;
  interpIntensity = clamp(0.08 * posCamSpace.z + 1.0, 0.0, 1.0) * wirecageIntensity;
}
