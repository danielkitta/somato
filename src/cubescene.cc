/*
 * Copyright (c) 2004-2012  Daniel Elstner  <daniel.kitta@gmail.com>
 *
 * This file is part of Somato.
 *
 * Somato is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Somato is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Somato; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#define GL_GLEXT_PROTOTYPES 1

#include "cubescene.h"
#include "appdata.h"
#include "glsceneprivate.h"
#include "glutils.h"
#include "mathutils.h"
#include "meshloader.h"

#include <glib.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdkgl.h>
#include <glibmm.h>
#include <gdkmm.h>
#include <gtkmm/accelgroup.h>

#ifdef GDK_WINDOWING_WIN32
# include <windows.h>
#endif
#include <GL/gl.h>

#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <algorithm>
#include <functional>
#include <limits>

#include <config.h>

#undef near /* WTF? */
#undef far  /* Out, demon, out! */

namespace
{

using Somato::Cube;

/* The type of indices into the wireframe vertex array.
 * Although GLubyte would suffice to represent the range of indices, it is
 * not an optimal configuration with my current hardware (Radeon HD5670).
 */
typedef GLushort WireframeIndex;

enum
{
  WIREFRAME_INDEX_TYPE   = GL_UNSIGNED_SHORT,
  WIREFRAME_VERTEX_COUNT = (Cube::N + 1) * (Cube::N + 1) * (Cube::N + 1),
  WIREFRAME_LINE_COUNT   = (Cube::N + 1) * (Cube::N + 1) *  Cube::N * 3
};

enum
{
  MESH_INDEX_TYPE = GL_UNSIGNED_SHORT
};

/* Vertex shader input attribute locations.
 */
enum
{
  ATTRIB_POSITION = 0,
  ATTRIB_NORMAL   = 1
};

/* Index usage convention for arrays of buffer objects.
 */
enum
{
  VERTICES = 0,
  INDICES  = 1
};

/*
 * Special value used to mark tracked pointer coordinates as invalid.  This
 * constant is equivalent to the integer indeterminate value some machines
 * use to signal an error condition when converting from floating point to
 * integer.  Math::clamp_to_int() avoids this value.
 */
enum { TRACK_UNSET = G_MININT };

/*
 * Single-precision factor used for conversion of angles.
 */
static const float rad_per_deg = G_PI / 180.0;

/*
 * The time span, in seconds, to wait for further user input
 * before hiding the mouse cursor while the animation is running.
 */
static const float hide_cursor_delay = 5.0;

/*
 * Side length of a cube cell in unzoomed model units.
 */
static const float cube_cell_size = 1.0;

/*
 * View offset in the direction of the z-axis.
 */
static const float view_z_offset = -9.0;

/*
 * The angle by which to rotate if a keyboard navigation key is pressed.
 */
static const float rotation_step = 3.0;

/*
 * The materials applied to cube pieces.  Indices into the materials array
 * match the original piece order as passed to CubeScene::set_cube_pieces(),
 * reduced modulo the number of materials.
 */
static const std::array<GLfloat[4], 8> piece_materials
{{
  { 0.80, 0.15, 0.00, 1.0 }, // orange
  { 0.05, 0.60, 0.05, 1.0 }, // green
  { 0.80, 0.00, 0.00, 1.0 }, // red
  { 0.80, 0.50, 0.00, 1.0 }, // yellow
  { 0.10, 0.00, 0.80, 1.0 }, // blue
  { 0.60, 0.00, 0.80, 1.0 }, // lavender
  { 0.05, 0.45, 0.80, 1.0 }, // cyan
  { 0.80, 0.00, 0.25, 1.0 }  // pink
}};

/*
 * Find the direction from which a cube piece can be shifted into
 * its desired position, without colliding with any other piece
 * already in place (think Tetris).
 */
static
void find_animation_axis(Cube cube, Cube piece, float* direction)
{
  struct MovementData
  {
    // The axis index to be passed to Cube::shift() (X, Y or Z).
    unsigned char axis;

    // If true, pass the (actually fixed) cube rather than the piece to be
    // animated to Cube::shift().  As a result the piece appears to "move"
    // into the opposite direction, without the need to introduce a second
    // Cube::shift() method.
    bool backward;

    // The starting point of the animation relative to the final position
    // of the cube piece.  In other words, (x, y, z) is the reverse of the
    // vector describing the direction of movement.
    float x, y, z;
  };

  // Directions listed first are prefered.
  static const std::array<MovementData, 6> movement_data
  {{
    { Cube::AXIS_Y, false,  0.0,  1.0,  0.0 }, // top->down
    { Cube::AXIS_Z, true,   0.0,  0.0,  1.0 }, // front->back
    { Cube::AXIS_X, true,  -1.0,  0.0,  0.0 }, // left->right
    { Cube::AXIS_X, false,  1.0,  0.0,  0.0 }, // right->left
    { Cube::AXIS_Z, false,  0.0,  0.0, -1.0 }, // back->front
    { Cube::AXIS_Y, true,   0.0, -1.0,  0.0 }  // bottom->up
  }};

  for (const auto& movement : movement_data)
  {
    // Swap fixed and moving pieces if backward shifting is indicated.
    const Cube fixed  = (movement.backward) ? piece : cube;
    Cube       moving = (movement.backward) ? cube : piece;

    // Now do the shifting until the moving piece has either
    // vanished from view or collided with the fixed piece.
    do
    {
      if (moving == Cube{}) // if it vanished we have just found our solution
      {
        direction[0] = movement.x;
        direction[1] = movement.y;
        direction[2] = movement.z;
        return;
      }
      moving.shift(movement.axis, true);
    }
    while ((fixed & moving) == Cube{});
  }

  // This should not happen as long as the input is correct.
  g_return_if_reached();
}

} // anonymous namespace

namespace Somato
{

struct CubeScene::Extensions : public GL::Extensions
{
private:
  // noncopyable
  Extensions(const CubeScene::Extensions&) = delete;
  CubeScene::Extensions& operator=(const CubeScene::Extensions&) = delete;

  void query();

public:
  float max_anisotropy;
  bool  have_texture_filter_anisotropic;

  Extensions() { query(); }
  virtual ~Extensions();
};

CubeScene::Extensions::~Extensions()
{}

void CubeScene::Extensions::query()
{
  max_anisotropy = 1.0;
  have_texture_filter_anisotropic = false;

  if (GL::have_gl_extension("GL_EXT_texture_filter_anisotropic"))
  {
    have_texture_filter_anisotropic = true;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &max_anisotropy);
  }
}

inline
const CubeScene::Extensions* CubeScene::gl_ext() const
{
  return static_cast<const CubeScene::Extensions*>(GL::Scene::gl_ext());
}

