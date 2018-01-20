/*
 * Copyright (c) 2004-2017  Daniel Elstner  <daniel.kitta@gmail.com>
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
 * along with Somato.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>

#include "cubescene.h"
#include "gltextlayout.h"
#include "glutils.h"
#include "mathutils.h"
#include "meshtypes.h"

#include <glib.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <glibmm.h>
#include <giomm/resource.h>
#include <gdkmm.h>
#include <gtkmm/accelgroup.h>
#include <epoxy/gl.h>

#include <cstddef>
#include <algorithm>
#include <functional>
#include <utility>

namespace
{

using namespace Somato;

template <typename T>
class BytesView
{
public:
  BytesView(const Glib::RefPtr<const Glib::Bytes>& bytes)
    : size_ {0}, data_ {static_cast<const T*>(bytes->get_data(size_))} {}

  std::size_t size() const { return size_ / sizeof(T); }
  const T* begin() const { return data_; }
  const T* end() const { return data_ + size(); }
  const T& operator[](std::size_t i) const { return data_[i]; }

private:
  gsize    size_;
  const T* data_;
};

/* Puzzle piece vertex shader input attribute locations.
 */
enum
{
  ATTRIB_POSITION = 0,
  ATTRIB_NORMAL   = 1
};

/* Puzzle piece fragment shader texture unit.
 */
enum
{
  SAMPLER_PIECE = 1
};

/* Index usage convention for arrays of buffer objects.
 */
enum
{
  VERTICES = 0,
  INDICES  = 1
};

/* Text layout indices.
 */
enum
{
  HEADING,
  FOOTING,
  NUM_TEXT_LAYOUTS
};

/*
 * The time span, in seconds, to wait for further user input
 * before hiding the mouse cursor while the animation is running.
 */
const float hide_cursor_delay = 5.;

/*
 * View offset in the direction of the z-axis.
 */
const float view_z_offset = -9.;

/*
 * The angle by which to rotate if a keyboard navigation key is pressed.
 */
const float rotation_step = G_PI / 60.;

/* Wood texture shear and translate matrix.
 */
const GLfloat texture_shear[2][4] =
{
  {0.474773,    0.0146367, -0.0012365, 0.74},
  {0.00168634, -0.0145917,  0.474773,  0.26}
};

/* The color of each cube piece. Indices into the colors array match
 * the original piece order as passed to CubeScene::set_cube_pieces(),
 * reduced modulo the number of colors.
 */
const std::array<GLfloat[4], 8> piece_colors
{{
  { 0.61, 0.04, 0.00, 1. }, // orange
  { 0.01, 0.33, 0.01, 1. }, // green
  { 0.61, 0.00, 0.00, 1. }, // red
  { 0.61, 0.20, 0.00, 1. }, // yellow
  { 0.01, 0.00, 0.61, 1. }, // blue
  { 0.33, 0.00, 0.61, 1. }, // lavender
  { 0.01, 0.17, 0.61, 1. }, // cyan
  { 0.61, 0.00, 0.05, 1. }  // pink
}};

/*
 * Find the direction from which a cube piece can be shifted into
 * its desired position, without colliding with any other piece
 * already in place (think Tetris).
 */
void find_animation_axis(SomaCube cube, SomaCube piece, float* direction)
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
    { AXIS_Y, false,  0.,  1.,  0. }, // top->down
    { AXIS_Z, true,   0.,  0.,  1. }, // front->back
    { AXIS_X, true,  -1.,  0.,  0. }, // left->right
    { AXIS_X, false,  1.,  0.,  0. }, // right->left
    { AXIS_Z, false,  0.,  0., -1. }, // back->front
    { AXIS_Y, true,   0., -1.,  0. }  // bottom->up
  }};

  for (const auto& movement : movement_data)
  {
    // Swap fixed and moving pieces if backward shifting is indicated.
    const SomaCube fixed  = (movement.backward) ? piece : cube;
    SomaCube       moving = (movement.backward) ? cube : piece;

    // Now do the shifting until the moving piece has either
    // vanished from view or collided with the fixed piece.
    do
    {
      if (!moving) // if it vanished we have just found our solution
      {
        direction[0] = movement.x;
        direction[1] = movement.y;
        direction[2] = movement.z;
        return;
      }
      moving.shift(movement.axis, ClipMode::SLICE);
    }
    while (!(fixed & moving));
  }

  // This should not happen as long as the input is correct.
  g_return_if_reached();
}

} // anonymous namespace

