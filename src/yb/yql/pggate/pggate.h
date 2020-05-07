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

#ifndef YB_YQL_PGGATE_PGGATE_H_
#define YB_YQL_PGGATE_PGGATE_H_

#include <algorithm>
#include <functional>
#include <thread>
#include <unordered_map>

#include <boost/preprocessor/cat.hpp>
#include <boost/preprocessor/seq/for_each.hpp>

#include "yb/util/metrics.h"
#include "yb/util/mem_tracker.h"
#include "yb/common/ybc_util.h"

#include "yb/client/client.h"
#include "yb/client/callbacks.h"
#include "yb/client/async_initializer.h"
#include "yb/server/server_base_options.h"

#include "yb/rpc/rpc_fwd.h"

#include "yb/yql/pggate/pg_env.h"
#include "yb/yql/pggate/pg_session.h"
#include "yb/yql/pggate/pg_statement.h"
#include "yb/yql/pggate/type_mapping.h"

#include "yb/server/hybrid_clock.h"

namespace yb {
namespace pggate {

//--------------------------------------------------------------------------------------------------

class PggateOptions : public yb::server::ServerBaseOptions {
 public:
  static const uint16_t kDefaultPort = 5432;
  static const uint16_t kDefaultWebPort = 13000;

  PggateOptions();
  virtual ~PggateOptions() {}
};

//--------------------------------------------------------------------------------------------------
// Implements support for CAPI.
class PgApiImpl {
 public:
  PgApiImpl(const YBCPgTypeEntity *YBCDataTypeTable, int count, YBCPgCallbacks pg_callbacks);
  virtual ~PgApiImpl();

  //------------------------------------------------------------------------------------------------
  // Access function to Pggate attribute.
  client::YBClient* client() {
    return async_client_init_.client();
  }

  // Initialize ENV within which PGSQL calls will be executed.
  CHECKED_STATUS CreateEnv(PgEnv **pg_env);
  CHECKED_STATUS DestroyEnv(PgEnv *pg_env);

  // Initialize a session to process statements that come from the same client connection.
  // If database_name is empty, a session is created without connecting to any database.
  CHECKED_STATUS InitSession(const PgEnv *pg_env, const string& database_name);

  // Invalidate the sessions table cache.
  CHECKED_STATUS InvalidateCache();

  Result<bool> IsInitDbDone();

  Result<uint64_t> GetSharedCatalogVersion();

  // Setup the table to store sequences data.
  CHECKED_STATUS CreateSequencesDataTable();

  CHECKED_STATUS InsertSequenceTuple(int64_t db_oid,
                                     int64_t seq_oid,
                                     uint64_t ysql_catalog_version,
                                     int64_t last_val,
                                     bool is_called);

  CHECKED_STATUS UpdateSequenceTupleConditionally(int64_t db_oid,
                                                  int64_t seq_oid,
                                                  uint64_t ysql_catalog_version,
                                                  int64_t last_val,
                                                  bool is_called,
                                                  int64_t expected_last_val,
                                                  bool expected_is_called,
                                                  bool *skipped);

  CHECKED_STATUS UpdateSequenceTuple(int64_t db_oid,
                                     int64_t seq_oid,
                                     uint64_t ysql_catalog_version,
                                     int64_t last_val,
                                     bool is_called,
                                     bool* skipped);

  CHECKED_STATUS ReadSequenceTuple(int64_t db_oid,
                                   int64_t seq_oid,
                                   uint64_t ysql_catalog_version,
                                   int64_t *last_val,
                                   bool *is_called);

  CHECKED_STATUS DeleteSequenceTuple(int64_t db_oid, int64_t seq_oid);

  // Delete statement.
  CHECKED_STATUS DeleteStatement(PgStatement *handle);

  // Remove all values and expressions that were bound to the given statement.
  CHECKED_STATUS ClearBinds(PgStatement *handle);

  // Search for type_entity.
  const YBCPgTypeEntity *FindTypeEntity(int type_oid);

  //------------------------------------------------------------------------------------------------
  // Connect database. Switch the connected database to the given "database_name".
  CHECKED_STATUS ConnectDatabase(const char *database_name);

  // Determine whether the given database is colocated.
  CHECKED_STATUS IsDatabaseColocated(const PgOid database_oid, bool *colocated);

  // Create database.
  CHECKED_STATUS NewCreateDatabase(const char *database_name,
                                   PgOid database_oid,
                                   PgOid source_database_oid,
                                   PgOid next_oid,
                                   const bool colocated,
                                   PgStatement **handle);
  CHECKED_STATUS ExecCreateDatabase(PgStatement *handle);

  // Drop database.
  CHECKED_STATUS NewDropDatabase(const char *database_name,
                                 PgOid database_oid,
                                 PgStatement **handle);
  CHECKED_STATUS ExecDropDatabase(PgStatement *handle);