CubeScene::CubeScene()
:
  piece_cells_            (Cube::N * Cube::N * Cube::N),
  heading_                {create_layout_texture()},
  footing_                {create_layout_texture()},

  uf_modelview_           {-1},
  uf_projection_          {-1},
  uf_diffuse_material_    {-1},
  uf_piece_texture_       {-1},

  cage_uf_modelview_      {-1},
  cage_uf_projection_     {-1},

  cube_texture_           {0},
  mesh_buffers_           {0, 0},
  wireframe_buffers_      {0, 0},
  pieces_vertex_array_    {0},
  cage_vertex_array_      {0},

  track_last_x_           {TRACK_UNSET},
  track_last_y_           {TRACK_UNSET},
  cursor_state_           {CURSOR_DEFAULT},

  animation_piece_        {0},
  exclusive_piece_        {0},
  animation_seek_         {1.0},
  animation_position_     {0.0},
  animation_delay_        {1.0 / 3.0},

  zoom_                   {1.0},
  frames_per_sec_         {60.0},
  pieces_per_sec_         {1.0},

  depth_order_changed_    {false},
  animation_running_      {false},
  show_wireframe_         {false},
  show_outline_           {false},
  zoom_visible_           {true}
{
  heading_->color().assign(0.85, 0.85, 0.85, 1.0);
  footing_->color().assign(0.65, 0.65, 0.65, 1.0);

  set_flags(Gtk::CAN_FOCUS);

  add_events(Gdk::BUTTON_PRESS_MASK | Gdk::BUTTON_RELEASE_MASK | Gdk::BUTTON1_MOTION_MASK
             | Gdk::POINTER_MOTION_MASK | Gdk::POINTER_MOTION_HINT_MASK | Gdk::KEY_PRESS_MASK
             | Gdk::KEY_RELEASE_MASK | Gdk::ENTER_NOTIFY_MASK | Gdk::VISIBILITY_NOTIFY_MASK);
}

CubeScene::~CubeScene()
{}

void CubeScene::set_heading(const Glib::ustring& heading)
{
  heading_->set_content(heading);

  if (heading_->need_update())
  {
    if (is_realized())
    {
      ScopeContext context {*this};

      gl_update_ui();
    }

    if (is_drawable())
      queue_draw();
  }
}

Glib::ustring CubeScene::get_heading() const
{
  return heading_->get_content();
}

void CubeScene::set_cube_pieces(const Solution& cube_pieces)
{
  try
  {
    cube_pieces_   .assign(cube_pieces.begin(), cube_pieces.end());
    animation_data_.assign(cube_pieces.size(), AnimationData{});
    depth_order_   .assign(cube_pieces.size(), 0);

    if (!cube_pieces.empty())
      update_animation_order();
  }
  catch (...)
  {
    // Even though we do not guarantee strong exception safety,
    // make sure the object is at least left in a sane state.

    pause_animation();

    depth_order_   .clear();
    animation_data_.clear();
    cube_pieces_   .clear();

    throw;
  }

  if (animation_running_ || animation_piece_ > int(cube_pieces.size()))
  {
    animation_piece_    = 0;
    animation_position_ = 0.0;
  }

  if (is_drawable())
    queue_draw();

  continue_animation();
}

void CubeScene::set_zoom(float zoom)
{
  const float value = Math::clamp(zoom, 0.125f, 8.0f);

  if (value != zoom_)
  {
    zoom_ = value;

    if (zoom_visible_)
      update_footing();

    if (is_realized())
    {
      ScopeContext context {*this};

      gl_update_projection();

      if (footing_->need_update())
        gl_update_ui();
    }

    if (is_drawable())
      queue_draw();
  }
}

float CubeScene::get_zoom() const
{
  return zoom_;
}

void CubeScene::set_rotation(const Math::Quat& rotation)
{
  rotation_ = rotation;
  rotation_.renormalize(4.0f * std::numeric_limits<float>::epsilon());

  depth_order_changed_ = true;

  if (!animation_data_.empty() && is_drawable())
    queue_draw();
}

Math::Quat CubeScene::get_rotation() const
{
  return rotation_;
}

void CubeScene::set_animation_delay(float animation_delay)
{
  animation_delay_ = Math::clamp(animation_delay, 0.0f, 1.0f);
}

float CubeScene::get_animation_delay() const
{
  return animation_delay_;
}

void CubeScene::set_frames_per_second(float frames_per_second)
{
  frames_per_sec_ = Math::clamp(frames_per_second, 1.0f, 100.0f);
}

float CubeScene::get_frames_per_second() const
{
  return frames_per_sec_;
}

void CubeScene::set_pieces_per_second(float pieces_per_second)
{
  const float value = Math::clamp(pieces_per_second, 0.01f, 100.0f);

  if (value != pieces_per_sec_)
  {
    pieces_per_sec_ = value;

    if (animation_position_ > 0.0f)
    {
      animation_seek_ = animation_position_;
      animation_timer_.start();
    }
  }
}

float CubeScene::get_pieces_per_second() const
{
  return pieces_per_sec_;
}

void CubeScene::set_animation_running(bool animation_running)
{
  if (animation_running != animation_running_)
  {
    animation_running_ = animation_running;

    if (animation_running_)
      continue_animation();
    else
      pause_animation();

    reset_hide_cursor_timeout();
  }
}

bool CubeScene::get_animation_running() const
{
  return animation_running_;
}

void CubeScene::set_zoom_visible(bool zoom_visible)
{
  if (zoom_visible != zoom_visible_)
  {
    zoom_visible_ = zoom_visible;

    update_footing();

    if (footing_->need_update())
    {
      if (is_realized())
      {
        ScopeContext context {*this};

        gl_update_ui();
      }

      if (is_drawable())
        queue_draw();
    }
  }
}

bool CubeScene::get_zoom_visible() const
{
  return zoom_visible_;
}

void CubeScene::set_show_wireframe(bool show_wireframe)
{
  if (show_wireframe != show_wireframe_)
  {
    show_wireframe_ = show_wireframe;

    if (is_realized())
    {
      ScopeContext context {*this};

      gl_update_wireframe();
    }

    if (is_drawable())
      queue_draw();
  }
}

bool CubeScene::get_show_wireframe() const
{
  return show_wireframe_;
}

void CubeScene::set_show_outline(bool show_outline)
{
  if (show_outline != show_outline_)
  {
    show_outline_ = show_outline;

    if (!animation_data_.empty() && is_drawable())
      queue_draw();
  }
}

bool CubeScene::get_show_outline() const
{
  return show_outline_;
}

int CubeScene::get_cube_triangle_count() const
{
  int cube_triangle_count = 0;

  for (const auto& mesh : mesh_data_)
    cube_triangle_count += mesh.triangle_count;

  return cube_triangle_count;
}

int CubeScene::get_cube_vertex_count() const
{
  int cube_vertex_count = 0;

  for (const auto& mesh : mesh_data_)
    cube_vertex_count += mesh.element_count();

  return cube_vertex_count;
}

/*
 * Rotate the camera around the given axis by an angle in degrees.
 */
