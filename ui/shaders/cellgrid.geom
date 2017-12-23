#if defined(GL_ES) && __VERSION__ < 320
# extension GL_EXT_geometry_shader : require
#endif

layout(lines) in;
layout(triangle_strip, max_vertices=4) out;

uniform vec4 pixelScale;

in Vertex {
  vec2  devCoord;
  float intensity;
} v_in[2];

out Varying {
  smooth mediump float intensity;
} var;

void emitVertexPair(vec4 pos, vec2 shift, float intensity)
{
  gl_Position   = pos + pos.w * vec4(shift, 0., 0.);
  var.intensity = intensity;
  EmitVertex();

  gl_Position   = pos - pos.w * vec4(shift, 0., 0.);
  var.intensity = intensity;
  EmitVertex();
}

void main()
{
  vec2 dir   = v_in[1].devCoord - v_in[0].devCoord;
  vec2 shift = pixelScale.zw * normalize(pixelScale.xy * dir).yx;

  emitVertexPair(gl_in[0].gl_Position, shift, v_in[0].intensity);
  emitVertexPair(gl_in[1].gl_Position, shift, v_in[1].intensity);
}