  // Alter database.
  CHECKED_STATUS NewAlterDatabase(const char *database_name,
                                 PgOid database_oid,
                                 PgStatement **handle);
  CHECKED_STATUS AlterDatabaseRenameDatabase(PgStatement *handle, const char *newname);
  CHECKED_STATUS ExecAlterDatabase(PgStatement *handle);

  // Reserve oids.
  CHECKED_STATUS ReserveOids(PgOid database_oid,
                             PgOid next_oid,
                             uint32_t count,
                             PgOid *begin_oid,
                             PgOid *end_oid);

  CHECKED_STATUS GetCatalogMasterVersion(uint64_t *version);

  // Load table.
  Result<PgTableDesc::ScopedRefPtr> LoadTable(const PgObjectId& table_id);

  //------------------------------------------------------------------------------------------------
  // Create, alter and drop table.
  CHECKED_STATUS NewCreateTable(const char *database_name,
                                const char *schema_name,
                                const char *table_name,
                                const PgObjectId& table_id,
                                bool is_shared_table,
                                bool if_not_exist,
                                bool add_primary_key,
                                const bool colocated,
                                PgStatement **handle);

  CHECKED_STATUS CreateTableAddColumn(PgStatement *handle, const char *attr_name, int attr_num,
                                      const YBCPgTypeEntity *attr_type, bool is_hash,
                                      bool is_range, bool is_desc, bool is_nulls_first);

  CHECKED_STATUS CreateTableSetNumTablets(PgStatement *handle, int32_t num_tablets);

  CHECKED_STATUS CreateTableAddSplitRow(PgStatement *handle, int num_cols,
                                        YBCPgTypeEntity **types, uint64_t *data);

  CHECKED_STATUS ExecCreateTable(PgStatement *handle);

  CHECKED_STATUS NewAlterTable(const PgObjectId& table_id,
                               PgStatement **handle);

  CHECKED_STATUS AlterTableAddColumn(PgStatement *handle, const char *name,
                                     int order, const YBCPgTypeEntity *attr_type, bool is_not_null);

  CHECKED_STATUS AlterTableRenameColumn(PgStatement *handle, const char *oldname,
                                        const char *newname);

  CHECKED_STATUS AlterTableDropColumn(PgStatement *handle, const char *name);

  CHECKED_STATUS AlterTableRenameTable(PgStatement *handle, const char *db_name,
                                       const char *newname);

  CHECKED_STATUS ExecAlterTable(PgStatement *handle);

  CHECKED_STATUS NewDropTable(const PgObjectId& table_id,
                              bool if_exist,
                              PgStatement **handle);

  CHECKED_STATUS ExecDropTable(PgStatement *handle);

  CHECKED_STATUS NewTruncateTable(const PgObjectId& table_id,
                                  PgStatement **handle);

  CHECKED_STATUS ExecTruncateTable(PgStatement *handle);

  CHECKED_STATUS GetTableDesc(const PgObjectId& table_id,
                              PgTableDesc **handle);

  CHECKED_STATUS DeleteTableDesc(PgTableDesc *handle);

  CHECKED_STATUS GetColumnInfo(YBCPgTableDesc table_desc,
                               int16_t attr_number,
                               bool *is_primary,
                               bool *is_hash);

  CHECKED_STATUS DmlModifiesRow(PgStatement *handle, bool *modifies_row);

  CHECKED_STATUS SetIsSysCatalogVersionChange(PgStatement *handle);

  CHECKED_STATUS SetCatalogCacheVersion(PgStatement *handle, uint64_t catalog_cache_version);

  //------------------------------------------------------------------------------------------------
  // Create and drop index.
  CHECKED_STATUS NewCreateIndex(const char *database_name,
                                const char *schema_name,
                                const char *index_name,
                                const PgObjectId& index_id,
                                const PgObjectId& table_id,
                                bool is_shared_index,
                                bool is_unique_index,
                                bool if_not_exist,
                                PgStatement **handle);

  CHECKED_STATUS CreateIndexAddColumn(PgStatement *handle, const char *attr_name, int attr_num,
                                      const YBCPgTypeEntity *attr_type, bool is_hash,
                                      bool is_range, bool is_desc, bool is_nulls_first);

  CHECKED_STATUS ExecCreateIndex(PgStatement *handle);

  CHECKED_STATUS NewDropIndex(const PgObjectId& index_id,
                              bool if_exist,
                              PgStatement **handle);

  CHECKED_STATUS ExecDropIndex(PgStatement *handle);

  //------------------------------------------------------------------------------------------------
  // All DML statements
  CHECKED_STATUS DmlAppendTarget(PgStatement *handle, PgExpr *expr);

