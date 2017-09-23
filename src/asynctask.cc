/*
 * Copyright (c) 2017  Daniel Elstner  <daniel.kitta@gmail.com>
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
#include "asynctask.h"

#include <glib.h>

namespace Async
{

Task::Task()
{}

void Task::wait_finish()
{
  if (thread_.joinable())
  {
    dtor_loop_ = Glib::MainLoop::create();
    dtor_loop_->run();
  }
}

Task::~Task()
{
  g_warn_if_fail(!running());
}

void Task::run()
{
  g_return_if_fail(!running());

  error_  = nullptr;
  thread_ = std::thread{std::bind(&Task::execute_task, this)};
}

std::exception_ptr Task::error() const
{
  return error_;
}

void Task::rethrow_any_error() const
{
  g_return_if_fail(!running());

  if (error_)
    std::rethrow_exception(error_);
}

void Task::execute_task()
{
  try
  {
    execute();
  }
  catch (...)
  {
    error_ = std::current_exception();
  }
  // Notify the main thread that we are done.
  Glib::signal_idle().connect_once(std::bind(&Task::task_finished, this));
}

void Task::task_finished()
{
  thread_.join();

  if (dtor_loop_)
    dtor_loop_->quit();
  else
    signal_done_(); // emit
}

} // namespace Async
