/*
 * Copyright (c) 2004-2006  Daniel Elstner  <daniel.kitta@gmail.com>
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

#include "cubescene.h"
#include "glsceneprivate.h"
#include "glutils.h"
#include "mathutils.h"
#include "tesselate.h"

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
#include <GL/glu.h>

#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <functional>
#include <limits>
#include <sstream>

namespace
{

using Somato::Cube;

static
const char *const cube_texture_filename = SOMATO_PKGDATADIR G_DIR_SEPARATOR_S "cubetexture.png";

/*
 * The type of indices into the wireframe vertex array.
 */
typedef GLubyte WireframeIndex;

enum
{
  WIREFRAME_INDEX_TYPE    = GL_UNSIGNED_BYTE,
  WIREFRAME_VERTEX_COUNT  = (Cube::N + 1) * (Cube::N + 1) * (Cube::N + 1),
  WIREFRAME_LINE_COUNT    = (Cube::N + 1) * (Cube::N + 1) *  Cube::N * 3
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
static const float radians_per_degree = G_PI / 180.0;

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
 * Field of view angle in the direction of the y-axis.
 */
static const float field_of_view = 45.0;

/*
 * View offset in the direction of the z-axis.
 */
static const float view_z_offset = -9.0;

/*
 * The angle by which to rotate if a keyboard navigation key is pressed.
 */
static const float rotation_step = 3.0;

/*
 * Predicate employed to sort the piece cells in front-to-back order.
 */
class CellAboveZ : public std::binary_function<Somato::PieceCell, Somato::PieceCell, bool>
{
private:
  const float* zcoords_;

public:
  explicit CellAboveZ(const float* zcoords)
    : zcoords_ (zcoords) {}

  bool operator()(const Somato::PieceCell& a, const Somato::PieceCell& b) const
    { return (zcoords_[a.cell] > zcoords_[b.cell]); }
};

/*
 * The materials applied to cube pieces.  Indices into the materials array
 * match the original piece order as passed to CubeScene::set_cube_pieces(),
 * reduced modulo the number of materials.
 */
struct PieceMaterial
{
  GLfloat diffuse [4];
  GLfloat specular[4];
};

static const PieceMaterial piece_materials[] =
{
  { { 0.80, 0.25, 0.00, 1.0 }, { 0.10, 0.09, 0.08, 1.0 } }, // orange
  { { 0.05, 0.70, 0.10, 1.0 }, { 0.08, 0.10, 0.08, 1.0 } }, // green
  { { 0.80, 0.00, 0.00, 1.0 }, { 0.10, 0.08, 0.08, 1.0 } }, // red
  { { 0.80, 0.65, 0.00, 1.0 }, { 0.10, 0.09, 0.08, 1.0 } }, // yellow
  { { 0.10, 0.00, 0.80, 1.0 }, { 0.08, 0.08, 0.10, 1.0 } }, // blue
  { { 0.75, 0.00, 0.80, 1.0 }, { 0.09, 0.08, 0.10, 1.0 } }, // magenta
  { { 0.05, 0.65, 0.75, 1.0 }, { 0.08, 0.09, 0.10, 1.0 } }, // cyan
  { { 0.80, 0.00, 0.25, 1.0 }, { 0.10, 0.08, 0.09, 1.0 } }  // pink
};

static inline
void gl_set_piece_material(unsigned int cube_index)
{
  const PieceMaterial& material = piece_materials[cube_index % G_N_ELEMENTS(piece_materials)];

  glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, material.diffuse);
  glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR,            material.specular);
}

/*
 * Find the direction from which a cube piece can be shifted into
 * its desired position, without colliding with any other piece
 * already in place (think Tetris).
 */