namespace Somato
{

CubeScene::CubeScene(BaseObjectType* obj, const Glib::RefPtr<Gtk::Builder>&)
:
  GL::Scene    {obj},
  piece_cells_ (SomaCube::N * SomaCube::N * SomaCube::N)
{
  text_layouts()->set_layout_count(NUM_TEXT_LAYOUTS);
  text_layouts()->set_layout_color(HEADING, GL::pack_4u8_norm(0.4, 0.4, 0.4, 1.));
  text_layouts()->set_layout_color(FOOTING, GL::pack_4u8_norm(0.2, 0.2, 0.2, 1.));

  set_can_focus(true);

  add_events(Gdk::BUTTON_PRESS_MASK | Gdk::BUTTON_RELEASE_MASK | Gdk::BUTTON1_MOTION_MASK
             | Gdk::POINTER_MOTION_MASK | Gdk::KEY_PRESS_MASK | Gdk::KEY_RELEASE_MASK
             | Gdk::ENTER_NOTIFY_MASK | Gdk::LEAVE_NOTIFY_MASK | Gdk::VISIBILITY_NOTIFY_MASK);
}

CubeScene::~CubeScene()
{}

void CubeScene::set_heading(Glib::ustring heading)
{
  text_layouts()->set_layout_text(HEADING, std::move(heading));

  if (text_layouts()->update_needed())
    queue_static_draw();
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

  if (animation_running_ || animation_piece_ > static_cast<int>(cube_pieces.size()))
  {
    animation_piece_    = 0;
    animation_position_ = 0.;
  }

  continue_animation();
  queue_static_draw();
}

void CubeScene::set_zoom(float zoom)
{
  const float value = Math::clamp(zoom, 0.125f, 8.f);

  if (value != zoom_)
  {
    zoom_ = value;

    if (zoom_visible_)
      update_footing();
  }
}

float CubeScene::get_zoom() const
{
  return zoom_;
}

void CubeScene::set_rotation(const Math::Quat& rotation)
{
  rotation_ = rotation.normalized();
  depth_order_changed_ = true;

  if (!animation_data_.empty())
    queue_static_draw();
}

Math::Quat CubeScene::get_rotation() const
{
  return rotation_;
}

void CubeScene::set_animation_delay(float animation_delay)
{
  animation_delay_ = Math::clamp(animation_delay, 0.f, 1.f);
}

float CubeScene::get_animation_delay() const
{
  return animation_delay_;
}

void CubeScene::set_pieces_per_second(float pieces_per_second)
{
  const float value = Math::clamp(pieces_per_second, 0.01f, 100.f);

  if (value != pieces_per_sec_)
  {
    pieces_per_sec_ = value;

    if (animation_position_ > 0.f)
    {
      animation_seek_ = animation_position_;
      reset_animation_tick();
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
  }
}

bool CubeScene::get_zoom_visible() const
{
  return zoom_visible_;
}

void CubeScene::set_show_cell_grid(bool show_cell_grid)
{
  if (show_cell_grid != show_cell_grid_)
  {
    show_cell_grid_ = show_cell_grid;

    if (mesh_vertex_array_)
      queue_static_draw();
  }
}

bool CubeScene::get_show_cell_grid() const
{
  return show_cell_grid_;
}

void CubeScene::set_show_outline(bool show_outline)
{
  if (show_outline != show_outline_)
  {
    show_outline_ = show_outline;

    if (!animation_data_.empty())
      queue_static_draw();
  }
}

bool CubeScene::get_show_outline() const
{
  return show_outline_;
}

int CubeScene::get_cube_triangle_count() const
{
  int cube_triangle_count = 0;

  for (const auto& mesh : BytesView<MeshDesc>{mesh_desc_})
    cube_triangle_count += mesh.triangle_count;

  return cube_triangle_count;
}

int CubeScene::get_cube_vertex_count() const
{
  int cube_vertex_count = 0;

  for (const auto& mesh : BytesView<MeshDesc>{mesh_desc_})
    cube_vertex_count += mesh.element_count();

  return cube_vertex_count;
}

void CubeScene::gl_initialize()
{
  GL::Scene::gl_initialize();

  glClearColor(0., 0., 0., 1.);
  glEnable(GL_CULL_FACE);

  // Trade viewspace clipping for depth clamping to avoid highly visible
  // volume clipping artifacts. The clamping could potentially produce some
  // artifacts of its own, but so far it appears to play along nicely.
  // Unfortunately this feature is not available with OpenGL ES.
  if (!GL::extensions().is_gles)
    glEnable(GL_DEPTH_CLAMP);

  gl_create_piece_shader();

  if (GL::extensions().geometry_shader)
  {
    gl_create_outline_shader();
    gl_create_grid_shader();
  }
  gl_init_cube_texture();
  gl_create_mesh_buffers();

  piece_shader_.use();
  glUniformMatrix2x4fv(uf_texture_shear_, 1, GL_FALSE, texture_shear[0]);
  glUniform1i(uf_piece_texture_, SAMPLER_PIECE);
}

void CubeScene::gl_create_piece_shader()
{
  GL::ShaderProgram program;
  program.set_label("puzzlepieces");

  program.attach({GL_VERTEX_SHADER,   RESOURCE_PREFIX "shaders/puzzlepieces.vert"});
  program.attach({GL_FRAGMENT_SHADER, RESOURCE_PREFIX "shaders/puzzlepieces.frag"});

  program.bind_attrib_location(ATTRIB_POSITION, "position");
  program.bind_attrib_location(ATTRIB_NORMAL,   "normal");
  program.bind_frag_data_location(0, "outputColor");
  program.link();

  uf_model_view_    = program.get_uniform_location("modelView");
  uf_view_frustum_  = program.get_uniform_location("viewFrustum");
  uf_texture_shear_ = program.get_uniform_location("textureShear");
  uf_diffuse_color_ = program.get_uniform_location("diffuseColor");
  uf_piece_texture_ = program.get_uniform_location("pieceTexture");

  piece_shader_ = std::move(program);
}

void CubeScene::gl_create_outline_shader()
{
  GL::ShaderProgram program;
  program.set_label("pieceoutline");

  program.attach({GL_VERTEX_SHADER,   RESOURCE_PREFIX "shaders/pieceoutline.vert"});
  program.attach({GL_GEOMETRY_SHADER, RESOURCE_PREFIX "shaders/pieceoutline.geom"});
  program.attach({GL_FRAGMENT_SHADER, RESOURCE_PREFIX "shaders/pieceoutline.frag"});

  program.bind_attrib_location(ATTRIB_POSITION, "position");
  program.bind_attrib_location(ATTRIB_NORMAL,   "normal");
  program.bind_frag_data_location(0, "outputColor");
  program.link();

  ol_uf_model_view_    = program.get_uniform_location("modelView");
  ol_uf_view_frustum_  = program.get_uniform_location("viewFrustum");
  ol_uf_window_size_   = program.get_uniform_location("windowSize");
  ol_uf_diffuse_color_ = program.get_uniform_location("diffuseColor");

  outline_shader_ = std::move(program);
}

void CubeScene::gl_create_grid_shader()
{
  GL::ShaderProgram program;
  program.set_label("cellgrid");

  program.attach({GL_VERTEX_SHADER,   RESOURCE_PREFIX "shaders/cellgrid.vert"});
  program.attach({GL_GEOMETRY_SHADER, RESOURCE_PREFIX "shaders/cellgrid.geom"});
  program.attach({GL_FRAGMENT_SHADER, RESOURCE_PREFIX "shaders/cellgrid.frag"});

  program.bind_attrib_location(ATTRIB_POSITION, "position");
  program.bind_frag_data_location(0, "outputColor");
  program.link();

  grid_uf_model_view_   = program.get_uniform_location("modelView");
  grid_uf_view_frustum_ = program.get_uniform_location("viewFrustum");
  grid_uf_pixel_scale_  = program.get_uniform_location("pixelScale");

  grid_shader_ = std::move(program);
}

void CubeScene::gl_cleanup()
{
  uf_model_view_        = -1;
  uf_view_frustum_      = -1;
  uf_texture_shear_     = -1;
  uf_diffuse_color_     = -1;
  uf_piece_texture_     = -1;
  ol_uf_model_view_     = -1;
  ol_uf_view_frustum_   = -1;
  ol_uf_window_size_    = -1;
  ol_uf_diffuse_color_  = -1;
  grid_uf_model_view_   = -1;
  grid_uf_view_frustum_ = -1;
  grid_uf_pixel_scale_  = -1;

  piece_shader_.reset();
  outline_shader_.reset();
  grid_shader_.reset();

  if (mesh_vertex_array_)
  {
    glDeleteVertexArrays(1, &mesh_vertex_array_);
    mesh_vertex_array_ = 0;
  }

  if (mesh_buffers_[VERTICES] | mesh_buffers_[INDICES])
  {
    glDeleteBuffers(G_N_ELEMENTS(mesh_buffers_), mesh_buffers_);
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

    if (mesh_vertex_array_)
    {
      glBindVertexArray(mesh_vertex_array_);
      glEnable(GL_DEPTH_TEST);

      Math::Matrix4 cube_transform {Math::Vector4::basis[0],
                                    Math::Vector4::basis[1],
                                    Math::Vector4::basis[2],
                                    {0.f, 0.f, view_z_offset, 1.f}};
      cube_transform *= Math::Matrix4::from_quaternion(rotation_);
      cube_transform.scale(zoom_);

      if (animation_piece_ > 0 && animation_piece_ <= static_cast<int>(animation_data_.size()))
        triangle_count += gl_draw_pieces(cube_transform);

      if (show_cell_grid_)
        gl_draw_cell_grid(cube_transform);

      glDisable(GL_DEPTH_TEST);
    }
  }
  triangle_count += GL::Scene::gl_render();

  return triangle_count;
}

void CubeScene::gl_update_viewport()
{
  GL::Scene::gl_update_viewport();

  cube_proj_dirty_    = true;
  outline_proj_dirty_ = true;
  grid_proj_dirty_    = true;

  const int margin_x = get_viewport_width()  / 10;
  const int margin_y = get_viewport_height() / 10;

  text_layouts()->set_layout_pos(HEADING, GL::TextLayout::TOP_LEFT,
                                 margin_x, get_viewport_height() - margin_y);
  text_layouts()->set_layout_pos(FOOTING, GL::TextLayout::BOTTOM_LEFT,
                                 margin_x, margin_y);
}

void CubeScene::gl_create_mesh_buffers()
{
  g_return_if_fail(mesh_vertex_array_ == 0);
  g_return_if_fail(mesh_buffers_[VERTICES] == 0 && mesh_buffers_[INDICES] == 0);

  mesh_desc_ = Gio::Resource::lookup_data_global(RESOURCE_PREFIX "mesh-desc.bin");
  const auto vertices = Gio::Resource::lookup_data_global(RESOURCE_PREFIX "mesh-vertices.bin");
  const auto indices  = Gio::Resource::lookup_data_global(RESOURCE_PREFIX "mesh-indices.bin");

  glGenVertexArrays(1, &mesh_vertex_array_);
  GL::Error::throw_if_fail(mesh_vertex_array_ != 0);

  glGenBuffers(G_N_ELEMENTS(mesh_buffers_), mesh_buffers_);
  GL::Error::throw_if_fail(mesh_buffers_[VERTICES] != 0 && mesh_buffers_[INDICES] != 0);

  glBindVertexArray(mesh_vertex_array_);
  GL::set_object_label(GL_VERTEX_ARRAY, mesh_vertex_array_, "meshArray");

  glBindBuffer(GL_ARRAY_BUFFER, mesh_buffers_[VERTICES]);
  GL::set_object_label(GL_BUFFER, mesh_buffers_[VERTICES], "meshVertices");

  gsize vertices_size = 0;
  const auto *const vertices_data = vertices->get_data(vertices_size);

  glBufferData(GL_ARRAY_BUFFER, vertices_size, vertices_data, GL_STATIC_DRAW);

  glVertexAttribPointer(ATTRIB_POSITION,
                        GL::attrib_size<decltype(MeshVertex::position)>,
                        GL::attrib_type<decltype(MeshVertex::position)>,
                        GL_FALSE, sizeof(MeshVertex),
                        GL::buffer_offset(offsetof(MeshVertex, position)));
  glVertexAttribPointer(ATTRIB_NORMAL,
                        GL::attrib_size<decltype(MeshVertex::normal)>,
                        GL::attrib_type<decltype(MeshVertex::normal)>,
                        GL_TRUE, sizeof(MeshVertex),
                        GL::buffer_offset(offsetof(MeshVertex, normal)));
  glEnableVertexAttribArray(ATTRIB_POSITION);
  glEnableVertexAttribArray(ATTRIB_NORMAL);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh_buffers_[INDICES]);
  GL::set_object_label(GL_BUFFER, mesh_buffers_[INDICES], "meshIndices");

  gsize indices_size = 0;
  const auto *const indices_data = indices->get_data(indices_size);

  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices_size, indices_data, GL_STATIC_DRAW);

  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  g_info("Mesh totals: %u vertices, %u indices",
         static_cast<unsigned int>(vertices_size / sizeof(MeshVertex)),
         static_cast<unsigned int>(indices_size  / sizeof(MeshIndex)));
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
  pointer_inside_ = true;
  reset_hide_cursor_timeout();

