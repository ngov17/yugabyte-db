// Copyright (c) YugaByte, Inc.

package com.yugabyte.yw.commissioner.tasks;

import java.util.Collection;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.yugabyte.yw.commissioner.TaskList;
import com.yugabyte.yw.commissioner.TaskListQueue;
import com.yugabyte.yw.commissioner.UserTaskDetails.SubTaskType;
import com.yugabyte.yw.commissioner.tasks.params.UniverseTaskParams;
import com.yugabyte.yw.commissioner.tasks.subtasks.RemoveUniverseEntry;
import com.yugabyte.yw.models.Universe;
import com.yugabyte.yw.models.helpers.NodeDetails;

public class DestroyUniverse extends UniverseTaskBase {
  public static final Logger LOG = LoggerFactory.getLogger(DestroyUniverse.class);

  public static class Params extends UniverseTaskParams { }

  @Override
  protected Params taskParams() {
    return (Params)taskParams;
  }

  @Override
  public void run() {
    try {
      // Create the task list sequence.
      taskListQueue = new TaskListQueue();

      // Update the universe DB with the update to be performed and set the 'updateInProgress' flag
      // to prevent other updates from happening.
      Universe universe = lockUniverseForUpdate(-1 /* expectedUniverseVersion */);

      // Create tasks to destroy the existing nodes.
      createDestroyServerTasks(universe.getNodes()).setUserSubTask(SubTaskType.RemovingUnusedServers);

      // Create tasks to remove the universe entry from the Universe table.
      createRemoveUniverseEntryTask().setUserSubTask(SubTaskType.RemovingUnusedServers);

      // Run all the tasks.
      taskListQueue.run();
    } catch (Throwable t) {
      // Unlock the universe in case of an error. Ignore if the universe entry is not present in the
      // universe table.
      try {
        unlockUniverseForUpdate();
      } catch (Throwable t1) {
        // Ignore.
      }
      LOG.error("Error executing task {} with error='{}'.", getName(), t.getMessage(), t);
      throw t;
    }
    LOG.info("Finished {} task.", getName());
  }

  public TaskList createRemoveUniverseEntryTask() {
    TaskList taskList = new TaskList("RemoveUniverseEntry", executor);
    RemoveUniverseEntry.Params params = new RemoveUniverseEntry.Params();
    // Add the universe uuid.
    params.universeUUID = taskParams().universeUUID;
    // Create the Ansible task to destroy the server.
    RemoveUniverseEntry task = new RemoveUniverseEntry();
    task.initialize(params);
    // Add it to the task list.
    taskList.addTask(task);
    taskListQueue.add(taskList);
    return taskList;
  }
}
