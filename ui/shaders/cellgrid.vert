uniform mat4 modelToCameraMatrix;
uniform vec4 viewFrustum;

in vec3 position;

smooth out float varIntensity;

const float gridIntensity = 0.3;

void main()
{
  vec4 posCamSpace = modelToCameraMatrix * vec4(position, 1.);
  float fadeIntensity = clamp(0.08 * posCamSpace.z + 1., 0., 1.);

  varIntensity = fadeIntensity * gridIntensity;
  gl_Position  = vec4(posCamSpace.xy * viewFrustum.xy,
                      posCamSpace.z  * viewFrustum.z + viewFrustum.w,
                     -posCamSpace.z);
}