static
void find_animation_axis(Cube cube, Cube piece, float* direction)
{
  static const struct
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
  }
  movement_data[] = // directions listed first are prefered
  {
    { Cube::AXIS_Y, false,  0.0,  1.0,  0.0 }, // top->down
    { Cube::AXIS_Z, true,   0.0,  0.0,  1.0 }, // front->back
    { Cube::AXIS_X, true,  -1.0,  0.0,  0.0 }, // left->right
    { Cube::AXIS_X, false,  1.0,  0.0,  0.0 }, // right->left
    { Cube::AXIS_Z, false,  0.0,  0.0, -1.0 }, // back->front
    { Cube::AXIS_Y, true,   0.0, -1.0,  0.0 }  // bottom->up
  };

  for (unsigned int i = 0; i < G_N_ELEMENTS(movement_data); ++i)
  {
    // Swap fixed and moving pieces if backward shifting is indicated.
    const Cube fixed  = (movement_data[i].backward) ? piece : cube;
    Cube       moving = (movement_data[i].backward) ? cube : piece;

    // Now do the shifting until the moving piece has either
    // vanished from view or collided with the fixed piece.
    do
    {
      if (moving == Cube()) // if it vanished we have just found our solution
      {
        direction[0] = movement_data[i].x;
        direction[1] = movement_data[i].y;
        direction[2] = movement_data[i].z;
        return;
      }
      moving.shift(movement_data[i].axis, true);
    }
    while ((fixed & moving) == Cube());
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
  Extensions(const CubeScene::Extensions&);
  CubeScene::Extensions& operator=(const CubeScene::Extensions&);

  void query();

public:

  bool have_separate_specular_color;
  bool have_generate_mipmap;
  bool have_draw_range_elements;
  bool have_multi_draw_arrays;
  bool have_depth_clamp;

  PFNGLDRAWRANGEELEMENTSPROC  DrawRangeElements;
  PFNGLMULTIDRAWARRAYSPROC    MultiDrawArrays;

  Extensions() { query(); }
  virtual ~Extensions();
};

CubeScene::Extensions::~Extensions()
{}

void CubeScene::Extensions::query()
{
  have_separate_specular_color = false;
  have_generate_mipmap         = false;
  have_draw_range_elements     = false;
  have_multi_draw_arrays       = false;
  have_depth_clamp             = false;

  DrawRangeElements = 0;
  MultiDrawArrays   = 0;

  if (have_version(1, 2) || have_extension("GL_EXT_separate_specular_color"))
    have_separate_specular_color = true;

  if (have_version(1, 4) || have_extension("GL_SGIS_generate_mipmap"))
    have_generate_mipmap = true;

  if (have_version(1, 2))
  {
    if (GL::get_proc_address(DrawRangeElements, "glDrawRangeElements"))
      have_draw_range_elements = true;
  }
  else if (have_extension("GL_EXT_draw_range_elements"))
  {
    if (GL::get_proc_address(DrawRangeElements, "glDrawRangeElementsEXT"))
      have_draw_range_elements = true;
  }

  if (have_version(1, 4))
  {
    if (GL::get_proc_address(MultiDrawArrays, "glMultiDrawArrays"))
      have_multi_draw_arrays = true;
  }
  else if (have_extension("GL_EXT_multi_draw_arrays"))
  {
    if (GL::get_proc_address(MultiDrawArrays, "glMultiDrawArraysEXT"))
      have_multi_draw_arrays = true;
  }

  if (have_extension("GL_NV_depth_clamp"))
  {
    have_depth_clamp = true;
  }
}

inline
const CubeScene::Extensions* CubeScene::gl_ext() const
{
  return static_cast<const CubeScene::Extensions*>(GL::Scene::gl_ext());
}

