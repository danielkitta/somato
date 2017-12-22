#if defined(GL_ES) && __VERSION__ < 320
# extension GL_EXT_shader_io_blocks : require
#endif
#ifdef GL_ES
precision mediump float;
#endif

uniform vec4 diffuseMaterial;

in Varying {
  smooth        vec3 halfVec;
  smooth        vec3 normal;
  noperspective vec3 edgeDist;
} var;

out vec3 outputColor;

const vec3  dirToLight     = vec3(0., 0.242535625, 0.9701425);
const float lightIntensity = 0.7;
const float ambIntensity   = 0.2;
const float specIntensity  = 0.1;
const float shininess      = 32.;
const vec3  edgeColor      = vec3(0.1);

float inverseLength(vec3 v)
{
  return inversesqrt(dot(v, v));
}

float minComponent(vec3 v)
{
  return min(min(v.x, v.y), v.z);
}

void main()
{
  float rMagNormal   = inverseLength(var.normal);
  float rMagHalfVec  = inverseLength(var.halfVec);
  float dotNormLight = dot(var.normal, dirToLight);
  float dotNormHalf  = dot(var.normal, var.halfVec);
  float cosIncidence = clamp(dotNormLight * rMagNormal, 0., 1.);
  float cosHalfIncid = clamp(dotNormHalf * rMagNormal * rMagHalfVec, 0., 1.);

  float specHighlight = pow(cosHalfIncid, shininess);
  float specularTerm  = specHighlight * specIntensity * cosIncidence;
  float diffuseTerm   = lightIntensity * cosIncidence + ambIntensity;

  float edgeFactor = smoothstep(0., 1., minComponent(var.edgeDist));
  vec3  innerColor = diffuseMaterial.rgb * diffuseTerm + specularTerm;

  outputColor = mix(edgeColor, innerColor, edgeFactor);
}
