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

#ifndef SOMATO_CUBESCENE_H_INCLUDED
#define SOMATO_CUBESCENE_H_INCLUDED

#include "glscene.h"
#include "cube.h"
#include "glshader.h"
#include "meshloader.h"
#include "puzzle.h"
#include "vectormath.h"

#include <sigc++/sigc++.h>
#include <glibmm/ustring.h>
#include <memory>
#include <vector>

namespace Gtk { class Builder; }

namespace Somato
{

struct MeshData
{
  unsigned int triangle_count = 0; // number of triangles
  unsigned int indices_offset = 0; // offset into element indices array
  unsigned int element_first  = 0; // minimum referenced element index
  unsigned int element_last   = 0; // maximum referenced element index

  unsigned int element_count() const { return element_last - element_first + 1; }
};

struct AnimationData
{
  Math::Matrix4 transform;      // puzzle piece orientation
  unsigned int  cube_index = 0; // index into pieces vector in original order
  float         direction[3] = {0., 0., 0.}; // animation move direction
};

struct PieceCell
{
  unsigned int piece = 0; // animation index of cube piece
  unsigned int cell  = 0; // linearized index of cube cell
};

typedef std::vector<PieceCell> PieceCellVector;

class CubeScene : public GL::Scene
{
public:
  CubeScene(BaseObjectType* obj, const Glib::RefPtr<Gtk::Builder>& ui);
  virtual ~CubeScene();

  sigc::signal<void>& signal_cycle_finished() { return signal_cycle_finished_; }

  void set_heading(const Glib::ustring& heading);
  Glib::ustring get_heading() const;

  void set_cube_pieces(const Solution& cube_pieces);

  void  set_zoom(float zoom);
  float get_zoom() const;

  void set_rotation(const Math::Quat& rotation);
  Math::Quat get_rotation() const;

  void  set_animation_delay(float animation_delay);
  float get_animation_delay() const;

  void  set_pieces_per_second(float pieces_per_second);
  float get_pieces_per_second() const;

  void set_animation_running(bool animation_running);
  bool get_animation_running() const;

  void set_zoom_visible(bool zoom_visible);
  bool get_zoom_visible() const;

  void set_show_cell_grid(bool show_cell_grid);
  bool get_show_cell_grid() const;

  void set_show_outline(bool show_outline);
  bool get_show_outline() const;

  int get_cube_triangle_count() const;
  int get_cube_vertex_count() const;

  void rotate(int axis, float angle);

protected:
  void gl_initialize() override;
  void gl_cleanup() override;
  void gl_reset_state() override;
  int  gl_render() override;
  void gl_update_projection() override;

  void on_size_allocate(Gtk::Allocation& allocation) override;
  bool on_visibility_notify_event(GdkEventVisibility* event) override;
  bool on_enter_notify_event(GdkEventCrossing* event) override;
  bool on_leave_notify_event(GdkEventCrossing* event) override;
  bool on_key_press_event(GdkEventKey* event) override;
  bool on_key_release_event(GdkEventKey* event) override;
  bool on_button_press_event(GdkEventButton* event) override;
  bool on_button_release_event(GdkEventButton* event) override;
  bool on_motion_notify_event(GdkEventMotion* event) override;

private:
  bool on_animation_tick(gint64 animation_time) override;
  void gl_reposition_layouts() override;

  typedef std::array<GL::MeshLoader::Node, CUBE_PIECE_COUNT> MeshNodeArray;

  enum CursorState
  {
    CURSOR_DEFAULT,
    CURSOR_DRAGGING,
    CURSOR_INVISIBLE
  };
  enum : int {
    TRACK_UNSET = G_MININT  // integer indeterminate
  };

  Math::Quat                  rotation_;

  std::vector<SomaCube>       cube_pieces_;
  std::vector<MeshData>       mesh_data_;
  std::vector<AnimationData>  animation_data_;
  PieceCellVector             piece_cells_;
  std::vector<int>            depth_order_;

  sigc::signal<void>          signal_cycle_finished_;
  sigc::connection            delay_timeout_;
  sigc::connection            hide_cursor_timeout_;

  std::unique_ptr<GL::MeshLoader> mesh_loader_;

  GL::LayoutTexture*          heading_;
  GL::LayoutTexture*          footing_;

  GL::ShaderProgram           piece_shader_;
  int                         uf_modelview_         = -1;
  int                         uf_projection_        = -1;
  int                         uf_diffuse_material_  = -1;
  int                         uf_piece_texture_     = -1;

  GL::ShaderProgram           grid_shader_;
  int                         grid_uf_modelview_    = -1;
  int                         grid_uf_projection_   = -1;

  unsigned int                cube_texture_         = 0;
  unsigned int                mesh_buffers_[2]      = {0, 0};
  unsigned int                mesh_vertex_array_    = 0;

  int                         track_last_x_         = TRACK_UNSET;
  int                         track_last_y_         = TRACK_UNSET;
  CursorState                 cursor_state_         = CURSOR_DEFAULT;

  int                         animation_piece_      = 0;
  int                         exclusive_piece_      = 0;
  float                       animation_seek_       = 1.;
  float                       animation_position_   = 0.;
  float                       animation_delay_      = 1. / 3.;

  float                       zoom_                 = 1.;
  float                       pieces_per_sec_       = 1.;

  bool                        pointer_inside_       = false;
  bool                        depth_order_changed_  = false;
  bool                        animation_running_    = false;
  bool                        show_cell_grid_       = false;
  bool                        show_outline_         = false;
  bool                        zoom_visible_         = true;

  void update_footing();
  void update_animation_order();
  void update_depth_order();
  void update_animation_timer();

  void start_piece_animation();
  void pause_animation();
  void continue_animation();
  void set_cursor(CursorState state);

  void reset_hide_cursor_timeout();
  bool on_hide_cursor_timeout();
  bool on_delay_timeout();

  void cycle_exclusive(int direction);
  void select_piece(int piece);
  void process_track_motion(int x, int y);

  void on_meshes_loaded();
  void gl_create_mesh_buffers(GL::MeshLoader& loader, const MeshNodeArray& nodes,
                              unsigned int total_vertices, unsigned int indices_size);
  void gl_create_piece_shader();
  void gl_create_grid_shader();
  void gl_generate_grid_vertices(volatile GL::MeshVertex* vertices);
  void gl_generate_grid_indices(volatile GL::MeshIndex* indices);
  void gl_draw_cell_grid(const Math::Matrix4& cube_transform);

  int  gl_draw_cube(const Math::Matrix4& cube_transform);
  int  gl_draw_pieces(const Math::Matrix4& cube_transform);
  int  gl_draw_pieces_range(const Math::Matrix4& cube_transform, int first, int last);
  void gl_draw_piece_elements(const Math::Matrix4& transform, const AnimationData& data);

  void gl_init_cube_texture();
};

} // namespace Somato

#endif // !SOMATO_CUBESCENE_H_INCLUDED