CubeScene::CubeScene()
:
  rotation_               (),

  cube_pieces_            (),
  animation_data_         (),
  piece_cells_            (Cube::N * Cube::N * Cube::N),
  depth_order_            (),

  signal_cycle_finished_  (),
  animation_timer_        (),
  frame_trigger_          (),
  delay_timeout_          (),
  hide_cursor_timeout_    (),

  heading_                (create_layout_texture()),
  footing_                (create_layout_texture()),

  cube_texture_           (0),
  wireframe_list_         (0),
  piece_list_base_        (0),
  piece_list_count_       (0),

  track_last_x_           (TRACK_UNSET),
  track_last_y_           (TRACK_UNSET),
  cursor_state_           (CURSOR_DEFAULT),

  animation_piece_        (0),
  exclusive_piece_        (0),
  animation_seek_         (1.0),
  animation_position_     (0.0),
  animation_delay_        (1.0 / 3.0),

  zoom_                   (1.0),
  rotation_angle_         (0.0),
  frames_per_sec_         (60.0),
  pieces_per_sec_         (1.0),

  depth_order_changed_    (false),
  animation_running_      (false),
  show_wireframe_         (false),
  show_outline_           (false),
  zoom_visible_           (true)
{
  piece_buffers_[0] = 0;
  piece_buffers_[1] = 0;
  wireframe_buffers_[0] = 0;
  wireframe_buffers_[1] = 0;

  heading_->set_color(0xD8, 0xD8, 0xD8);
  footing_->set_color(0xA6, 0xA6, 0xA6);

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
      ScopeContext context (*this);

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
    animation_data_.assign(cube_pieces.size(), AnimationData());
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

  if (is_realized())
  {
    ScopeContext context (*this);

    gl_update_cube_pieces();
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
      ScopeContext context (*this);

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
  rotation_.renormalize(8.0f * std::numeric_limits<float>::epsilon());

  // Precompute the angle for glRotatef()
  rotation_angle_ = rotation_.angle() / radians_per_degree;

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
        ScopeContext context (*this);

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
      ScopeContext context (*this);

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

  for (std::vector<AnimationData>::const_iterator p = animation_data_.begin();
       p != animation_data_.end(); ++p)
  {
    cube_triangle_count += p->triangle_count;
  }

  return cube_triangle_count;
}

int CubeScene::get_cube_vertex_count() const
{
  int cube_vertex_count = 0;

  for (std::vector<AnimationData>::const_iterator p = animation_data_.begin();
       p != animation_data_.end(); ++p)
  {
    cube_vertex_count += p->get_vertex_count();
  }

  return cube_vertex_count;
}

/*
 * Rotate the cube around the axis which is currently closest to the one
 * specified via the axis parameter (where 0=x, 1=y, 2=z) in an unrotated
 * coordinate system.  In other words, pretend the cube has not been rotated
 * yet at all.
 *
 * Example:  If the cube was turned on its front side (rotated 270 degrees
 * counterclockwise around the x-axis), the next call to rotate() with axis = 1
 * will rotate the cube not around the y-axis but around the inverted z-axis
 * instead (as the latter is now pointing upwards).
 */
void CubeScene::rotate(int axis, float angle)
{
  g_return_if_fail(axis >= Cube::AXIS_X && axis <= Cube::AXIS_Z);

  const Math::Matrix4 matrix = Math::Quat::to_matrix(rotation_);
  int imax = axis;

  for (int i = 0; i < 3; ++i)
  {
    if (std::abs(matrix[i][axis]) > std::abs(matrix[imax][axis]))
      imax = i;
  }

  Math::Vector4 a;
  a[imax] = (matrix[imax][axis] < 0.0f) ? -1.0f : 1.0f;

  set_rotation(rotation_ * Math::Quat::from_axis(a, angle * radians_per_degree));
}

void CubeScene::gl_initialize()
{
  GL::Scene::gl_initialize();

  // Set up global parameters which relate to rendering performance.  Note
  // that even with backface culling, the application is still fill-limited.
  // Despite of that, perspective-correct interpolation does not measurably
  // impact the framerate on my system.

  glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

  if (gl_ext()->have_generate_mipmap)
    glHint(GL_GENERATE_MIPMAP_HINT, GL_NICEST);

  glEnable(GL_CULL_FACE);

  // Trade viewspace clipping for depth clamping to avoid highly visible
  // volume clipping artifacts.  The clamping could potentially produce some
  // artifacts of its own, but so far it appears to play along nicely.
  if (gl_ext()->have_depth_clamp)
    glEnable(GL_DEPTH_CLAMP_NV);

  // Set up lighting parameters for the cube pieces.  If supported, enable
  // the separate specular color, so that the specular term of the lighting
  // calculation is applied independently of texturing.

  if (gl_ext()->have_separate_specular_color)
    glLightModeli(GL_LIGHT_MODEL_COLOR_CONTROL, GL_SEPARATE_SPECULAR_COLOR);

  static const GLfloat scene_ambient [4] = { 0.25, 0.25, 0.25, 1.0 };
  static const GLfloat light_position[4] = { 0.0,  1.0,  4.0,  0.0 };

  glLightModelfv(GL_LIGHT_MODEL_AMBIENT, scene_ambient);
  glLightfv(GL_LIGHT0, GL_POSITION, light_position);
  glEnable(GL_LIGHT0);

  // Use a single global shininess for all lit surfaces.
  glMateriali(GL_FRONT_AND_BACK, GL_SHININESS, 8);

  GL::Error::check();

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

  gl_update_wireframe();
  gl_update_cube_pieces();
}

void CubeScene::gl_cleanup()
{
  gl_delete_cube_pieces();
  gl_delete_wireframe();

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

  glDisableClientState(GL_VERTEX_ARRAY);
  glDisableClientState(GL_NORMAL_ARRAY);
  glDisableClientState(GL_TEXTURE_COORD_ARRAY);

  if (gl_ext()->have_vertex_buffer_object)
  {
    gl_ext()->BindBuffer(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
    gl_ext()->BindBuffer(GL_ARRAY_BUFFER_ARB, 0);
  }

  glDisable(GL_TEXTURE_2D);
  glDisable(GL_LIGHTING);
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

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Divide the z offset by the zoom factor to account for the scaling of
    // the projection matrix.  The combined transformation then yields the
    // same result as if we had scaled the modelview matrix directly.  Note
    // that this trick only works with a directional light model.
    glTranslatef(0.0, 0.0, view_z_offset / zoom_);

    // According to the OpenGL Programming Guide, Appendix F, Rotation:
    // "The R matrix is always defined. If x=y=z=0, then R is the identity
    // matrix."  Thus it's safe not to special-case the identity rotation.
    glRotatef(rotation_angle_, rotation_.x(), rotation_.y(), rotation_.z());

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

  const double width  = Math::max(1, get_width());
  const double height = Math::max(1, get_height());

  // Place the far clipping plane so that the cube origin will be
  // positioned halfway between the near and far clipping planes.
  const double clip_near = 1.0;
  const double clip_far  = -view_z_offset * 2.0 - clip_near;

  gluPerspective(field_of_view, width / height, clip_near, clip_far);

  // Thanks to the simple directional light model we use, the zoom operation
  // can be implemented by scaling the view distance and the projection matrix.
  // This way, we can avoid GL_NORMALIZE without having to recompute the whole
  // vertex data everytime after a zoom operation.
  glScalef(zoom_, zoom_, zoom_);

  GL::Error::check();
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
        case GDK_Left:  case GDK_KP_Left:   rotate(Cube::AXIS_Y, -rotation_step); return true;
        case GDK_Right: case GDK_KP_Right:  rotate(Cube::AXIS_Y,  rotation_step); return true;
        case GDK_Up:    case GDK_KP_Up:     rotate(Cube::AXIS_X, -rotation_step); return true;
        case GDK_Down:  case GDK_KP_Down:   rotate(Cube::AXIS_X,  rotation_step); return true;
        case GDK_Begin: case GDK_KP_Begin:
        case GDK_5:     case GDK_KP_5:      set_rotation(Math::Quat()); return true;
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

void CubeScene::setup_gl_context()
{
  GL::configure_widget(*this, GDK_GL_MODE_RGBA | GDK_GL_MODE_DEPTH | GDK_GL_MODE_DOUBLE);
}

GL::Extensions* CubeScene::gl_query_extensions()
{
  return new CubeScene::Extensions();
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

  static const unsigned char order[N*N*N][3] =
  {
    {2,0,2}, {1,0,2}, {2,0,1}, {1,0,1}, {1,1,1}, {2,1,2}, {1,1,2},
    {2,1,1}, {0,0,2}, {2,0,0}, {2,2,2}, {0,0,1}, {1,0,0}, {0,1,2},
    {2,1,0}, {1,2,2}, {2,2,1}, {0,1,1}, {1,1,0}, {1,2,1}, {0,0,0},
    {0,2,2}, {2,2,0}, {0,1,0}, {0,2,1}, {1,2,0}, {0,2,0}
  };

  unsigned int count = 0;
  Cube         cube;

  for (unsigned int i = 0; i < G_N_ELEMENTS(order); ++i)
  {
    const unsigned int cell_index = N*N * order[i][0] + N * order[i][1] + order[i][2];

    Cube cell;
    cell.put(order[i][0], order[i][1], order[i][2], true);

    g_return_if_fail(cell_index < piece_cells_.size());

    piece_cells_[cell_index].piece = G_MAXUINT;
    piece_cells_[cell_index].cell  = cell_index;

    // 1) Find the cube piece which occupies this cell.
    // 2) Look it up in the already processed range of the animation data.
    // 3) If not processed yet, generate and store a new animation data element.
    // 4) Write the piece's animation index to the piece cells vector.

    const std::vector<Cube>::iterator pcube = std::find_if(cube_pieces_.begin(), cube_pieces_.end(),
                                                           Util::DoesIntersect<Cube>(cell));
    if (pcube != cube_pieces_.end())
    {
      const unsigned int cube_index = pcube - cube_pieces_.begin();
      unsigned int       anim_index = 0;

      while (anim_index < count && animation_data_[anim_index].cube_index != cube_index)
        ++anim_index;

      if (anim_index == count)
      {
        g_return_if_fail((cube & *pcube) == Cube());           // collision
        g_return_if_fail(anim_index < animation_data_.size()); // invalid input

        animation_data_[anim_index].cube_index = cube_index;
        find_animation_axis(cube, *pcube, animation_data_[anim_index].direction);

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

  float  zcoords[N*N*N];
  float* pcell = zcoords;

  for (int x = 1 - N; x < N; x += 2)
    for (int y = 1 - N; y < N; y += 2)
      for (int z = N - 1; z > -N; z -= 2)
      {
        const Math::Vector4 coords = matrix * Math::Vector4(x, y, z);

        *pcell++ = coords.z();
      }

  std::sort(piece_cells_.begin(), piece_cells_.end(), CellAboveZ(zcoords));

  Cube cube;
  std::vector<int>::iterator pdepth = depth_order_.begin();

  g_return_if_fail(pdepth != depth_order_.end());

  for (std::vector<PieceCell>::iterator p = piece_cells_.begin(); p != piece_cells_.end(); ++p)
  {
    if (p->piece < animation_data_.size())
    {
      const unsigned int index = animation_data_[p->piece].cube_index;

      g_return_if_fail(index < cube_pieces_.size());

      if ((cube & cube_pieces_[index]) == Cube())
      {
        cube |= cube_pieces_[index];
        *pdepth = p->piece;

        if (++pdepth == depth_order_.end())
          break;
      }
    }
  }

  g_return_if_fail(pdepth == depth_order_.end());

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
    const Glib::RefPtr<Gdk::Window> window = get_window();

    switch (state)
    {
      case CURSOR_DEFAULT:
        window->set_cursor();
        break;

      case CURSOR_DRAGGING:
        window->set_cursor(Gdk::Cursor(get_display(), Gdk::FLEUR));
        break;

      case CURSOR_INVISIBLE:
        {
          const char *const bitmap_data = "";

          const Glib::RefPtr<Gdk::Pixmap> bitmap = Gdk::Bitmap::create(window, bitmap_data, 1, 1);
          const Gdk::Color color;

          // Use the empty 1x1 bitmap to set up an invisible cursor.
          window->set_cursor(Gdk::Cursor(bitmap, bitmap, color, color, 0, 0));
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
  Util::MemChunk<GLfloat>        vertices (3 * WIREFRAME_VERTEX_COUNT);
  Util::MemChunk<WireframeIndex> indices  (2 * WIREFRAME_LINE_COUNT);

  enum { N = Cube::N + 1 };

  float stride[N];

  for (int i = 0; i < N; ++i)
    stride[i] = (2 * i - (N - 1)) * (cube_cell_size / 2.0f);

  Util::MemChunk<GLfloat>::iterator pv = vertices.begin();

  for (int z = 0; z < N; ++z)
    for (int y = 0; y < N; ++y)
      for (int x = 0; x < N; ++x)
      {
        pv[0] = stride[x];
        pv[1] = stride[y];
        pv[2] = stride[z];

        pv += 3;
      }

  Util::MemChunk<WireframeIndex>::iterator pi = indices.begin();

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

  if (gl_ext()->have_vertex_buffer_object)
  {
    g_return_if_fail(wireframe_buffers_[0] == 0 && wireframe_buffers_[1] == 0);

    gl_ext()->GenBuffers(2, wireframe_buffers_);

    GL::Error::throw_if_fail(wireframe_buffers_[0] != 0 && wireframe_buffers_[1] != 0);

    gl_ext()->BindBuffer(GL_ARRAY_BUFFER_ARB, wireframe_buffers_[0]);
    gl_ext()->BufferData(GL_ARRAY_BUFFER_ARB, vertices.bytes(), &vertices[0], GL_STATIC_DRAW_ARB);

    gl_ext()->BindBuffer(GL_ELEMENT_ARRAY_BUFFER_ARB, wireframe_buffers_[1]);
    gl_ext()->BufferData(GL_ELEMENT_ARRAY_BUFFER_ARB, indices.bytes(), &indices[0], GL_STATIC_DRAW_ARB);

    gl_ext()->BindBuffer(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
    gl_ext()->BindBuffer(GL_ARRAY_BUFFER_ARB, 0);
  }
  else
  {
    g_return_if_fail(wireframe_list_ == 0);

    wireframe_list_ = glGenLists(1);

    GL::Error::throw_if_fail(wireframe_list_ != 0);

    glVertexPointer(3, GL_FLOAT, 0, &vertices[0]);
    glEnableClientState(GL_VERTEX_ARRAY);

    {
      GL::ScopeList list (wireframe_list_, GL_COMPILE);

      gl_draw_wireframe_elements(&indices[0]);
    }

    glDisableClientState(GL_VERTEX_ARRAY);
  }

  GL::Error::check();
}

void CubeScene::gl_delete_wireframe()
{
  if (gl_ext()->have_vertex_buffer_object)
  {
    if (wireframe_buffers_[0] || wireframe_buffers_[1])
    {
      gl_ext()->DeleteBuffers(2, wireframe_buffers_);
      wireframe_buffers_[0] = 0;
      wireframe_buffers_[1] = 0;
    }
  }
  else
  {
    if (wireframe_list_)
    {
      glDeleteLists(wireframe_list_, 1);
      wireframe_list_ = 0;
    }
  }
}

void CubeScene::gl_draw_wireframe_elements(void* indices)
{
  static const GLubyte wireframe_color[3] = { 0x47, 0x47, 0x47 };

  glColor3ubv(wireframe_color);

  if (gl_ext()->have_draw_range_elements)
  {
    gl_ext()->DrawRangeElements(GL_LINES, 0, WIREFRAME_VERTEX_COUNT - 1,
                                2 * WIREFRAME_LINE_COUNT, WIREFRAME_INDEX_TYPE, indices);
  }
  else
  {
    glDrawElements(GL_LINES, 2 * WIREFRAME_LINE_COUNT, WIREFRAME_INDEX_TYPE, indices);
  }
}

void CubeScene::gl_draw_wireframe()
{
  if (gl_ext()->have_vertex_buffer_object)
  {
    if (wireframe_buffers_[0] && wireframe_buffers_[1])
    {
      gl_ext()->BindBuffer(GL_ARRAY_BUFFER_ARB, wireframe_buffers_[0]);
      glVertexPointer(3, GL_FLOAT, 0, GL::buffer_offset(0));
      glEnableClientState(GL_VERTEX_ARRAY);

      gl_ext()->BindBuffer(GL_ELEMENT_ARRAY_BUFFER_ARB, wireframe_buffers_[1]);

      gl_draw_wireframe_elements(GL::buffer_offset(0));

      gl_ext()->BindBuffer(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);

      glDisableClientState(GL_VERTEX_ARRAY);
      gl_ext()->BindBuffer(GL_ARRAY_BUFFER_ARB, 0);
    }
  }
  else
  {
    if (wireframe_list_)
      glCallList(wireframe_list_);
  }
}

int CubeScene::gl_draw_cube() const
{
  int triangle_count = 0;

  if (animation_piece_ > 0 && animation_piece_ <= int(animation_data_.size()))
  {
    glEnable(GL_LIGHTING);

    if (!show_outline_)
    {
      glBindTexture(GL_TEXTURE_2D, cube_texture_);
      glEnable(GL_TEXTURE_2D);

      triangle_count += gl_draw_pieces();

      glDisable(GL_TEXTURE_2D);
    }
    else
    {
      glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

      triangle_count += gl_draw_pieces();

      glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    glDisable(GL_LIGHTING);
  }

  return triangle_count;
}

int CubeScene::gl_draw_pieces() const
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

  int triangle_count = 0;

  if (last >= first)
  {
    if (gl_ext()->have_vertex_buffer_object)
      triangle_count = gl_draw_piece_buffer_range(first, last);
    else
      triangle_count = gl_draw_piece_list_range(first, last);
  }

  return triangle_count;
}

inline
void CubeScene::gl_translate_animated_piece(const float* direction) const
{
  // Distance in model units an animated cube piece has to travel.
  const float animation_distance = 1.75 * Cube::N * cube_cell_size;

  const float d = animation_position_ * animation_distance / zoom_;

  glTranslatef(direction[0] * d, direction[1] * d, direction[2] * d);
}

void CubeScene::gl_draw_piece_elements(const AnimationData& data) const
{
  gl_set_piece_material(data.cube_index);

  if (gl_ext()->have_draw_range_elements)
  {
    gl_ext()->DrawRangeElements(GL_TRIANGLES, data.element_first, data.element_last,
                                3 * data.triangle_count, CUBE_INDEX_TYPE,
                                GL::buffer_offset(data.indices_offset * sizeof(CubeIndex)));
  }
  else
  {
    glDrawElements(GL_TRIANGLES, 3 * data.triangle_count, CUBE_INDEX_TYPE,
                   GL::buffer_offset(data.indices_offset * sizeof(CubeIndex)));
  }
}

int CubeScene::gl_draw_piece_buffer_range(int first, int last) const
{
  int triangle_count = 0;

  if (piece_buffers_[0] && piece_buffers_[1])
  {
    gl_ext()->BindBuffer(GL_ARRAY_BUFFER_ARB, piece_buffers_[0]);
    glInterleavedArrays(CUBE_ELEMENT_TYPE, 0, GL::buffer_offset(0));

    gl_ext()->BindBuffer(GL_ELEMENT_ARRAY_BUFFER_ARB, piece_buffers_[1]);

    int last_fixed = last;

    if (animation_position_ > 0.0f && last == animation_piece_ - 1)
      --last_fixed;

    if (last_fixed >= first)
    {
      for (std::vector<int>::const_iterator p = depth_order_.begin(); p != depth_order_.end(); ++p)
      {
        if (*p >= first && *p <= last_fixed)
        {
          const AnimationData& data = animation_data_[*p];

          triangle_count += data.triangle_count;

          gl_draw_piece_elements(data);
        }
      }
    }

    if (last != last_fixed)
    {
      const AnimationData& data = animation_data_[last];

      triangle_count += data.triangle_count;

      gl_translate_animated_piece(data.direction);
      gl_draw_piece_elements(data);
    }

    gl_ext()->BindBuffer(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);

    gl_ext()->BindBuffer(GL_ARRAY_BUFFER_ARB, 0);
  }

  return triangle_count;
}

int CubeScene::gl_draw_piece_list_range(int first, int last) const
{
  int triangle_count = 0;

  if (piece_list_base_ && last < piece_list_count_)
  {
    int last_fixed = last;

    if (animation_position_ > 0.0f && last == animation_piece_ - 1)
      --last_fixed;

    if (last_fixed >= first)
    {
      for (std::vector<int>::const_iterator p = depth_order_.begin(); p != depth_order_.end(); ++p)
      {
        if (*p >= first && *p <= last_fixed)
        {
          const AnimationData& data = animation_data_[*p];

          triangle_count += data.triangle_count;

          glCallList(piece_list_base_ + *p);
        }
      }
    }

    if (last != last_fixed)
    {
      const AnimationData& data = animation_data_[last];

      triangle_count += data.triangle_count;

      gl_translate_animated_piece(data.direction);
      glCallList(piece_list_base_ + last);
    }
  }

  return triangle_count;
}

void CubeScene::gl_init_cube_texture()
{
  g_return_if_fail(cube_texture_ == 0);

  // No matter what the real dimensions of the input image are, scale the
  // texture during load to the fixed size defined here.  Forcing a fixed
  // width and height actually enhances flexibility, as the user may drop
  // in whatever image file without wreaking havoc.  In essence, the size
  // of a texture is simply a quality setting unrelated to the input data.

  enum { WIDTH = 128, HEIGHT = 128 };

  const Glib::RefPtr<Gdk::Pixbuf> pixbuf =
      Gdk::Pixbuf::create_from_file(cube_texture_filename, WIDTH, HEIGHT, false);

  g_return_if_fail(pixbuf->get_width() == WIDTH && pixbuf->get_height() == HEIGHT);
  g_return_if_fail(pixbuf->get_bits_per_sample() == 8);

  Util::MemChunk<GLubyte> tex_pixels (HEIGHT * WIDTH);

  const guint8 *const buf_pixels = pixbuf->get_pixels();

  const std::ptrdiff_t buf_rowstride = pixbuf->get_rowstride();
  const std::ptrdiff_t buf_channels  = pixbuf->get_n_channels();

  for (int row = 0; row < HEIGHT; ++row)
  {
    const guint8* pbuf = &buf_pixels[(HEIGHT - 1 - row) * buf_rowstride];
    GLubyte*      ptex = &tex_pixels[row * WIDTH];

    for (int col = WIDTH; col > 0; --col)
    {
      // Read only the first channel, assuming gray-scale.
      *ptex = *pbuf;

      pbuf += buf_channels;
      ptex += 1;
    }
  }

  glGenTextures(1, &cube_texture_);

  GL::Error::throw_if_fail(cube_texture_ != 0);

  glBindTexture(GL_TEXTURE_2D, cube_texture_);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);

  if (gl_ext()->have_generate_mipmap)
  {
    glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE8, WIDTH, HEIGHT, 0,
                 GL_LUMINANCE, GL_UNSIGNED_BYTE, &tex_pixels[0]);
  }
  else
  {
    const int result = gluBuild2DMipmaps(GL_TEXTURE_2D, GL_LUMINANCE8, WIDTH, HEIGHT,
                                         GL_LUMINANCE, GL_UNSIGNED_BYTE, &tex_pixels[0]);
    if (result != GL_NO_ERROR)
      throw GL::Error(result);
  }

  GL::Error::check();
}

void CubeScene::update_footing()
{
  const int percentage = int(100.0f * zoom_ + 0.5f);

  if (zoom_visible_ && percentage != 100)
  {
    std::ostringstream output;
    output << "Zoom " << percentage << '%';
    footing_->set_content(Glib::locale_to_utf8(output.str()));
  }
  else
    footing_->set_content(Glib::ustring());
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

void CubeScene::gl_update_cube_pieces()
{
  gl_delete_cube_pieces();

  const unsigned int piece_count = cube_pieces_.size();

  if (piece_count > 0)
  {
    g_return_if_fail(piece_count == animation_data_.size());

    try
    {
      if (gl_ext()->have_vertex_buffer_object)
        gl_create_piece_buffers();
      else
        gl_create_piece_lists();
    }
    catch (...)
    {
      gl_reset_state();
      gl_delete_cube_pieces();
      throw;
    }
  }
}

void CubeScene::gl_delete_cube_pieces()
{
  if (gl_ext()->have_vertex_buffer_object)
  {
    if (piece_buffers_[0] || piece_buffers_[1])
    {
      gl_ext()->DeleteBuffers(2, piece_buffers_);
      piece_buffers_[0] = 0;
      piece_buffers_[1] = 0;
    }
  }
  else
  {
    if (piece_list_base_)
    {
      glDeleteLists(piece_list_base_, piece_list_count_);
      piece_list_base_  = 0;
      piece_list_count_ = 0;
    }
  }
}

void CubeScene::gl_create_piece_buffers()
{
  CubeElementArray  element_array;
  CubeIndexArray    index_array;

  element_array.reserve(2048);
  index_array.reserve(10240);

  CubeTesselator tesselator;

  tesselator.set_element_array(&element_array);
  tesselator.set_index_array(&index_array);
  tesselator.set_cellsize(cube_cell_size);

  for (std::vector<AnimationData>::iterator p = animation_data_.begin();
       p != animation_data_.end(); ++p)
  {
    const unsigned int offset = index_array.size();
    const unsigned int first  = element_array.size();

    const unsigned int cube_index = p->cube_index;

    g_return_if_fail(cube_index < cube_pieces_.size());

    tesselator.run(cube_pieces_[cube_index]);

    const int count = tesselator.reset_triangle_count();

    g_return_if_fail(3 * count == int(index_array.size() - offset));

    p->triangle_count = count;
    p->indices_offset = offset;
    p->element_first  = first;
    p->element_last   = element_array.size() - 1;
  }

  g_return_if_fail(piece_buffers_[0] == 0 && piece_buffers_[1] == 0);

  gl_ext()->GenBuffers(2, piece_buffers_);

  GL::Error::throw_if_fail(piece_buffers_[0] != 0 && piece_buffers_[1] != 0);

  gl_ext()->BindBuffer(GL_ARRAY_BUFFER_ARB, piece_buffers_[0]);
  gl_ext()->BufferData(GL_ARRAY_BUFFER_ARB,
                       element_array.size() * sizeof(CubeElement),
                       &element_array[0], GL_STATIC_DRAW_ARB);

  gl_ext()->BindBuffer(GL_ELEMENT_ARRAY_BUFFER_ARB, piece_buffers_[1]);
  gl_ext()->BufferData(GL_ELEMENT_ARRAY_BUFFER_ARB,
                       index_array.size() * sizeof(CubeIndex),
                       &index_array[0], GL_STATIC_DRAW_ARB);

  gl_ext()->BindBuffer(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
  gl_ext()->BindBuffer(GL_ARRAY_BUFFER_ARB, 0);

  GL::Error::check();
}

void CubeScene::gl_create_piece_lists()
{
  CubeElementArray  element_array;
  RangeStartArray   start_array;
  RangeCountArray   count_array;

  element_array.reserve(768);
  start_array.reserve(96);
  count_array.reserve(96);

  CubeTesselator tesselator;

  tesselator.set_element_array(&element_array);
  tesselator.set_range_arrays(&start_array, &count_array);
  tesselator.set_cellsize(cube_cell_size);

  const int piece_count = animation_data_.size();

  g_return_if_fail(piece_list_base_ == 0);

  piece_list_base_ = glGenLists(piece_count);

  GL::Error::throw_if_fail(piece_list_base_ != 0);

  piece_list_count_ = piece_count;

  for (int i = 0; i < piece_count; ++i)
  {
    const unsigned int cube_index = animation_data_[i].cube_index;

    g_return_if_fail(cube_index < cube_pieces_.size());

    tesselator.run(cube_pieces_[cube_index]);

    g_return_if_fail(!element_array.empty() && !start_array.empty());
    g_return_if_fail(start_array.size() == count_array.size());

    animation_data_[i].triangle_count = tesselator.reset_triangle_count();
    animation_data_[i].indices_offset = 0;
    animation_data_[i].element_first  = 0;
    animation_data_[i].element_last   = element_array.size() - 1;

    glInterleavedArrays(CUBE_ELEMENT_TYPE, 0, &element_array[0]);

    {
      GL::ScopeList list (piece_list_base_ + i, GL_COMPILE);

      gl_set_piece_material(cube_index);

      const int strip_count = start_array.size();

      if (gl_ext()->have_multi_draw_arrays)
      {
        gl_ext()->MultiDrawArrays(GL_TRIANGLE_STRIP,
                                  &start_array[0], &count_array[0], strip_count);
      }
      else
      {
        for (int k = 0; k < strip_count; ++k)
          glDrawArrays(GL_TRIANGLE_STRIP, start_array[k], count_array[k]);
      }
    }

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);

    GL::Error::check();

    count_array.clear();
    start_array.clear();
    element_array.clear();
  }
}

} // namespace Somato
