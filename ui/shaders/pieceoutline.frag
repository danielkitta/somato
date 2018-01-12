#if defined(GL_ES) && __VERSION__ < 320
# extension GL_EXT_shader_io_blocks : require
#endif
precision mediump float;

uniform vec4 diffuseColor;

in Varying {
  smooth        mediump vec3 halfVec;
  smooth        mediump vec3 normal;
  noperspective mediump vec3 edgeDist;
} var;

out vec4 outputColor;

const vec3  dirToLight     = vec3(0., 0.242535625, 0.9701425);
const float lightIntensity = 0.8;
const float ambIntensity   = 0.05;
const float specIntensity  = 0.04;
const float shininess      = 12.;
const vec3  edgeColor      = vec3(0.02);

float minComponent(vec3 v)
{
  return min(min(v.x, v.y), v.z);
}

void main()
{
  float rmagNorm = inversesqrt(dot(var.normal, var.normal));
  float rmagHalf = inversesqrt(dot(var.halfVec, var.halfVec));

  float cosLight = clamp(dot(var.normal, dirToLight) * rmagNorm, 0., 1.);
  float cosHalf  = clamp(dot(var.normal, var.halfVec) * rmagNorm * rmagHalf, 0., 1.);

  float specular = pow(cosHalf, shininess) * specIntensity * cosLight;
  float diffuse  = lightIntensity * cosLight + ambIntensity;

  float edgeDist = smoothstep(0., 1., minComponent(var.edgeDist));
  vec3  interior = diffuseColor.rgb * diffuse + specular;

  outputColor = vec4(mix(edgeColor, interior, edgeDist), 1.);
}