void CubeScene::rotate(int axis, float angle)
{
  g_return_if_fail(axis >= Cube::AXIS_X && axis <= Cube::AXIS_Z);

  const Math::Vector4 axis_vec {Math::Matrix4::identity[axis]};
  set_rotation(Math::Quat::from_axis(axis_vec, angle * rad_per_deg) * rotation_);
}

void CubeScene::gl_initialize()
{
  if (!mesh_loader_)
  {
    std::unique_ptr<GL::MeshLoader> loader
      {new GL::MeshLoader{Util::locate_data_file("puzzlepieces.dae")}};

    loader->set_on_done(std::bind(&CubeScene::on_meshes_loaded, this));
    loader->run();

    mesh_loader_ = std::move(loader);
  }

  gl_create_piece_shader();
  gl_create_cage_shader();

  GL::Scene::gl_initialize();

  glEnable(GL_CULL_FACE);

  // Trade viewspace clipping for depth clamping to avoid highly visible
  // volume clipping artifacts.  The clamping could potentially produce some
  // artifacts of its own, but so far it appears to play along nicely.
  glEnable(GL_DEPTH_CLAMP);

  try // go on without texturing if loading the image fails
  {
    gl_init_cube_texture();
  }
  catch (const Glib::Error& error)
  {
    const Glib::ustring message = error.what();
    g_warning("%s", message.c_str());
  }
  catch (const GL::Error& error)
  {
    const Glib::ustring message = error.what();
    g_warning("GL error: %s", message.c_str());
  }

  piece_shader_.use();
  glUniform1i(uf_piece_texture_, 0);
  GL::ShaderProgram::unuse();

  gl_update_wireframe();
}

void CubeScene::gl_create_piece_shader()
{
  GL::ShaderProgram program;

  program.attach(GL::ShaderObject{GL_VERTEX_SHADER,
                                  Util::locate_shader_file("puzzlepieces.vert")});
  program.attach(GL::ShaderObject{GL_FRAGMENT_SHADER,
                                  Util::locate_shader_file("puzzlepieces.frag")});

  program.bind_attrib_location(ATTRIB_POSITION, "position");
  program.bind_attrib_location(ATTRIB_NORMAL,   "normal");
  program.link();

  uf_modelview_        = program.get_uniform_location("modelToCameraMatrix");
  uf_projection_       = program.get_uniform_location("cameraToClipMatrix");
  uf_diffuse_material_ = program.get_uniform_location("diffuseMaterial");
  uf_piece_texture_    = program.get_uniform_location("pieceTexture");

  piece_shader_ = std::move(program);
}

void CubeScene::gl_create_cage_shader()
{
  GL::ShaderProgram program;

  program.attach(GL::ShaderObject{GL_VERTEX_SHADER,
                                  Util::locate_shader_file("wirecage.vert")});
  program.attach(GL::ShaderObject{GL_FRAGMENT_SHADER,
                                  Util::locate_shader_file("wirecage.frag")});

  program.bind_attrib_location(ATTRIB_POSITION, "position");
  program.link();

  cage_uf_modelview_  = program.get_uniform_location("modelToCameraMatrix");
  cage_uf_projection_ = program.get_uniform_location("cameraToClipMatrix");

  cage_shader_ = std::move(program);
}

void CubeScene::gl_cleanup()
{
  uf_modelview_        = -1;
  uf_projection_       = -1;
  uf_diffuse_material_ = -1;
  uf_piece_texture_    = -1;
  cage_uf_modelview_   = -1;
  cage_uf_projection_  = -1;

  piece_shader_.reset();
  cage_shader_.reset();

  gl_delete_wireframe();

  if (pieces_vertex_array_)
  {
    glDeleteVertexArrays(1, &pieces_vertex_array_);
    pieces_vertex_array_ = 0;
  }

  if (mesh_buffers_[VERTICES] || mesh_buffers_[INDICES])
  {
    glDeleteBuffers(2, mesh_buffers_);
    mesh_buffers_[VERTICES] = 0;
    mesh_buffers_[INDICES]  = 0;
  }

  if (cube_texture_)
  {
    glDeleteTextures(1, &cube_texture_);
    cube_texture_ = 0;
  }

  GL::Scene::gl_cleanup();
}

