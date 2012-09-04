#version 150

uniform mat4 modelToCameraMatrix;
uniform mat4 cameraToClipMatrix;
uniform vec4 diffuseMaterial;

const vec3 dirToLight = vec3(0.0, 0.242535625, 0.9701425);
const vec4 lightIntensity    = vec4(0.8, 0.8, 0.8, 0.0);
const vec4 ambientIntensity  = vec4(0.25, 0.25, 0.25, 1.0);
const vec4 specularIntensity = vec4(0.1, 0.1, 0.1, 0.0);

in vec3 position;
in vec3 normal;

out vec2 interpTexcoord;
smooth out vec4 interpColor;
smooth out vec4 interpSpecular;

void main()
{
  vec4 posCamSpace = modelToCameraMatrix * vec4(position, 1.0);
  interpTexcoord = vec2(0.5, -0.5) * position.xz + vec2(0.75);

  vec4 normCamSpace = modelToCameraMatrix * vec4(normal, 0.0);
  float cosAngIncidence = clamp(dot(normCamSpace.xyz, dirToLight), 0.0, 1.0);

  vec3 viewDir = normalize(-posCamSpace.xyz);
  vec3 halfVec = normalize(dirToLight + viewDir);
  float blinnTerm = pow(clamp(dot(normCamSpace.xyz, halfVec), 0.0, 1.0), 64);

  float specularReflection = (cosAngIncidence != 0.0) ? blinnTerm : 0.0;

  gl_Position = cameraToClipMatrix * posCamSpace;
  interpColor = diffuseMaterial * (lightIntensity * cosAngIncidence + ambientIntensity);
  interpSpecular = specularIntensity * specularReflection;
}
