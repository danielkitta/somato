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

#ifndef SOMATO_ASYNCTASK_H_INCLUDED
#define SOMATO_ASYNCTASK_H_INCLUDED

#include <sigc++/sigc++.h>
#include <glibmm/main.h>

#include <exception>
#include <functional>
#include <memory>
#include <thread>
#include <utility>

namespace Async
{

/* Synchronous encapsulation of an asynchrously executed task.
 *
 * The slot connected to signal_done() will be executed from the main
 * thread's event loop. Once the done notification has been received,
 * any memory modified during asynchronous execution is completely
 * visible and can be safely accessed.
 *
 * Any exception thrown by the asynchronous task will be caught and made
 * available synchronously via error() for inspection or re-throwing.
 */
class Task
{
public:
  virtual ~Task();

  Task(const Task& other) = delete;
  Task& operator=(const Task& other) = delete;

  sigc::signal<void>& signal_done() { return signal_done_; }

  void run();
  bool running() const { return thread_.joinable(); }

  std::exception_ptr error() const;
  void rethrow_any_error() const;

protected:
  Task();

  // To be called from derived classes' destructor. Normally, the task
  // should not be running anymore during destruction, but in case it is,
  // wait for it to finish in order to ensure proper cleanup.
  void wait_finish();

private:
  virtual void execute() = 0;

  void execute_task();
  void task_finished();

  sigc::signal<void>            signal_done_;
  Glib::RefPtr<Glib::MainLoop>  dtor_loop_;
  std::thread                   thread_;
  std::exception_ptr            error_ = nullptr;
};

/* A deferred delete functor for use with standard smart pointers. Object
 * destruction is delayed until the main event loop becomes idle again.
 * This is mainly useful in signal handlers for safely destroying the
 * signal's sender object.
 */
template <typename T, typename D = std::default_delete<T>>
class DeferredDelete
{
public:
  constexpr DeferredDelete() noexcept = default;

  DeferredDelete(DeferredDelete&&) noexcept = default;
  DeferredDelete& operator=(DeferredDelete&&) noexcept = default;

  DeferredDelete(D&& del) noexcept : del_ {del} {}
  DeferredDelete& operator=(D&& del) noexcept { del_ = del; return *this; }

  void operator()(T* ptr) const;

private:
  D del_;
};

template <typename T, typename D>
void DeferredDelete<T, D>::operator()(T* ptr) const
{
  if (ptr)
    Glib::signal_idle().connect_once(std::bind(del_, ptr), Glib::PRIORITY_HIGH);
}

/* Move ownership from a standard unique_ptr<> into a unique_ptr<>
 * with deferred delete.
 */
template <typename T, typename D> inline
std::unique_ptr<T, DeferredDelete<T, D>> deferred_delete(std::unique_ptr<T, D>& ptr)
{
  return std::move(ptr);
}

} // namespace Async

#endif // !SOMATO_ASYNCTASK_H_INCLUDED