  // Binding Columns: Bind column with a value (expression) in a statement.
  // + This API is used to identify the rows you want to operate on. If binding columns are not
  //   there, that means you want to operate on all rows (full scan). You can view this as a
  //   a definitions of an initial rowset or an optimization over full-scan.
  //
  // + There are some restrictions on when BindColumn() can be used.
  //   Case 1: INSERT INTO tab(x) VALUES(x_expr)
  //   - BindColumn() can be used for BOTH primary-key and regular columns.
  //   - This bind-column function is used to bind "x" with "x_expr", and "x_expr" that can contain
  //     bind-variables (placeholders) and constants whose values can be updated for each execution
  //     of the same allocated statement.
  //
  //   Case 2: SELECT / UPDATE / DELETE <WHERE key = "key_expr">
  //   - BindColumn() can only be used for primary-key columns.
  //   - This bind-column function is used to bind the primary column "key" with "key_expr" that can
  //     contain bind-variables (placeholders) and constants whose values can be updated for each
  //     execution of the same allocated statement.
  CHECKED_STATUS DmlBindColumn(YBCPgStatement handle, int attr_num, YBCPgExpr attr_value);
  CHECKED_STATUS DmlBindColumnCondEq(YBCPgStatement handle, int attr_num, YBCPgExpr attr_value);
  CHECKED_STATUS DmlBindColumnCondBetween(YBCPgStatement handle, int attr_num, YBCPgExpr attr_value,
      YBCPgExpr attr_value_end);
  CHECKED_STATUS DmlBindColumnCondIn(YBCPgStatement handle, int attr_num, int n_attr_values,
      YBCPgExpr *attr_value);

  // Binding Tables: Bind the whole table in a statement.  Do not use with BindColumn.
  CHECKED_STATUS DmlBindTable(YBCPgStatement handle);

  // API for SET clause.
  CHECKED_STATUS DmlAssignColumn(YBCPgStatement handle, int attr_num, YBCPgExpr attr_value);

  // This function is to fetch the targets in YBCPgDmlAppendTarget() from the rows that were defined
  // by YBCPgDmlBindColumn().
  CHECKED_STATUS DmlFetch(PgStatement *handle, int32_t natts, uint64_t *values, bool *isnulls,
                          PgSysColumns *syscols, bool *has_data);

  // Utility method that checks stmt type and calls exec insert, update, or delete internally.
  CHECKED_STATUS DmlExecWriteOp(PgStatement *handle, int32_t *rows_affected_count);

  // This function adds a primary column to be used in the construction of the tuple id (ybctid).
  CHECKED_STATUS DmlAddYBTupleIdColumn(PgStatement *handle, int attr_num, uint64_t datum,
                                       bool is_null, const YBCPgTypeEntity *type_entity);


  // This function returns the tuple id (ybctid) of a Postgres tuple.
  CHECKED_STATUS DmlBuildYBTupleId(PgStatement *handle, const PgAttrValueDescriptor *attrs,
                                   int32_t nattrs, uint64_t *ybctid);

  // DB Operations: SET, WHERE, ORDER_BY, GROUP_BY, etc.
  // + The following operations are run by DocDB.
  //   - API for "set_clause" (not yet implemented).
  //
  // + The following operations are run by Postgres layer. An API might be added to move these
  //   operations to DocDB.
  //   - API for "where_expr"
  //   - API for "order_by_expr"
  //   - API for "group_by_expr"

  // Buffer write operations.
  void StartOperationsBuffering();
  void ResetOperationsBuffering();
  CHECKED_STATUS FlushBufferedOperations();

  //------------------------------------------------------------------------------------------------
  // Insert.
  CHECKED_STATUS NewInsert(const PgObjectId& table_id,
                           bool is_single_row_txn,
                           PgStatement **handle);

  CHECKED_STATUS ExecInsert(PgStatement *handle);

  CHECKED_STATUS InsertStmtSetUpsertMode(PgStatement *handle);

  //------------------------------------------------------------------------------------------------
  // Update.
  CHECKED_STATUS NewUpdate(const PgObjectId& table_id,
                           bool is_single_row_txn,
                           PgStatement **handle);

  CHECKED_STATUS ExecUpdate(PgStatement *handle);

  //------------------------------------------------------------------------------------------------
  // Delete.
  CHECKED_STATUS NewDelete(const PgObjectId& table_id,
                           bool is_single_row_txn,
                           PgStatement **handle);

  CHECKED_STATUS ExecDelete(PgStatement *handle);

  //------------------------------------------------------------------------------------------------
  // Colocated Truncate.
  CHECKED_STATUS NewTruncateColocated(const PgObjectId& table_id,
                                      bool is_single_row_txn,
                                      PgStatement **handle);

  CHECKED_STATUS ExecTruncateColocated(PgStatement *handle);