  return GL::Scene::on_enter_notify_event(event);
}

bool CubeScene::on_leave_notify_event(GdkEventCrossing* event)
{
  pointer_inside_ = false;
  reset_hide_cursor_timeout();

  return GL::Scene::on_leave_notify_event(event);
}

bool CubeScene::on_key_press_event(GdkEventKey* event)
{
  using Math::Quat;

  reset_hide_cursor_timeout();

  switch (event->state & Gtk::AccelGroup::get_default_mod_mask())
  {
    case 0:
      switch (event->keyval)
      {
        case GDK_KEY_Left:
        case GDK_KEY_KP_Left:
          set_rotation(Quat::from_axis(0., 1., 0., rotation_step) * rotation_);
          return true;
        case GDK_KEY_Right:
        case GDK_KEY_KP_Right:
          set_rotation(Quat::from_axis(0., 1., 0., -rotation_step) * rotation_);
          return true;
        case GDK_KEY_Up:
        case GDK_KEY_KP_Up:
          set_rotation(Quat::from_axis(1., 0., 0., rotation_step) * rotation_);
          return true;
        case GDK_KEY_Down:
        case GDK_KEY_KP_Down:
          set_rotation(Quat::from_axis(1., 0., 0., -rotation_step) * rotation_);
          return true;
        case GDK_KEY_Begin:
        case GDK_KEY_KP_Begin:
        case GDK_KEY_5:
        case GDK_KEY_KP_5:
          set_rotation({});
          return true;
      }
      break;

    case GDK_CONTROL_MASK:
    case GDK_CONTROL_MASK | GDK_SHIFT_MASK:
      switch (event->keyval)
      {
        case GDK_KEY_Tab:
        case GDK_KEY_KP_Tab:
          cycle_exclusive(1);
          return true;
        case GDK_KEY_ISO_Left_Tab:
        case GDK_KEY_3270_BackTab:
          cycle_exclusive(-1);
          return true;
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

  auto state = static_cast<Gdk::ModifierType>(event->state);

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

bool CubeScene::on_animation_tick(gint64 animation_time)
{
  const float elapsed  = animation_time * (1.f / G_USEC_PER_SEC);
  const float position = animation_seek_ - (elapsed * pieces_per_sec_);

  animation_position_ = std::max(0.f, position);
  queue_draw();

  if (position > 0.)
    return true;

  if (!delay_timeout_.connected())
  {
    const int interval =
        static_cast<int>(animation_delay_ / pieces_per_sec_ * 1000.f + 0.5f);

    delay_timeout_ = Glib::signal_timeout().connect(
        sigc::mem_fun(*this, &CubeScene::on_delay_timeout),
        interval, Glib::PRIORITY_DEFAULT_IDLE);
  }
  return false;
}

void CubeScene::update_footing()
{
  const int percentage = static_cast<int>(100.f * zoom_ + 0.5f);

  if (zoom_visible_ && percentage != 100)
    text_layouts()->set_layout_text(FOOTING, Glib::ustring::compose("Zoom %1%%", percentage));
  else
    text_layouts()->set_layout_text(FOOTING, {});

  if (text_layouts()->update_needed())
    queue_static_draw();
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
  enum { N = SomaCube::N };

  static const std::array<unsigned char[3], N*N*N> cell_order
  {{
    {2,0,2}, {1,0,2}, {2,0,1}, {1,0,1}, {1,1,1}, {2,1,2}, {1,1,2},
    {2,1,1}, {0,0,2}, {2,0,0}, {2,2,2}, {0,0,1}, {1,0,0}, {0,1,2},
    {2,1,0}, {1,2,2}, {2,2,1}, {0,1,1}, {1,1,0}, {1,2,1}, {0,0,0},
    {0,2,2}, {2,2,0}, {0,1,0}, {0,2,1}, {1,2,0}, {0,2,0}
  }};

  unsigned int count = 0;
  SomaCube     cube;

  for (const auto& order : cell_order)
  {
    const unsigned int cell_index = N*N * order[0] + N * order[1] + order[2];

    SomaCube cell;
    cell.put(order[0], order[1], order[2], true);

    g_return_if_fail(cell_index < piece_cells_.size());

    piece_cells_[cell_index].piece = G_MAXUINT;
    piece_cells_[cell_index].cell  = cell_index;

    // 1) Find the cube piece which occupies this cell.
    // 2) Look it up in the already processed range of the animation data.
    // 3) If not processed yet, generate and store a new animation data element.
    // 4) Write the piece's animation index to the piece cells vector.

    const auto pcube = std::find_if(cube_pieces_.cbegin(), cube_pieces_.cend(),
                                    [cell](SomaCube c) { return (c & cell); });
    if (pcube != cube_pieces_.cend())
    {
      const unsigned int cube_index = pcube - cube_pieces_.cbegin();
      unsigned int       anim_index = 0;

      while (anim_index < count && animation_data_[anim_index].cube_index != cube_index)
        ++anim_index;

      if (anim_index == count)
      {
        g_return_if_fail(!(cube & *pcube));                    // collision
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
  const auto matrix = Math::Matrix4::from_quaternion(rotation_);

  enum { N = SomaCube::N };

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

  SomaCube cube;
  auto pdepth = begin(depth_order_);

  g_return_if_fail(pdepth != end(depth_order_));

  for (const PieceCell& pc : piece_cells_)
  {
    if (pc.piece < animation_data_.size())
    {
      const unsigned int index = animation_data_[pc.piece].cube_index;

      g_return_if_fail(index < cube_pieces_.size());

      if (!(cube & cube_pieces_[index]))
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

void CubeScene::set_cursor(CubeScene::CursorState state)
{
  if (state != cursor_state_ && get_realized())
  {
    const auto window = get_window();

    switch (state)
    {
      case CURSOR_DEFAULT:
        window->set_cursor();
        break;
      case CURSOR_DRAGGING:
        window->set_cursor(Gdk::Cursor::create(get_display(), "all-scroll"));
        break;
      case CURSOR_INVISIBLE:
        window->set_cursor(Gdk::Cursor::create(get_display(), "none"));
        break;
      default:
        g_return_if_reached();
    }
  }
  cursor_state_ = state;
}

bool CubeScene::on_delay_timeout()
{
  if (animation_running_ && !animation_data_.empty())
  {
    if (animation_piece_ < static_cast<int>(cube_pieces_.size()))
    {
      ++animation_piece_;
      animation_position_ = 1.;

      start_piece_animation();
    }
    else
    {
      animation_piece_ = 0;
      animation_position_ = 0.;

      signal_cycle_finished_(); // emit

      if (animation_running_ && !animation_data_.empty())
        start_piece_animation();
    }
  }
  return false; // disconnect
}

void CubeScene::start_piece_animation()
{
  if (get_is_drawable())
  {
    animation_seek_ = animation_position_;
    start_animation_tick();
  }
}

void CubeScene::pause_animation()
{
  stop_animation_tick();
  delay_timeout_.disconnect();
}

void CubeScene::continue_animation()
{
  if (animation_running_ && !animation_data_.empty() && get_is_drawable()
      && !animation_tick_active() && !delay_timeout_.connected())
  {
    if (animation_position_ > 0.f)
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
  {
    set_cursor(CURSOR_DEFAULT);

    if (pointer_inside_ && animation_running_)
    {
      const int interval = static_cast<int>(1000.f * hide_cursor_delay + 0.5f);

      hide_cursor_timeout_ = Glib::signal_timeout().connect(
          sigc::mem_fun(*this, &CubeScene::on_hide_cursor_timeout),
          interval, Glib::PRIORITY_DEFAULT_IDLE);
    }
  }
}

bool CubeScene::on_hide_cursor_timeout()
{
  if ((track_last_x_ == TRACK_UNSET || track_last_y_ == TRACK_UNSET)
      && pointer_inside_ && animation_running_ && get_realized())
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

  queue_static_draw();
}

void CubeScene::select_piece(int piece)
{
  pause_animation();

  if (piece > static_cast<int>(animation_data_.size()))
    piece = animation_data_.size();

  animation_piece_ = piece;
  animation_position_ = 0.;

  if (exclusive_piece_ > 0)
    exclusive_piece_ = piece;

  continue_animation();
  queue_static_draw();
}

void CubeScene::process_track_motion(int x, int y)
{
  if (x != track_last_x_ || y != track_last_y_)
  {
    // In order to compute an appropriate trackball size, calculate the radius
    // of a sphere which roughly approximates the dimensions of the Soma cube.
    // As there is no single definite solution to this problem, the choice of
    // the "right" size is somewhat subjective. The radius used here corresponds
    // to a sphere which touches the cube's edges.
    const float edge_length    = SomaCube::N * grid_cell_size;
    const float trackball_size = (0.5 * G_SQRT2 + 1.) / -view_z_offset * edge_length;

    const int   width  = std::max(1, get_allocated_width());
    const int   height = std::max(1, get_allocated_height());
    const float scale  = 1.f / height;

    const auto track = Math::trackball_motion((2 * track_last_x_ - width + 1)  * scale,
                                              (height - 2 * track_last_y_ - 1) * scale,
                                              (2 * x - width + 1)  * scale,
                                              (height - 2 * y - 1) * scale,
                                              zoom_ * trackball_size);
    set_rotation(track * rotation_);
  }
}

void CubeScene::gl_set_projection(int id, float offset)
{
  const float width  = get_viewport_width();
  const float height = get_viewport_height();

  const float topinv   = G_SQRT2 + 1.; // cot(pi/8)
  const float rightinv = height / width * topinv;

  // Set up a perspective projection with a field of view angle of 45 degrees
  // in the y-direction.  Place the far clipping plane so that the cube origin
  // will be positioned halfway between the near and far clipping planes.
  const float near = 1.;
  const float far  = -view_z_offset * 2.f - near;
  const float dist = near - far;

  // Since the viewing volume is symmetrical, the resulting projection matrix
  // can be compacted to just four coefficients packed into a single 4-vector
  // uniform. This requires slightly more verbose code in the vertex shader,
  // but saves instruction cycles compared to a matrix multiply.
  const Math::Vector4 frustum {near * rightinv,
                               near * topinv,
                               (far + near) / dist + offset,
                               2.f * far * near / dist};
  glUniform4fv(id, 1, &frustum[0]);
}

void CubeScene::gl_draw_cell_grid(const Math::Matrix4& cube_transform)
{
  if (grid_shader_)
  {
    grid_shader_.use();

    if (grid_proj_dirty_)
    {
      grid_proj_dirty_ = false;

      const float w = get_unscaled_width();
      const float h = get_unscaled_height();

      // Negate the width reciprocal to save a partial negation in the shader.
      const Math::Vector4 pixel_scale {0.5f * w, 0.5f * h, -2.f / w, 2.f / h};

      glUniform4fv(grid_uf_pixel_scale_, 1, &pixel_scale[0]);

      // Shift grid lines slighty to the front to suppress z-fighting.
      gl_set_projection(grid_uf_view_frustum_, 1.f / (1 << 13));
    }
    const Math::Matrix4 model_view = cube_transform.transposed();

    glUniformMatrix3x4fv(grid_uf_model_view_, 1, GL_FALSE, &model_view[0][0]);

    glDrawRangeElements(GL_LINES, 0, GRID_VERTEX_COUNT - 1,
                        2 * GRID_LINE_COUNT, GL::attrib_type<MeshIndex>,
                        GL::buffer_offset<MeshIndex>(0));
  }
}

int CubeScene::gl_draw_pieces(const Math::Matrix4& cube_transform)
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
    return gl_draw_pieces_range(cube_transform, first, last);

  return 0;
}

int CubeScene::gl_draw_pieces_range(const Math::Matrix4& cube_transform,
                                    int first, int last)
{
  int triangle_count = 0;
  GL::ShaderProgram& shader = (show_outline_) ? outline_shader_ : piece_shader_;

  if (shader)
  {
    shader.use();
    bool& proj_dirty = (show_outline_) ? outline_proj_dirty_ : cube_proj_dirty_;

    if (proj_dirty)
    {
      proj_dirty = false;

      if (show_outline_)
      {
        const float window_size[] = {0.5f * get_unscaled_width(), 0.5f * get_unscaled_height()};
        glUniform2fv(ol_uf_window_size_, 1, window_size);
      }
      gl_set_projection((show_outline_) ? ol_uf_view_frustum_ : uf_view_frustum_);
    }
    const BytesView<MeshDesc> desc_view {mesh_desc_};

    int last_fixed = last;

    if (animation_position_ > 0.f && last == animation_piece_ - 1)
      --last_fixed;

    if (last_fixed >= first)
    {
      for (const int i : depth_order_)
        if (i >= first && i <= last_fixed)
        {
          const auto& data = animation_data_[i];
          const auto& mesh = desc_view[data.cube_index];
          triangle_count += mesh.triangle_count;

          gl_draw_piece_elements(cube_transform, data);
        }
    }
    if (last != last_fixed)
    {
      const auto& data = animation_data_[last];
      const auto& mesh = desc_view[data.cube_index];
      triangle_count += mesh.triangle_count;

      // Distance in model units an animated cube piece has to travel.
      const float animation_distance = 1.75 * SomaCube::N * grid_cell_size;
      const float d = animation_position_ * animation_distance;

      const auto transform = cube_transform.translated({data.direction[0] * d,
                                                        data.direction[1] * d,
                                                        data.direction[2] * d,
                                                        1.f});
      gl_draw_piece_elements(transform, data);
    }
  }
  return triangle_count;
}

void CubeScene::gl_draw_piece_elements(const Math::Matrix4& transform,
                                       const AnimationData& data)
{
  Math::Matrix4 model_view = transform * data.transform;
  model_view.transpose();

  glUniformMatrix3x4fv((show_outline_) ? ol_uf_model_view_ : uf_model_view_,
                       1, GL_FALSE, &model_view[0][0]);
  glUniform4fv((show_outline_) ? ol_uf_diffuse_color_ : uf_diffuse_color_,
               1, piece_colors[data.cube_index % piece_colors.size()]);

  const auto& mesh = BytesView<MeshDesc>{mesh_desc_}[data.cube_index];

  glDrawRangeElements(GL_TRIANGLES, mesh.element_first, mesh.element_last,
                      3 * mesh.triangle_count, GL::attrib_type<MeshIndex>,
                      GL::buffer_offset<MeshIndex>(mesh.indices_offset));
}

void CubeScene::gl_init_cube_texture()
{
  const auto resource = Gio::Resource::lookup_data_global(RESOURCE_PREFIX "woodtexture.ktx");
  g_return_if_fail(resource);

  const BytesView<guint32> ktx {resource};

  glActiveTexture(GL_TEXTURE0 + SAMPLER_PIECE);

  glGenTextures(1, &cube_texture_);
  GL::Error::throw_if_fail(cube_texture_ != 0);

  glBindTexture(GL_TEXTURE_2D, cube_texture_);
  GL::set_object_label(GL_TEXTURE, cube_texture_, "woodtexture");

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

  if (GL::extensions().texture_filter_anisotropic)
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT,
                    std::min(8.f, GL::extensions().max_anisotropy));

  GL::tex_image_from_ktx(&ktx[0], ktx.size());
}

} // namespace Somato
