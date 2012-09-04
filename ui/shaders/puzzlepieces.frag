#version 150

uniform sampler2D pieceTexture;
uniform vec4 diffuseMaterial;

const vec3 dirToLight        = vec3(0.0, 0.242535625, 0.9701425);
const vec4 lightIntensity    = vec4(0.8, 0.8, 0.8, 0.0);
const vec4 ambientIntensity  = vec4(0.25, 0.25, 0.25, 1.0);
const vec4 specularIntensity = vec4(0.1, 0.1, 0.1, 0.0);

smooth in vec3 interpPosition;
smooth in vec3 interpNormal;
smooth in vec2 interpTexcoord;

out vec4 outputColor;

void main()
{
  vec3 normNormal = normalize(interpNormal);
  float cosAngIncidence = clamp(dot(normNormal, dirToLight), 0.0, 1.0);

  vec3 viewDir = normalize(interpPosition);
  vec3 halfVec = normalize(dirToLight - viewDir);

  float blinnTerm = pow(clamp(dot(normNormal, halfVec), 0.0, 1.0), 32);
  float specularReflection = (cosAngIncidence != 0.0) ? blinnTerm : 0.0;

  vec4 diffuseTerm = diffuseMaterial * (lightIntensity * cosAngIncidence + ambientIntensity);
  vec4 texColor = texture(pieceTexture, interpTexcoord);

  outputColor = texColor * diffuseTerm + specularIntensity * specularReflection;
}