  //------------------------------------------------------------------------------------------------
  // Select.
  CHECKED_STATUS NewSelect(const PgObjectId& table_id,
                           const PgObjectId& index_id,
                           const PgPrepareParameters *prepare_params,
                           PgStatement **handle);

  CHECKED_STATUS SetForwardScan(PgStatement *handle, bool is_forward_scan);

  CHECKED_STATUS ExecSelect(PgStatement *handle, const PgExecParameters *exec_params);

  //------------------------------------------------------------------------------------------------
  // Transaction control.
  PgTxnManager* GetPgTxnManager() { return pg_txn_manager_.get(); }

  CHECKED_STATUS BeginTransaction();
  CHECKED_STATUS RestartTransaction();
  CHECKED_STATUS CommitTransaction();
  CHECKED_STATUS AbortTransaction();
  CHECKED_STATUS SetTransactionIsolationLevel(int isolation);
  CHECKED_STATUS SetTransactionReadOnly(bool read_only);
  CHECKED_STATUS SetTransactionDeferrable(bool deferrable);
  CHECKED_STATUS EnterSeparateDdlTxnMode();
  CHECKED_STATUS ExitSeparateDdlTxnMode(bool success);

  //------------------------------------------------------------------------------------------------
  // Expressions.
  //------------------------------------------------------------------------------------------------
  // Column reference.
  CHECKED_STATUS NewColumnRef(PgStatement *handle, int attr_num, const PgTypeEntity *type_entity,
                              const PgTypeAttrs *type_attrs, PgExpr **expr_handle);

  // Constant expressions.
  CHECKED_STATUS NewConstant(YBCPgStatement stmt, const YBCPgTypeEntity *type_entity,
                             uint64_t datum, bool is_null, YBCPgExpr *expr_handle);
  CHECKED_STATUS NewConstantOp(YBCPgStatement stmt, const YBCPgTypeEntity *type_entity,
                             uint64_t datum, bool is_null, YBCPgExpr *expr_handle, bool is_gt);

  // TODO(neil) UpdateConstant should be merged into one.
  // Update constant.
  template<typename value_type>
  CHECKED_STATUS UpdateConstant(PgExpr *expr, value_type value, bool is_null) {
    if (expr->opcode() != PgExpr::Opcode::PG_EXPR_CONSTANT) {
      // Invalid handle.
      return STATUS(InvalidArgument, "Invalid expression handle for constant");
    }
    down_cast<PgConstant*>(expr)->UpdateConstant(value, is_null);
    return Status::OK();
  }
  CHECKED_STATUS UpdateConstant(PgExpr *expr, const char *value, bool is_null);
  CHECKED_STATUS UpdateConstant(PgExpr *expr, const void *value, int64_t bytes, bool is_null);

  // Operators.
  CHECKED_STATUS NewOperator(PgStatement *stmt, const char *opname,
                             const YBCPgTypeEntity *type_entity,
                             PgExpr **op_handle);
  CHECKED_STATUS OperatorAppendArg(PgExpr *op_handle, PgExpr *arg);

  // Foreign key reference caching.
  bool ForeignKeyReferenceExists(YBCPgOid table_id, std::string&& ybctid);
  CHECKED_STATUS CacheForeignKeyReference(YBCPgOid table_id, std::string&& ybctid);
  CHECKED_STATUS DeleteForeignKeyReference(YBCPgOid table_id, std::string&& ybctid);
  void ClearForeignKeyReferenceCache();

  struct MessengerHolder {
    std::unique_ptr<rpc::SecureContext> security_context;
    std::unique_ptr<rpc::Messenger> messenger;
  };

 private:
  // Control variables.
  PggateOptions pggate_options_;

  // Metrics.
  gscoped_ptr<MetricRegistry> metric_registry_;
  scoped_refptr<MetricEntity> metric_entity_;

  // Memory tracker.
  std::shared_ptr<MemTracker> mem_tracker_;

  MessengerHolder messenger_holder_;

  // YBClient is to communicate with either master or tserver.
  yb::client::AsyncClientInitialiser async_client_init_;

  // TODO(neil) Map for environments (we should have just one ENV?). Environments should contain
  // all the custom flags the PostgreSQL sets. We ignore them all for now.
  PgEnv::SharedPtr pg_env_;

  scoped_refptr<server::HybridClock> clock_;

  // Local tablet-server shared memory segment handle.
  std::unique_ptr<tserver::TServerSharedObject> tserver_shared_object_;

  scoped_refptr<PgTxnManager> pg_txn_manager_;

  // Mapping table of YugaByte and PostgreSQL datatypes.
  std::unordered_map<int, const YBCPgTypeEntity *> type_map_;

  scoped_refptr<PgSession> pg_session_;

  YBCPgCallbacks pg_callbacks_;
};

}  // namespace pggate
}  // namespace yb

#endif // YB_YQL_PGGATE_PGGATE_H_
