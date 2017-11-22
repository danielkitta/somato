uniform mat4 modelToCameraMatrix;
uniform mat4 cameraToClipMatrix;

in vec3 position;

smooth out float interpIntensity;

const float gridIntensity = 0.3;

void main()
{
  vec4 posCamSpace = modelToCameraMatrix * vec4(position, 1.);
  float fadeIntensity = clamp(0.08 * posCamSpace.z + 1., 0., 1.);

  gl_Position = cameraToClipMatrix * vec4(posCamSpace.xyz, 1.);
  interpIntensity = fadeIntensity * gridIntensity;
}