void CubeScene::gl_reset_state()
{
  GL::Scene::gl_reset_state();

  GL::ShaderProgram::unuse();

  glBindVertexArray(0);

  glDisableVertexAttribArray(ATTRIB_NORMAL);
  glDisableVertexAttribArray(ATTRIB_POSITION);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  glDisable(GL_DEPTH_TEST);
  glDisable(GL_POLYGON_OFFSET_FILL);
  glDisable(GL_POLYGON_OFFSET_LINE);

  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

int CubeScene::gl_render()
{
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  int triangle_count = 0;

  if (!animation_data_.empty())
  {
    // As recommended, spend the time immediately after a screen clear doing
    // some useful non-drawing work rather than jamming the GPU pipeline and
    // then sitting around idle waiting for the GPU to finish.
    if (depth_order_changed_)
      update_depth_order();

    if (animation_running_)
      advance_animation();

    glEnable(GL_DEPTH_TEST);

    if (show_wireframe_)
    {
      const GLenum offset_mode = (show_outline_) ? GL_POLYGON_OFFSET_LINE : GL_POLYGON_OFFSET_FILL;

      gl_draw_wireframe();

      glPolygonOffset(1.0, 4.0);
      glEnable(offset_mode);

      triangle_count += gl_draw_cube();

      glDisable(offset_mode);
    }
    else
    {
      triangle_count += gl_draw_cube();
    }

    glDisable(GL_DEPTH_TEST);
  }

  triangle_count += GL::Scene::gl_render();

  return triangle_count;
}

void CubeScene::gl_update_projection()
{
  GL::Scene::gl_update_projection();

  const float width  = Math::max(1, get_width());
  const float height = Math::max(1, get_height());

  // Set up a perspective projection with a field of view angle of 45 degrees
  // in the y-direction.  Place the far clipping plane so that the cube origin
  // will be positioned halfway between the near and far clipping planes.
  const float near = 1.0;
  const float far  = -view_z_offset * 2.0f - near;

  const float topinv   = G_SQRT2 + 1.0; // cot(pi/8)
  const float rightinv = height / width * topinv;

  using Math::Matrix4;
  using Math::Vector4;

  Matrix4 projection {Vector4{near * rightinv, 0.0, 0.0, 0.0},
                      Vector4{0.0, near * topinv, 0.0, 0.0},
                      Vector4{0.0, 0.0, (far + near) / (near - far), -1.0},
                      Vector4{0.0, 0.0, 2.0f * far * near / (near - far), 0.0}};
  if (piece_shader_)
  {
    piece_shader_.use();
    glUniformMatrix4fv(uf_projection_, 1, GL_FALSE, &projection[0][0]);
  }
  if (cage_shader_)
  {
    cage_shader_.use();
    glUniformMatrix4fv(cage_uf_projection_, 1, GL_FALSE, &projection[0][0]);
  }
  GL::ShaderProgram::unuse();
}

void CubeScene::gl_create_mesh_buffers(GL::MeshLoader& loader,
                                       const MeshNodeArray& nodes,
                                       unsigned int total_vertices,
                                       unsigned int indices_size)
{
  g_return_if_fail(pieces_vertex_array_ == 0);
  g_return_if_fail(mesh_buffers_[VERTICES] == 0 && mesh_buffers_[INDICES] == 0);

  glGenVertexArrays(1, &pieces_vertex_array_);
  GL::Error::throw_if_fail(pieces_vertex_array_ != 0);

  glGenBuffers(2, mesh_buffers_);
  GL::Error::throw_if_fail(mesh_buffers_[VERTICES] != 0 && mesh_buffers_[INDICES] != 0);

  glBindVertexArray(pieces_vertex_array_);

  glBindBuffer(GL_ARRAY_BUFFER, mesh_buffers_[VERTICES]);
  glBufferData(GL_ARRAY_BUFFER,
               total_vertices * sizeof(GL::MeshVertex),
               nullptr, GL_STATIC_DRAW);

  void *const vertex_data =
    glMapBufferRange(GL_ARRAY_BUFFER,
                     0, total_vertices * sizeof(GL::MeshVertex),
                     GL_MAP_WRITE_BIT
                     | GL_MAP_INVALIDATE_RANGE_BIT
                     | GL_MAP_INVALIDATE_BUFFER_BIT
                     | GL_MAP_UNSYNCHRONIZED_BIT);
  if (vertex_data)
  {
    for (size_t i = 0; i < nodes.size(); ++i)
      if (const auto node = nodes[i])
      {
        const auto& mesh = mesh_data_[i];
        const auto start = static_cast<GL::MeshVertex*>(vertex_data) + mesh.element_first;

        loader.get_node_vertices(node, start, mesh.element_count());
      }

    if (!glUnmapBuffer(GL_ARRAY_BUFFER))
      g_warning("glUnmapBuffer(GL_ARRAY_BUFFER) failed");
  }
  else
    g_warning("glMapBufferRange(GL_ARRAY_BUFFER) failed");

  glVertexAttribPointer(ATTRIB_POSITION, 3, GL_FLOAT, GL_FALSE, sizeof(GL::MeshVertex),
                        GL::buffer_offset(offsetof(GL::MeshVertex, vertex)));
  glVertexAttribPointer(ATTRIB_NORMAL, 3, GL_FLOAT, GL_FALSE, sizeof(GL::MeshVertex),
                        GL::buffer_offset(offsetof(GL::MeshVertex, normal)));
  glEnableVertexAttribArray(ATTRIB_POSITION);
  glEnableVertexAttribArray(ATTRIB_NORMAL);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh_buffers_[INDICES]);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               indices_size * sizeof(GL::MeshIndex),
               nullptr, GL_STATIC_DRAW);

  void *const index_data =
    glMapBufferRange(GL_ELEMENT_ARRAY_BUFFER,
                     0, indices_size * sizeof(GL::MeshIndex),
                     GL_MAP_WRITE_BIT
                     | GL_MAP_INVALIDATE_RANGE_BIT
                     | GL_MAP_INVALIDATE_BUFFER_BIT
                     | GL_MAP_UNSYNCHRONIZED_BIT);
  if (index_data)
  {
    for (size_t i = 0; i < nodes.size(); ++i)
      if (const auto node = nodes[i])
      {
        const auto& mesh = mesh_data_[i];
        const auto start = static_cast<GL::MeshIndex*>(index_data) + mesh.indices_offset;

        loader.get_node_indices(node, mesh.element_first, start,
                                GL::MeshLoader::aligned_index_count(3 * mesh.triangle_count));
      }

    if (!glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER))
      g_warning("glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER) failed");
  }
  else
    g_warning("glMapBufferRange(GL_ELEMENT_ARRAY_BUFFER) failed");

  glBindVertexArray(0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void CubeScene::on_meshes_loaded()
{
  static const std::array<char[16], CUBE_PIECE_COUNT> object_names
  {{
    "PieceOrange",
    "PieceGreen",
    "PieceRed",
    "PieceYellow",
    "PieceBlue",
    "PieceLavender",
    "PieceCyan"
  }};

  const auto loader = std::move(mesh_loader_);
  g_return_if_fail(loader);

  if (!is_realized())
    return;

  std::array<GL::MeshLoader::Node, CUBE_PIECE_COUNT> nodes;

  for (size_t i = 0; i < nodes.size(); ++i)
  {
    nodes[i] = loader->lookup_node(object_names[i]);

    if (!nodes[i])
      g_warning("Failed to load mesh of %s", object_names[i]);
  }

  mesh_data_.assign(nodes.size(), MeshData{});

  unsigned int total_vertices = 0;
  unsigned int indices_offset = 0;

  for (size_t i = 0; i < nodes.size(); ++i)
    if (const auto node = nodes[i])
    {
      const auto counts = loader->count_node_vertices_triangles(node);

      if (counts.first > 0 && counts.second > 0)
      {
        auto& mesh = mesh_data_[i];

        mesh.triangle_count = counts.second;
        mesh.indices_offset = indices_offset;
        mesh.element_first  = total_vertices;
        mesh.element_last   = total_vertices + counts.first - 1;

        total_vertices += counts.first;
        indices_offset += GL::MeshLoader::aligned_index_count(3 * counts.second);
      }
    }

  {
    ScopeContext context {*this};

    gl_create_mesh_buffers(*loader, nodes, total_vertices, indices_offset);
  }
}

void CubeScene::on_size_allocate(Gtk::Allocation& allocation)
{
  GL::Scene::on_size_allocate(allocation);

  // Mark the last track position as invalid to avoid interpreting the
  // change of the relative mouse position caused by resizing the window.
  track_last_x_ = TRACK_UNSET;
  track_last_y_ = TRACK_UNSET;

  // If the left mouse button is held down during an allocation change, and
  // afterwards the mouse pointer ends up outside the window, we will never
  // receive the corresponding button release event.  Thus it is necessary
  // to restore the default cursor at this point, as we cannot rely on the
  // button_release_event handler to do so.
  set_cursor(CURSOR_DEFAULT);
}

bool CubeScene::on_visibility_notify_event(GdkEventVisibility* event)
{
  if (animation_running_ && !animation_data_.empty())
  {
    if (event->state == GDK_VISIBILITY_FULLY_OBSCURED)
      pause_animation();
    else
      continue_animation();
  }

  return GL::Scene::on_visibility_notify_event(event);
}

bool CubeScene::on_enter_notify_event(GdkEventCrossing* event)
{
  reset_hide_cursor_timeout();

  return GL::Scene::on_enter_notify_event(event);
}

bool CubeScene::on_key_press_event(GdkEventKey* event)
{
  reset_hide_cursor_timeout();

  switch (event->state & Gtk::AccelGroup::get_default_mod_mask())
  {
    case 0:
      switch (event->keyval)
      {
        case GDK_Left:  case GDK_KP_Left:   rotate(Cube::AXIS_Y,  rotation_step); return true;
        case GDK_Right: case GDK_KP_Right:  rotate(Cube::AXIS_Y, -rotation_step); return true;
        case GDK_Up:    case GDK_KP_Up:     rotate(Cube::AXIS_X,  rotation_step); return true;
        case GDK_Down:  case GDK_KP_Down:   rotate(Cube::AXIS_X, -rotation_step); return true;
        case GDK_Begin: case GDK_KP_Begin:
        case GDK_5:     case GDK_KP_5:      set_rotation(Math::Quat{}); return true;
      }
      break;

    case GDK_CONTROL_MASK:
    case GDK_CONTROL_MASK | GDK_SHIFT_MASK:
      switch (event->keyval)
      {
        case GDK_Tab:          case GDK_KP_Tab:       cycle_exclusive( 1); return true;
        case GDK_ISO_Left_Tab: case GDK_3270_BackTab: cycle_exclusive(-1); return true;
      }
      break;

    case GDK_MOD1_MASK:
      {
        const int digit = Glib::Unicode::digit_value(gdk_keyval_to_unicode(event->keyval));

        if (digit >= 0)
        {
          select_piece(digit);
          return true;
        }
      }
      break;

    default:
      break;
  }

  return GL::Scene::on_key_press_event(event);
}

bool CubeScene::on_key_release_event(GdkEventKey* event)
{
  reset_hide_cursor_timeout();

  return GL::Scene::on_key_release_event(event);
}

bool CubeScene::on_button_press_event(GdkEventButton* event)
{
  if (event->type == GDK_BUTTON_PRESS)
  {
    grab_focus();

    if (event->button == 1)
    {
      track_last_x_ = Math::clamp_to_int(event->x);
      track_last_y_ = Math::clamp_to_int(event->y);

      set_cursor(CURSOR_DRAGGING);
    }
  }

  reset_hide_cursor_timeout();

  return GL::Scene::on_button_press_event(event);
}

bool CubeScene::on_button_release_event(GdkEventButton* event)
{
  reset_hide_cursor_timeout();

  if (event->button == 1)
  {
    if (track_last_x_ != TRACK_UNSET && track_last_y_ != TRACK_UNSET)
    {
      process_track_motion(Math::clamp_to_int(event->x), Math::clamp_to_int(event->y));

      track_last_x_ = TRACK_UNSET;
      track_last_y_ = TRACK_UNSET;

      set_cursor(CURSOR_DEFAULT);
    }
  }

  return GL::Scene::on_button_release_event(event);
}

bool CubeScene::on_motion_notify_event(GdkEventMotion* event)
{
  reset_hide_cursor_timeout();

  int x = TRACK_UNSET;
  int y = TRACK_UNSET;

  Gdk::ModifierType state = Gdk::ModifierType(event->state);

  if (event->is_hint)
  {
    get_window()->get_pointer(x, y, state);
  }
  else
  {
    x = Math::clamp_to_int(event->x);
    y = Math::clamp_to_int(event->y);
  }

  // Test both the event state and the more recent one from get_pointer(),
  // so that we do not end up processing motion events before the initial
  // button press event has been received.

  if ((state & event->state & GDK_BUTTON1_MASK) != 0 && x != TRACK_UNSET && y != TRACK_UNSET)
  {
    // If the track position has been invalidated due to an allocation change,
    // completely ignore any further button motion events until the next button
    // press event.  This avoids all sorts of problems due to the uncertainty
    // of whether we will receive a button release event or not.

    if (track_last_x_ != TRACK_UNSET && track_last_y_ != TRACK_UNSET)
    {
      process_track_motion(x, y);

      track_last_x_ = x;
      track_last_y_ = y;
    }
  }

  return GL::Scene::on_motion_notify_event(event);
}

GL::Extensions* CubeScene::gl_query_extensions()
{
  return new CubeScene::Extensions{};
}

void CubeScene::gl_reposition_layouts()
{
  const int margin_x = get_width() / 10;
  const int height   = get_height();
  const int margin_y = height / 10;

  heading_->set_window_pos(margin_x, height - margin_y - heading_->get_height());
  footing_->set_window_pos(margin_x, margin_y);
}

/*
 * Figure out an appropriate animation order for the cube pieces, so that
 * the puzzle can be put together without two pieces blocking each other.
 * Also, the puzzle should be put together in a way that appears natural
 * to the human observer, i.e. without inserting pieces from below etc.
 *
 * This is one of the few places that are closely tied to the specific
 * application of animating the Soma cube puzzle.  Generalizing the code
 * is going to be somewhat difficult, should the need ever arise.
 */
void CubeScene::update_animation_order()
{
  enum { N = Cube::N };

  static const std::array<unsigned char[3], N*N*N> cell_order
  {{
    {2,0,2}, {1,0,2}, {2,0,1}, {1,0,1}, {1,1,1}, {2,1,2}, {1,1,2},
    {2,1,1}, {0,0,2}, {2,0,0}, {2,2,2}, {0,0,1}, {1,0,0}, {0,1,2},
    {2,1,0}, {1,2,2}, {2,2,1}, {0,1,1}, {1,1,0}, {1,2,1}, {0,0,0},
    {0,2,2}, {2,2,0}, {0,1,0}, {0,2,1}, {1,2,0}, {0,2,0}
  }};

  unsigned int count = 0;
  Cube         cube;

  for (const auto& order : cell_order)
  {
    const unsigned int cell_index = N*N * order[0] + N * order[1] + order[2];

    Cube cell;
    cell.put(order[0], order[1], order[2], true);

    g_return_if_fail(cell_index < piece_cells_.size());

    piece_cells_[cell_index].piece = G_MAXUINT;
    piece_cells_[cell_index].cell  = cell_index;

    // 1) Find the cube piece which occupies this cell.
    // 2) Look it up in the already processed range of the animation data.
    // 3) If not processed yet, generate and store a new animation data element.
    // 4) Write the piece's animation index to the piece cells vector.

    const auto pcube = std::find_if(cube_pieces_.cbegin(), cube_pieces_.cend(),
                                    [cell](Cube c) { return ((c & cell) != Cube{}); });
    if (pcube != cube_pieces_.cend())
    {
      const unsigned int cube_index = pcube - cube_pieces_.cbegin();
      unsigned int       anim_index = 0;

      while (anim_index < count && animation_data_[anim_index].cube_index != cube_index)
        ++anim_index;

      if (anim_index == count)
      {
        g_return_if_fail((cube & *pcube) == Cube{});           // collision
        g_return_if_fail(anim_index < animation_data_.size()); // invalid input

        auto& anim = animation_data_[anim_index];

        anim.cube_index = cube_index;
        anim.transform = find_puzzle_piece_orientation(cube_index, *pcube);
        find_animation_axis(cube, *pcube, anim.direction);

        cube |= *pcube;
        ++count;
      }
      piece_cells_[cell_index].piece = anim_index;
    }
  }
  g_return_if_fail(count == animation_data_.size()); // invalid input

  depth_order_changed_ = true;
}

/*
 * Roughly sort the cube pieces in front-to-back order in order to take
 * advantage of the early-z optimization implemented by modern GPUs.  As
 * a side effect, the rotation of the cube impacts rendering performance
 * much less than with a static ordering.
 */
void CubeScene::update_depth_order()
{
  const Math::Matrix4 matrix = Math::Quat::to_matrix(rotation_);

  enum { N = Cube::N };

  std::array<float, N*N*N> zcoords;
  auto pcell = begin(zcoords);

  for (int x = 1 - N; x < N; x += 2)
    for (int y = 1 - N; y < N; y += 2)
      for (int z = N - 1; z > -N; z -= 2)
      {
        const Math::Vector4 coords = matrix * Math::Vector4(x, y, z);

        *pcell++ = coords.z();
      }

  std::sort(begin(piece_cells_), end(piece_cells_),
            [&zcoords](const PieceCell& a, const PieceCell& b)
            { return (zcoords[a.cell] > zcoords[b.cell]); });

  Cube cube;
  auto pdepth = begin(depth_order_);

  g_return_if_fail(pdepth != end(depth_order_));

  for (const PieceCell& pc : piece_cells_)
  {
    if (pc.piece < animation_data_.size())
    {
      const unsigned int index = animation_data_[pc.piece].cube_index;

      g_return_if_fail(index < cube_pieces_.size());

      if ((cube & cube_pieces_[index]) == Cube{})
      {
        cube |= cube_pieces_[index];
        *pdepth = pc.piece;

        if (++pdepth == end(depth_order_))
          break;
      }
    }
  }
  g_return_if_fail(pdepth == end(depth_order_));

  depth_order_changed_ = false;
}

void CubeScene::advance_animation()
{
  if (frame_trigger_.connected())
  {
    const float elapsed = animation_timer_.elapsed();

    if (elapsed >= 0.0f)
    {
      const float position = animation_seek_ - (elapsed * pieces_per_sec_);

      if (position > 0.0f)
      {
        animation_position_ = position;
      }
      else
      {
        frame_trigger_.disconnect();

        animation_position_ = 0.0;

        if (!delay_timeout_.connected())
        {
          const int interval = int(animation_delay_ / pieces_per_sec_ * 1000.0f + 0.5f);

          delay_timeout_ = Glib::signal_timeout().connect(
              sigc::mem_fun(*this, &CubeScene::on_delay_timeout),
              interval, Glib::PRIORITY_DEFAULT_IDLE);
        }
      }
    }
    else
    {
      // The system time appears to have been changed backwards.  To recover,
      // reset the timer and continue animation at the last known position.
      animation_seek_ = animation_position_;
      animation_timer_.start();
    }
  }
}

void CubeScene::set_cursor(CubeScene::CursorState state)
{
  if (state != cursor_state_ && is_realized())
  {
    const auto window = get_window();

    switch (state)
    {
      case CURSOR_DEFAULT:
        window->set_cursor();
        break;

      case CURSOR_DRAGGING:
        window->set_cursor(Gdk::Cursor{get_display(), Gdk::FLEUR});
        break;

      case CURSOR_INVISIBLE:
        {
          const char *const bitmap_data = "";

          const auto bitmap = Gdk::Bitmap::create(window, bitmap_data, 1, 1);
          const Gdk::Color color;

          // Use the empty 1x1 bitmap to set up an invisible cursor.
          window->set_cursor(Gdk::Cursor{bitmap, bitmap, color, color, 0, 0});
        }
        break;

      default:
        g_return_if_reached();
    }
  }

  cursor_state_ = state;
}

bool CubeScene::on_frame_trigger()
{
  if (animation_running_ && !animation_data_.empty() && is_drawable())
  {
    queue_draw();

    return true; // call me again
  }

  return false; // disconnect
}

bool CubeScene::on_delay_timeout()
{
  if (animation_running_ && !animation_data_.empty())
  {
    if (animation_piece_ < int(cube_pieces_.size()))
    {
      ++animation_piece_;
      animation_position_ = 1.0;

      start_piece_animation();
    }
    else
    {
      animation_piece_ = 0;
      animation_position_ = 0.0;

      signal_cycle_finished_(); // emit

      if (animation_running_ && !animation_data_.empty())
        start_piece_animation();
    }
  }

  return false; // disconnect
}

void CubeScene::start_piece_animation()
{
  if (is_drawable() && !frame_trigger_.connected())
  {
    queue_draw();

    animation_seek_ = animation_position_;

    if (vsync_enabled())
    {
      frame_trigger_ = Glib::signal_idle().connect(
          sigc::mem_fun(*this, &CubeScene::on_frame_trigger),
          Glib::PRIORITY_DEFAULT_IDLE);
    }
    else
    {
      const int interval = int(1000.0f / frames_per_sec_ + 0.5f);

      frame_trigger_ = Glib::signal_timeout().connect(
          sigc::mem_fun(*this, &CubeScene::on_frame_trigger),
          interval, Glib::PRIORITY_DEFAULT_IDLE);
    }

    animation_timer_.start();
  }
}

void CubeScene::pause_animation()
{
  frame_trigger_.disconnect();
  delay_timeout_.disconnect();
}

void CubeScene::continue_animation()
{
  if (animation_running_ && !animation_data_.empty() && is_drawable()
      && !frame_trigger_.connected() && !delay_timeout_.connected())
  {
    if (animation_position_ > 0.0f)
    {
      start_piece_animation();
    }
    else
    {
      delay_timeout_ = Glib::signal_idle().connect(
          sigc::mem_fun(*this, &CubeScene::on_delay_timeout),
          Glib::PRIORITY_DEFAULT_IDLE);
    }
  }
}

void CubeScene::reset_hide_cursor_timeout()
{
  hide_cursor_timeout_.disconnect();

  if (track_last_x_ == TRACK_UNSET || track_last_y_ == TRACK_UNSET)
    set_cursor(CURSOR_DEFAULT);

  if (animation_running_)
  {
    const int interval = int(1000.0f * hide_cursor_delay + 0.5f);

    hide_cursor_timeout_ = Glib::signal_timeout().connect(
        sigc::mem_fun(*this, &CubeScene::on_hide_cursor_timeout),
        interval, Glib::PRIORITY_DEFAULT_IDLE);
  }
}

bool CubeScene::on_hide_cursor_timeout()
{
  if ((track_last_x_ == TRACK_UNSET || track_last_y_ == TRACK_UNSET)
      && animation_running_ && is_realized())
  {
    set_cursor(CURSOR_INVISIBLE);
  }

  return false; // disconnect
}

void CubeScene::cycle_exclusive(int direction)
{
  int piece = exclusive_piece_ + direction;

  if (piece > animation_piece_)
    piece = 0;
  else if (piece < 0)
    piece = animation_piece_;

  exclusive_piece_ = piece;

  if (is_drawable())
    queue_draw();
}

void CubeScene::select_piece(int piece)
{
  pause_animation();

  if (piece > int(animation_data_.size()))
    piece = animation_data_.size();

  animation_piece_ = piece;
  animation_position_ = 0.0;

  if (exclusive_piece_ > 0)
    exclusive_piece_ = piece;

  if (is_drawable())
    queue_draw();

  continue_animation();
}

void CubeScene::process_track_motion(int x, int y)
{
  if (x != track_last_x_ || y != track_last_y_)
  {
    // In order to compute an appropriate trackball size, calculate the radius
    // of a sphere which roughly approximates the dimensions of the Soma cube.
    // As there is no single definite solution to this problem, the choice of
    // the "right" size is of course somewhat subjective.  I went for the radius
    // of a sphere which is inscribed into the cube, as that "feels" good to me.
    //
    // Ideally, the trackball should itself be a cube, but to do that the code
    // would have to take the current rotation into account.  More effort than
    // it is worth, if you ask me.

    const float cube_radius    = Cube::N * cube_cell_size / 2.0;
    const float trackball_size = (1.0 + G_SQRT2) / -view_z_offset * cube_radius;

    const int   width  = get_width();
    const int   height = get_height();
    const float scale  = 1.0f / Math::max(1, height);

    const Math::Quat track = Math::trackball_motion((2 * track_last_x_ - width + 1)  * scale,
                                                    (height - 2 * track_last_y_ - 1) * scale,
                                                    (2 * x - width + 1)  * scale,
                                                    (height - 2 * y - 1) * scale,
                                                    zoom_ * trackball_size);
    set_rotation(track * rotation_);
  }
}

/*
 * Generate a grid of solid lines along the cell boundaries.
 *
 * Note that the lines are split at the crossing points in order to avoid
 * gaps, and also to more closely match the tesselation of the cube parts.
 */
void CubeScene::gl_create_wireframe()
{
  g_return_if_fail(cage_vertex_array_ == 0);
  g_return_if_fail(wireframe_buffers_[VERTICES] == 0 && wireframe_buffers_[INDICES] == 0);

  glGenVertexArrays(1, &cage_vertex_array_);
  GL::Error::throw_if_fail(cage_vertex_array_ != 0);

  glGenBuffers(2, wireframe_buffers_);
  GL::Error::throw_if_fail(wireframe_buffers_[VERTICES] != 0 && wireframe_buffers_[INDICES] != 0);

  glBindVertexArray(cage_vertex_array_);

  glBindBuffer(GL_ARRAY_BUFFER, wireframe_buffers_[VERTICES]);
  glBufferData(GL_ARRAY_BUFFER,
               WIREFRAME_VERTEX_COUNT * sizeof(GLfloat) * 3,
               nullptr, GL_STATIC_DRAW);

  void *const vertex_data =
    glMapBufferRange(GL_ARRAY_BUFFER,
                     0, WIREFRAME_VERTEX_COUNT * sizeof(GLfloat) * 3,
                     GL_MAP_WRITE_BIT
                     | GL_MAP_INVALIDATE_RANGE_BIT
                     | GL_MAP_INVALIDATE_BUFFER_BIT
                     | GL_MAP_UNSYNCHRONIZED_BIT);
  if (vertex_data)
  {
    enum { N = Cube::N + 1 };

    float stride[N];

    for (int i = 0; i < N; ++i)
      stride[i] = (2 * i - (N - 1)) * (cube_cell_size / 2.0f);

    GLfloat* pv = static_cast<GLfloat*>(vertex_data);

    for (int z = 0; z < N; ++z)
      for (int y = 0; y < N; ++y)
        for (int x = 0; x < N; ++x)
        {
          pv[0] = stride[x];
          pv[1] = stride[y];
          pv[2] = stride[z];

          pv += 3;
        }

    if (!glUnmapBuffer(GL_ARRAY_BUFFER))
      g_warning("glUnmapBuffer(GL_ARRAY_BUFFER) failed");
  }
  else
    g_warning("glMapBufferRange(GL_ARRAY_BUFFER) failed");

  glVertexAttribPointer(ATTRIB_POSITION, 3, GL_FLOAT, GL_FALSE,
                        3 * sizeof(GLfloat), GL::buffer_offset(0));
  glEnableVertexAttribArray(ATTRIB_POSITION);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, wireframe_buffers_[INDICES]);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               WIREFRAME_LINE_COUNT * sizeof(WireframeIndex) * 2,
               nullptr, GL_STATIC_DRAW);

  void *const index_data =
    glMapBufferRange(GL_ELEMENT_ARRAY_BUFFER,
                     0, WIREFRAME_LINE_COUNT * sizeof(WireframeIndex) * 2,
                     GL_MAP_WRITE_BIT
                     | GL_MAP_INVALIDATE_RANGE_BIT
                     | GL_MAP_INVALIDATE_BUFFER_BIT
                     | GL_MAP_UNSYNCHRONIZED_BIT);
  if (index_data)
  {
    enum { N = Cube::N + 1 };

    WireframeIndex* pi = static_cast<WireframeIndex*>(index_data);

    for (int i = 0; i < N; ++i)
      for (int k = 0; k < N; ++k)
        for (int m = 0; m < N - 1; ++m)
        {
          pi[0] = N*N*i + N*k + m;
          pi[1] = N*N*i + N*k + m + 1;

          pi[2] = N*N*i + N*m + k;
          pi[3] = N*N*i + N*m + k + N;

          pi[4] = N*N*m + N*i + k;
          pi[5] = N*N*m + N*i + k + N*N;

          pi += 6;
        }

    if (!glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER))
      g_warning("glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER) failed");
  }
  else
    g_warning("glMapBufferRange(GL_ELEMENT_ARRAY_BUFFER) failed");

  glBindVertexArray(0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void CubeScene::gl_delete_wireframe()
{
  if (cage_vertex_array_)
  {
    glDeleteVertexArrays(1, &cage_vertex_array_);
    cage_vertex_array_ = 0;
  }

  if (wireframe_buffers_[VERTICES] || wireframe_buffers_[INDICES])
  {
    glDeleteBuffers(2, wireframe_buffers_);
    wireframe_buffers_[VERTICES] = 0;
    wireframe_buffers_[INDICES]  = 0;
  }
}

void CubeScene::gl_draw_wireframe()
{
  using Math::Matrix4;
  using Math::Vector4;

  if (cage_shader_ && cage_vertex_array_
      && wireframe_buffers_[VERTICES] && wireframe_buffers_[INDICES])
  {
    cage_shader_.use();
    glBindVertexArray(cage_vertex_array_);

    Matrix4 modelview {Matrix4::identity[0],
                       Matrix4::identity[1],
                       Matrix4::identity[2],
                       Vector4{0.0, 0.0, view_z_offset, 1.0}};

    modelview *= Math::Quat::to_matrix(rotation_);
    modelview *= Matrix4{Vector4{zoom_, 0.0, 0.0, 0.0},
                         Vector4{0.0, zoom_, 0.0, 0.0},
                         Vector4{0.0, 0.0, zoom_, 0.0},
                         Matrix4::identity[3]};

    glUniformMatrix4fv(cage_uf_modelview_, 1, GL_FALSE, &modelview[0][0]);

    glDrawRangeElements(GL_LINES, 0, WIREFRAME_VERTEX_COUNT - 1,
                        2 * WIREFRAME_LINE_COUNT, WIREFRAME_INDEX_TYPE,
                        GL::buffer_offset(0));

    glBindVertexArray(0);
    GL::ShaderProgram::unuse();
  }
}

int CubeScene::gl_draw_cube()
{
  int triangle_count = 0;

  if (animation_piece_ > 0 && animation_piece_ <= int(animation_data_.size()))
  {
    glBindTexture(GL_TEXTURE_2D, cube_texture_);

    if (!show_outline_)
    {
      triangle_count += gl_draw_pieces();
    }
    else
    {
      glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

      triangle_count += gl_draw_pieces();

      glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
  }
  return triangle_count;
}

int CubeScene::gl_draw_pieces()
{
  const int count = animation_data_.size();

  int first = 0;
  int last  = animation_piece_ - 1;

  if (last > count - 1)
    last = count - 1;

  if (exclusive_piece_ > 0)
  {
    if (exclusive_piece_ - 1 < last)
      last = exclusive_piece_ - 1;

    first = last;
  }

  if (last >= first)
    return gl_draw_pieces_range(first, last);

  return 0;
}

int CubeScene::gl_draw_pieces_range(int first, int last)
{
  int triangle_count = 0;

  if (piece_shader_ && pieces_vertex_array_
      && mesh_buffers_[VERTICES] && mesh_buffers_[INDICES])
  {
    piece_shader_.use();
    glBindVertexArray(pieces_vertex_array_);

    int last_fixed = last;

    if (animation_position_ > 0.0f && last == animation_piece_ - 1)
      --last_fixed;

    if (last_fixed >= first)
    {
      for (int i : depth_order_)
        if (i >= first && i <= last_fixed)
        {
          const auto& data = animation_data_[i];
          const auto& mesh = mesh_data_[data.cube_index];
          triangle_count += mesh.triangle_count;

          gl_draw_piece_elements(data, Math::Matrix4::identity[3]);
        }
    }

    if (last != last_fixed)
    {
      const auto& data = animation_data_[last];
      const auto& mesh = mesh_data_[data.cube_index];
      triangle_count += mesh.triangle_count;

      // Distance in model units an animated cube piece has to travel.
      const float animation_distance = 1.75 * Cube::N * cube_cell_size;
      const float d = animation_position_ * animation_distance;

      const Math::Vector4 translate {data.direction[0] * d,
                                     data.direction[1] * d,
                                     data.direction[2] * d,
                                     1.0};
      gl_draw_piece_elements(data, translate);
    }

    glBindVertexArray(0);
    GL::ShaderProgram::unuse();
  }

  return triangle_count;
}

void CubeScene::gl_draw_piece_elements(const AnimationData& data,
                                       Math::Vector4 animpos)
{
  using Math::Matrix4;
  using Math::Vector4;

  Matrix4 modelview {Matrix4::identity[0],
                     Matrix4::identity[1],
                     Matrix4::identity[2],
                     Vector4{0.0, 0.0, view_z_offset, 1.0}};

  modelview *= Math::Quat::to_matrix(rotation_);
  modelview *= Matrix4{Vector4{zoom_, 0.0, 0.0, 0.0},
                       Vector4{0.0, zoom_, 0.0, 0.0},
                       Vector4{0.0, 0.0, zoom_, 0.0},
                       animpos};
  modelview *= data.transform;

  glUniformMatrix4fv(uf_modelview_, 1, GL_FALSE, &modelview[0][0]);

  glUniform4fv(uf_diffuse_material_, 1,
               piece_materials[data.cube_index % piece_materials.size()]);

  const auto& mesh = mesh_data_[data.cube_index];

  glDrawRangeElements(GL_TRIANGLES, mesh.element_first, mesh.element_last,
                      3 * mesh.triangle_count, MESH_INDEX_TYPE,
                      GL::buffer_offset(mesh.indices_offset * sizeof(GL::MeshIndex)));
}

void CubeScene::gl_init_cube_texture()
{
  // Map from the number of color components minus one to the corresponding
  // OpenGL texture image data format.  We'll let OpenGL do the conversion
  // to the internal format on the fly, and flip the image through inverted
  // texture coordinates in the vertex shader.
  static const std::array<GLenum, 4> formats {{ GL_RED, GL_RG, GL_RGB, GL_RGBA }};

  // No matter what the real dimensions of the input image are, scale the
  // texture during load to the fixed size defined here.  Forcing a fixed
  // width and height actually enhances flexibility, as the user may drop
  // in whatever image file without wreaking havoc.  In essence, the size
  // of a texture is simply a quality setting unrelated to the input data.
  enum { WIDTH = 512, HEIGHT = 512 };

  g_return_if_fail(cube_texture_ == 0);

  const auto pixbuf = Gdk::Pixbuf::create_from_file(Util::locate_data_file("woodtexture.png"),
                                                    WIDTH, HEIGHT, false);

  g_return_if_fail(pixbuf->get_width() == WIDTH && pixbuf->get_height() == HEIGHT);
  g_return_if_fail(pixbuf->get_bits_per_sample() == 8);

  const int n_channels = pixbuf->get_n_channels();

  g_return_if_fail(n_channels >= 1 && n_channels <= int(formats.size()));
  g_return_if_fail(pixbuf->get_rowstride() == n_channels * WIDTH);

  glGenTextures(1, &cube_texture_);
  GL::Error::throw_if_fail(cube_texture_ != 0);

  glBindTexture(GL_TEXTURE_2D, cube_texture_);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

  if (gl_ext()->have_texture_filter_anisotropic)
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT,
                    Math::min(8.0f, gl_ext()->max_anisotropy));

  glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, WIDTH, HEIGHT, 0,
               formats[n_channels - 1], GL_UNSIGNED_BYTE, pixbuf->get_pixels());

  glGenerateMipmap(GL_TEXTURE_2D);
}

void CubeScene::update_footing()
{
  const int percentage = int(100.0f * zoom_ + 0.5f);

  if (zoom_visible_ && percentage != 100)
    footing_->set_content(Glib::ustring::compose("Zoom %1%%", percentage));
  else
    footing_->set_content({});
}

void CubeScene::gl_update_wireframe()
{
  gl_delete_wireframe();

  if (show_wireframe_)
  {
    try
    {
      gl_create_wireframe();
    }
    catch (...)
    {
      gl_reset_state();
      gl_delete_wireframe();
      throw;
    }
  }
}

} // namespace Somato
