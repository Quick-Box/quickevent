# QxSql API

QxSql is a simple SQL access API exposed over SHV RPC. It provides raw query/exec plus record CRUD on tables that have an integer `id` primary key.

Implementations:

* Rust – trait `QxSqlApi` in the [qxsql](https://github.com/fvacek/qxsql/blob/master/src/sql/mod.rs) crate. This is the reference implementation.
* C++ – class `qf::core::sql::QxSqlApi` (`libqf/libqfcore/src/sql/qxsql.h`), used by QuickEvent.
  * `QxSqlApiImpl` – backed by a `QSqlDatabase`.
  * `QxSql` – a `QObject` wrapper around `QxSqlApiImpl` that also emits `recChng` after every successful create/update/delete.

## Types

### DbValue

A single DB cell value:

| variant    | Rust                            | RPC type |
|------------|---------------------------------|----------|
| `String`   | `String`                        | `s`      |
| `Blob`     | `Vec<u8>`                       | planned  |
| `Int`      | `i64`                           | `i`      |
| `Double`   | `f64`                           | planned  |
| `Bool`     | `bool`                          | `b`      |
| `DateTime` | `chrono::DateTime<FixedOffset>` | `t`      |
| `Null`     | –                               | `n`      |

Currently only `s|i|b|t|n` values are accepted from RPC. Support for Double and Blob is planned but not implemented yet. Decimal, List, Map and IMap values are not supported. In C++ the value is a `QVariant`, and SQL NULL is an invalid `QVariant`.

### Record

A map from field name to value.

```rust
pub type Record = HashMap<String, DbValue>;
```

```cpp
using Record = QVariantMap;
```

### QueryResult

```rust
pub struct DbField {
    pub name: String,
}

pub struct QueryResult {
    pub fields: Vec<DbField>,
    pub rows: Vec<Vec<DbValue>>,
}
```

Helper methods:

* `record(row)` – row as `Record`, `None` if out of range.
* `value(row, col)` – cell by column index (Rust only).
* `value_by_name(row, col_name)` – cell by column name (Rust only).
* `row_count()`, `column_count()` (Rust only).

The C++ `QxSqlApiImpl::query()` converts field names to **lower case**.

### ExecResult

```rust
pub struct ExecResult {
    pub rows_affected: i64,
    pub insert_id: Option<i64>,
}
```

In C++ the fields are `numRowsAffected` and `lastInsertId`.

## RPC methods

The C++ server exposes the API on the SHV node `sql`
(`quickevent/app/quickevent/plugins/Event/src/services/qx/sqlapinode.cpp`). Signatures are defined by constants in the Rust crate (`QUERY_PARAMS`, `QUERY_RESULT`, …).

| method        | params | result |
|---------------|--------|--------|
| `query`       | `[s:query,{s\|i\|b\|t\|n}:params]` | `{{s:name}:fields,[[s\|i\|b\|t\|n]]:rows}` |
| `exec`        | `[s:query,{s\|i\|b\|t\|n}:params]` | `{i:rows_affected,i\|n:insert_id}` |
| `transaction` | `[s:query,[[s\|i\|b\|t\|n]]:params]` | `n` |
| `list`        | `{s:table,[s]\|n:fields,i\|n:ids_above,i\|n:limit}` | `[{s\|i\|b\|t\|n}]` |
| `create`      | `{s:table,{s\|i\|b\|t\|n}:record}` | `i` |
| `read`        | `{s:table,i:id,{s}\|n:fields}` | `{s\|i\|b\|t\|n}\|n` |
| `update`      | `{s:table,i:id,{s\|i\|b\|t\|n}:record}` | `b` |
| `delete`      | `{s:table,i:id}` | `b` |

### Parameter structs (Rust)

```rust
// query, exec: ["SELECT ..."], ["SELECT ... :id", {"id": 42}], ["...", {...}, "issuer"]
pub struct QueryAndParams(String, Option<Record>, Option<String> /* issuer */);

// transaction
pub struct QueryAndParamsList(String, Vec<Record>);

pub struct RecListParam   { table: String, fields: Option<Vec<String>>, ids_above: Option<i64>, limit: Option<i64> }
pub struct RecReadParam   { table: String, id: i64, fields: Option<Vec<String>> }
pub struct RecInsertParam { table: String, record: Record, issuer: Option<String> }
pub struct RecUpdateParam { table: String, id: i64, record: Record, issuer: Option<String> }
pub struct RecDeleteParam { table: String, id: i64, issuer: Option<String> }
```

`params`, `fields`, `ids_above`, `limit` and `issuer` are optional. `issuer` identifies who made the change and is copied into the `recchng` notification.

## Raw SQL

Named parameters are written in the query as `:name` and passed in `params` without the leading colon:

```
["SELECT * FROM runs WHERE competitorId = :cid", {"cid": 123}]
```

### query

```rust
async fn query(&self, query: &str, params: Option<&Record>) -> anyhow::Result<QueryResult>;
```

```cpp
virtual QueryResult query(const QString &query, const QVariantMap &params) = 0;
```

Executes a statement that returns rows (`SELECT`, `INSERT … RETURNING`, …) and returns column names and all rows.

### exec

```rust
async fn exec(&self, query: &str, params: Option<&Record>) -> anyhow::Result<ExecResult>;
```

```cpp
virtual ExecResult exec(const QString &query, const QVariantMap &params) = 0;
```

Executes a statement that does not return rows (`INSERT`, `UPDATE`, `DELETE`, DDL, …). Returns the number of affected rows and the last insert id, if the driver provides one.

### transaction

```cpp
void transaction(const QString &query, const QVariantList &params, QSqlDatabase db);
```

Prepares `query` once and executes it for every item of `params` inside a single DB transaction. Every item of `params` is a record of named parameters. If any execution fails, the transaction is rolled back and an error is returned.

The Rust trait has no `transaction` method; only the RPC signature is defined.

## Record CRUD

All record methods assume the table has an integer primary key column `id`. They are provided methods of the trait / base class, built on `query()` and `exec()`.

### list

```rust
async fn list_records(&self, table: &str, fields: Option<Vec<&str>>,
                      from_id: Option<i64>, limit: Option<i64>) -> anyhow::Result<Vec<Record>>;
```

```cpp
QList<Record> listRecords(const QString &table,
		const std::optional<QStringList> &fields = std::nullopt,
		const std::optional<qint64> &fromId = std::nullopt,
		const std::optional<qint64> &limit = std::nullopt);
```

Returns records ordered by `id`.

| param                   | meaning |
|-------------------------|---------|
| `table`                 | table name |
| `fields`                | columns to return; all columns (`*`) if missing |
| `from_id` / `ids_above` | return only records with `id >= from_id` |
| `limit`                 | maximum number of returned records |

```sql
SELECT <fields> FROM <table> [WHERE id >= <from_id>] ORDER BY id [LIMIT <limit>]
```

When `from_id` is set and `limit == 1`, the condition is `id = from_id`. This is how `read` is implemented.

### create

```rust
async fn create_record(&self, table: &str, record: &Record) -> anyhow::Result<i64>;
```

```cpp
qint64 createRecord(const QString &table, const Record &record, const QString &issuer);
```

Inserts `record` into `table` and returns the new `id`:

```sql
INSERT INTO <table> (<keys>) VALUES (<:keys>) RETURNING id
```

Fails if the DB does not return an integer id.

### read

```rust
async fn read_record(&self, table: &str, id: i64, fields: Option<Vec<&str>>) -> anyhow::Result<Option<Record>>;
```

```cpp
std::optional<Record> readRecord(const QString &table, qint64 id,
		const std::optional<QStringList> &fields = std::nullopt);
```

Returns the record with the given `id`, or `None`/`null` if it does not exist. `fields` has the same meaning as in `list`.

### update

```rust
async fn update_record(&self, table: &str, id: i64, record: &Record) -> anyhow::Result<bool>;
```

```cpp
bool updateRecord(const QString &table, qint64 id, const Record &record, const QString &issuer);
```

Updates only the fields present in `record`:

```sql
UPDATE <table> SET key1 = :key1, ... WHERE id = <id>
```

Returns `true` if exactly one row was updated.

### delete

```rust
async fn delete_record(&self, table: &str, id: i64) -> anyhow::Result<bool>;
```

```cpp
bool deleteRecord(const QString &table, qint64 id, const QString &issuer);
```

```sql
DELETE FROM <table> WHERE id = <id>
```

Returns `true` if exactly one row was deleted.

## Errors

The Rust implementation returns `anyhow::Error`. The C++ implementation throws `qf::core::Exception`, or `std::runtime_error` for transaction BEGIN/COMMIT failures. Over RPC, errors are returned as an RPC error response.

## Change notifications

Every successful `create`, `update` and `delete` is broadcast as the `recchng` signal on the `sql` node:

```rust
pub enum RecOp { Insert, Update, Delete }

pub struct RecChng {
    pub table: String,
    pub id: i64,
    pub record: Option<Record>,  // inserted/updated fields, none for Delete
    pub op: RecOp,
    pub issuer: Option<String>,
}
```

In C++ this is `qf::core::sql::QxRecChng`, emitted by `QxSql` as:

```cpp
void recChng(const qf::core::sql::QxRecChng &recchng, QObject *source);
```

`source` is the object that requested the change, so it can ignore its own notifications.

Changes made by raw `query`/`exec`/`transaction` do **not** emit `recchng`. In C++, use `QxSql::emitRecInserted()`, `emitRecUpdated()`, `emitRecDeleted()` or `emitRecChng()` to notify manually.

## Differences between implementations

* `exec` RPC result: the C++ server returns `{numRowsAffected, lastInsertId}` instead of the specified `{rows_affected, insert_id}`.
* `transaction` signature: the signature string declares params as a list of lists (`[[...]]`), but both implementations expect a list of records (maps).
* `issuer`: Rust accepts it per request. C++ ignores the `issuer` method argument and uses the issuer passed to the `QxSql` constructor.
* Empty `fields`: C++ treats an empty list as `*`. Rust only uses `*` for `None`, so `Some(vec![])` produces invalid SQL.
* `read` `fields` is declared as `{s}|n` in both, but it is actually parsed as a list of strings (`[s]|n`).
