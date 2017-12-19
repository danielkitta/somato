#if defined(GL_ES) && __VERSION__ < 320
# extension GL_OES_geometry_shader : require
#endif

layout(triangles) in;
layout(triangle_strip, max_vertices=3) out;

in Vertex {
  vec2 winPos;
  vec3 halfVec;
  vec3 normal;
} v_in[3];

out Varying {
  smooth        vec3 halfVec;
  smooth        vec3 normal;
  noperspective vec3 edgeDist;
} var;

float triangleHeight(float area2, vec2 edge)
{
  return abs(area2) * inversesqrt(dot(edge, edge));
}

void main()
{
  vec2 ab = v_in[1].winPos - v_in[0].winPos;
  vec2 ac = v_in[2].winPos - v_in[0].winPos;
  vec2 bc = v_in[2].winPos - v_in[1].winPos;

  float area2 = ab.x * ac.y - ab.y * ac.x;

  gl_Position  = gl_in[0].gl_Position;
  var.halfVec  = v_in[0].halfVec;
  var.normal   = v_in[0].normal;
  var.edgeDist = vec3(triangleHeight(area2, bc), 0., 0.);
  EmitVertex();

  gl_Position  = gl_in[1].gl_Position;
  var.halfVec  = v_in[1].halfVec;
  var.normal   = v_in[1].normal;
  var.edgeDist = vec3(0., triangleHeight(area2, ac), 0.);
  EmitVertex();

  gl_Position  = gl_in[2].gl_Position;
  var.halfVec  = v_in[2].halfVec;
  var.normal   = v_in[2].normal;
  var.edgeDist = vec3(0., 0., triangleHeight(area2, ab));
  EmitVertex();
}
