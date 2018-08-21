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
package com.yugabyte.driver.core;

import org.junit.Test;

import com.datastax.driver.core.TableMetadata;

import org.yb.cql.BaseCQLTest;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.yb.YBTestRunner;

import org.junit.runner.RunWith;

@RunWith(value=YBTestRunner.class)
public class TestPartitionMetadata extends BaseCQLTest {

  private TableSplitMetadata getPartitionMap(TableMetadata table) {
    return cluster.getMetadata().getTableSplitMetadata(table.getKeyspace().getName(),
                                                       table.getName());
  }

  @Test
  public void testCreateDropTable() throws Exception {

    final int MAX_WAIT_SECONDS = 10;

    // Create test table. Verify that the PartitionMetadata gets notified of the table creation
    // and loads the metadata.
    session.execute("create table test_partition1 (k int primary key);");
    TableMetadata table = cluster
                          .getMetadata()
                          .getKeyspace(BaseCQLTest.DEFAULT_TEST_KEYSPACE)
                          .getTable("test_partition1");
    boolean found = false;
    for (int i = 0; i < MAX_WAIT_SECONDS; i++) {
      TableSplitMetadata partitionMap = getPartitionMap(table);
      if (partitionMap != null && partitionMap.getHosts(0).size() > 0) {
        found = true;
        break;
      }
      Thread.sleep(1000);
    }
    assertTrue(found);

    // Drop test table. Verify that the PartitionMetadata gets notified of the table drop
    // and clears the the metadata.
    session.execute("Drop table test_partition1;");
    for (int i = 0; i < MAX_WAIT_SECONDS; i++) {
      if (getPartitionMap(table) == null) {
        found = false;
        break;
      }
      Thread.sleep(1000);
    }
    assertFalse(found);
  }
}
