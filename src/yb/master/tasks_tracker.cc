// Copyright (c) YugaByte, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except
// in compliance with the License.  You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software distributed under the License
// is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
// or implied.  See the License for the specific language governing permissions and limitations
// under the License.
//

#include "yb/gutil/strings/substitute.h"
#include "yb/master/tasks_tracker.h"
#include "yb/util/atomic.h"

DEFINE_int32(tasks_tracker_num_tasks, 100,
             "Number of most recent tasks to track for displaying in utilities UI.");

DEFINE_int32(tasks_tracker_keep_time_multiplier, 300,
             "How long we should keep tasks before cleaning them up, as a multiple of the "
             "load balancer interval (catalog_manager_bg_task_wait_ms).");

namespace yb {
namespace master {

using strings::Substitute;

TasksTracker::TasksTracker() : tasks_(FLAGS_tasks_tracker_num_tasks) {}

void TasksTracker::Reset() {
  std::lock_guard<decltype(lock_)> l(lock_);
  tasks_.clear();
}

void TasksTracker::AddTask(std::shared_ptr<MonitoredTask> task) {
  std::lock_guard<decltype(lock_)> l(lock_);
  tasks_.push_back(task);
}

std::vector<std::shared_ptr<MonitoredTask>> TasksTracker::GetTasks() {
  shared_lock<decltype(lock_)> l(lock_);
  std::vector<std::shared_ptr<MonitoredTask>> tasks;
  for (const auto& task : tasks_) {
    tasks.push_back(task);
  }
  return tasks;
}

void TasksTracker::CleanupOldTasks() {
  std::lock_guard<decltype(lock_)> l(lock_);
  for (boost::circular_buffer<std::shared_ptr<MonitoredTask>>::iterator iter = tasks_.begin();
       iter != tasks_.end(); ) {
    if (MonoTime::Now().GetDeltaSince((*iter)->start_timestamp()).ToMilliseconds() >
        GetAtomicFlag(&FLAGS_tasks_tracker_keep_time_multiplier) *
        FLAGS_catalog_manager_bg_task_wait_ms) {
      iter = tasks_.erase(iter);
    } else {
      // Tasks are implicitly sorted by time, so we can break once a task is within
      // the keep time.
      break;
    }
  }
}

std::string TasksTracker::ToString() {
  shared_lock<decltype(lock_)> l(lock_);
  return Substitute("TasksTracker has $0 tasks in buffer.",
                    tasks_.size());
}

} // namespace master
} // namespace yb
