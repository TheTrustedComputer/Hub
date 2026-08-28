/*
 *  Author: 2026- TheTrustedComputer
 *
 *  Provides a simple interface for storing solved positions to the database.
 *  Instead of repeating, we query it whether a position has already been solved.
 *
 *  On database hit: fetch the saved entry directly.
 *  On database miss: search for a solution and save it.
 *
 *  Currently, we rely on SQLite to store the solutions.
 *  After we design our own, SQLite will become optional.
 */

#ifndef DATABASE_H
#define DATABASE_H

#ifdef FTW_SQLITE
#define SQLite_check(_func, _db, _string) \
if (_func != SQLITE_OK) \
{ \
    fprintf(stderr, _string, FTW_STR_ERROR_PREFIX, sqlite3_errmsg(_db)); \
    \
    return false; \
}

static sqlite3 *database = nullptr;
static uint8_t *restrict keyBlob, *restrict resBlob;

/////////////////////////////////////////////////////////////////
/// @brief      Sets some SQLite configurations for performance.
/// @param  _db Unaliased pointer to the database handle.
/// @return     `true` if all checks passed; otherwise, `false`.
/////////////////////////////////////////////////////////////////
static inline bool SQLite_pragmas(sqlite3 *const restrict _db)
{
    SQLite_check(sqlite3_exec(_db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr), _db, "\e[1m%s: Could not set the journaling mode to WAL -- %s.\e[0m\n");
    SQLite_check(sqlite3_exec(_db, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr), _db, "\e[1m%s: Could not set the synchronous flag to NORMAL -- %s.\e[0m\n");
    SQLite_check(sqlite3_exec(_db, "PRAGMA temp_store=MEMORY;", nullptr, nullptr, nullptr), _db, "\e[1m%s: Could not set the temporary storage medium to MEMORY -- %s.\e[0m\n");

    return true;
}

///////////////////////////////////////////////////////////////////
/// @brief          Opens a SQLite database for read/write.
/// @param  _db     A pointer to the database handle.
/// @param  _NAME   Name of the file relative to the binary path.
/// @return         Value of `SQLite_pragmas()`.
/// @details        Adds a blank table if the file does not exist.
///////////////////////////////////////////////////////////////////
static inline bool SQLite_open(sqlite3 **_db, const char *const restrict _NAME)
{
    SQLite_check(sqlite3_open(_NAME, _db), *_db, "\e[1m%s: Could not open the solution database -- %s.\e[0m\n");

    // s = solution; k = key; t = turn; m = method; r = result
    static constexpr char C4_TABLE_QUERY[] = "CREATE TABLE IF NOT EXISTS s (k BLOB PRIMARY KEY, r BLOB) WITHOUT ROWID;";
    static constexpr char PT_TABLE_QUERY[] = "CREATE TABLE IF NOT EXISTS s (k0 BLOB, k1 INTEGER, r BLOB, PRIMARY KEY(k0, k1)) WITHOUT ROWID;";
    static constexpr char M7_TABLE_QUERY[] = "CREATE TABLE IF NOT EXISTS s (k0 INTEGER, k1 INTEGER, k2 INTEGER, m INTEGER, r BLOB, PRIMARY KEY(k0, k1, k2, m)) WITHOUT ROWID;";

    const bool POP10 = C4_variant == CONNECT4_POP10;
    const bool MAKE7 = C4_variant == CONNECT4_MAKE7;

    SQLite_check(sqlite3_exec(*_db, MAKE7 ? M7_TABLE_QUERY : POP10 ? PT_TABLE_QUERY : C4_TABLE_QUERY, nullptr, nullptr, nullptr), *_db, "\e[1m%s: Could not create the solution database -- %s.\e[0m\n");

    keyBlob = !MAKE7 ? REC_calloc(sizeof(Board), 1, "Could not allocate memory for the key blob.", true) : nullptr;
    resBlob = REC_calloc(!MAKE7 + 1, 1, "Could not allocate memory for the result blob.", true);

    return SQLite_pragmas(*_db);
}

///////////////////////////////////////////////////////////
/// @brief      Closes a SQLite database and optimizes it.
/// @param  _db Unaliased pointer to the database handle.
///////////////////////////////////////////////////////////
static inline bool SQLite_close(sqlite3 *const restrict _db)
{
    SQLite_check(sqlite3_exec(_db, "PRAGMA optimize; VACUUM;", nullptr, nullptr, nullptr), _db, "\e[1m%s: Could not optimize the solution database -- %s.\e[0m\n");
    SQLite_check(sqlite3_close(_db), _db, "\e[1m%s: Could not close the solution database -- %s.\e[0m\n");

    REC_free(keyBlob);
    REC_free(resBlob);

    return true;
}

///////////////////////////////////////////////////////////
/// @brief      Deletes all rows in the solution database.
/// @param  _db Unaliased pointer to the database handle.
///////////////////////////////////////////////////////////
static inline bool SQLite_delete(sqlite3 *const restrict _db)
{
    SQLite_check(sqlite3_exec(_db, "DELETE FROM s;", nullptr, nullptr, nullptr), _db, "\e[1m%s: Could not delete the solution database table -- %s.\e[0m\n");

    return true;
}

//////////////////////////////////////////////////////////
/// @brief      Launches the beginning of a transaction.
/// @param  _db Unaliased pointer to the database handle.
//////////////////////////////////////////////////////////
static inline bool SQLite_beginTransaction(sqlite3 *const restrict _db)
{
    SQLite_check(sqlite3_exec(_db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr), _db, "\e[1m%s: Could not begin a database transaction -- %s.\e[0m\n");

    return true;
}

///////////////////////////////////////////////////////////
/// @brief       Commits all changes in a transaction.
/// @param  _db  Unaliased pointer to the database handle.
///////////////////////////////////////////////////////////
static inline bool SQLite_commitTransaction(sqlite3 *const restrict _db)
{
    SQLite_check(sqlite3_exec(_db, "COMMIT TRANSACTION;", nullptr, nullptr, nullptr), _db, "\e[1m%s: Could not commit a database transaction -- %s.\e[0m\n");

    return true;
}

///////////////////////////////////////////////////////////
/// @brief      Queries the DB for a solution (Connect 4).
/// @param _db  Unaliased pointer to the DB handle.
/// @param _KEY The Connect 4 key to look for.
/// @param _res Where to write the result if found.
/// @return     `true` on hit; `false` on miss or error.
///////////////////////////////////////////////////////////
static inline bool SQLite_Connect4_query(sqlite3 *const restrict _db, const Board _KEY, Result *const restrict _res)
{
    sqlite3_stmt *stmt;

    SQLite_check(sqlite3_prepare_v2(_db, "SELECT r FROM s WHERE k = ?;", -1, &stmt, nullptr), _db, "\e[1m%s: Could not prepare a selection statement for Connect 4 -- %s.\e[0m\n");

    uint8_t keyBytes = 0;

    memset(keyBlob, 0, sizeof(Board));

    for (Board key = Connect4_canonicalize(_KEY); key; key >>= 8)
    {
        keyBlob[keyBytes++] = key;
    }

    sqlite3_bind_blob(stmt, 1, keyBlob, keyBytes, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        _res ? *_res = Result_fromBLOB(sqlite3_column_blob(stmt, 0)) : FTW_VOID_NOP;
        sqlite3_finalize(stmt);

        return true;
    }

    sqlite3_finalize(stmt);

    return false;
}

//////////////////////////////////////////////////////////////////
/// @brief  Queries the DB for the Pop 10 variation of Connect 4.
/// @param  _db
/// @param  _KEY_A
/// @param  _KEY_B
/// @param  _res
/// @return See `SQLite_Connect4_query()` for return value.
//////////////////////////////////////////////////////////////////
static inline bool SQLite_Connect4_pop10_query(sqlite3 *const restrict _db, const Board _KEY_A, const Board _KEY_B, Result *const restrict _res)
{
    sqlite3_stmt *stmt;

    SQLite_check(sqlite3_prepare_v2(_db, "SELECT r FROM s WHERE k0 = ? AND k1 = ?;", -1, &stmt, nullptr), _db, "\e[1m%s: Could not prepare a selection statement for Connect 4 Pop 10 -- %s.\e[0m\n");

    uint8_t keyBytes = 0;

    memset(keyBlob, 0, sizeof(Board));

    for (Board key = Connect4_canonicalize(_KEY_A); key; key >>= 8)
    {
        keyBlob[keyBytes++] = key;
    }

    sqlite3_bind_blob(stmt, 1, keyBlob, keyBytes, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, _KEY_B);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        _res ? *_res = Result_fromBLOB(sqlite3_column_blob(stmt, 0)) : FTW_VOID_NOP;
        sqlite3_finalize(stmt);

        return true;
    }

    sqlite3_finalize(stmt);

    return false;
}

///////////////////////////////////////////////////////////////
/// @brief  Searches the DB for a precomputed Make 7 solution.
/// @param  _db
/// @param  _k0
/// @param  _k1
/// @param  _k2
/// @param  _WM
/// @param  _res
///////////////////////////////////////////////////////////////
static inline bool SQLite_Make7_query(sqlite3 *const restrict _db, uint64_t _k0, uint64_t _k1, uint64_t _k2, const bool _WM, Result *const restrict _res)
{
    sqlite3_stmt *stmt;

    SQLite_check(sqlite3_prepare_v2(_db, "SELECT r FROM s WHERE k0 = ? AND k1 = ? AND k2 = ? AND m = ?;", -1, &stmt, nullptr), _db, "\e[1m%s: Could not prepare a selection statement for Make 7 -- %s.\e[0m\n");

    Make7_canonicalize(_k0, _k1, _k2, &_k0, &_k1, &_k2);

    sqlite3_bind_int64(stmt, 1, _k0);
    sqlite3_bind_int64(stmt, 2, _k1);
    sqlite3_bind_int64(stmt, 3, _k2);
    sqlite3_bind_int(stmt, 4, _WM);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        _res ? *_res = Result_fromBLOB(sqlite3_column_blob(stmt, 0)) : FTW_VOID_NOP;
        sqlite3_finalize(stmt);

        return true;
    }

    sqlite3_finalize(stmt);

    return false;
}

//////////////////////////////////////////////////////////
/// @brief      Inserts a result into the DB (Connect 4).
/// @param _db  Unaliased pointer to the DB handle.
/// @param _KEY The Connect 4 key to look for.
/// @param _RES The result to insert or replace.
/// @return     `true` if successful; otherwise `false`.
//////////////////////////////////////////////////////////
static inline bool SQLite_Connect4_insert(sqlite3 *const restrict _db, const Board _KEY, const Result *const restrict _RES)
{
    sqlite3_stmt *stmt;

    SQLite_check(sqlite3_prepare_v2(_db, "REPLACE INTO s (k, r) VALUES (?, ?);", -1, &stmt, nullptr), _db, "\e[1m%s: Could not prepare an insertion statement for Connect 4 -- %s.\e[0m\n");

    uint8_t keyBytes = 0, resBytes = 0;

    memset(keyBlob, 0, sizeof(Board));
    memset(resBlob, 0, sizeof(*resBlob) * 2);

    for (Board key = Connect4_canonicalize(_KEY); key; key >>= 8)
    {
        keyBlob[keyBytes++] = key;
    }

    for (uint16_t blob = Result_toScalar(_RES); blob; blob >>= 8)
    {
        resBlob[resBytes++] = blob;
    }

    sqlite3_bind_blob(stmt, 1, keyBlob, keyBytes, SQLITE_STATIC);
    sqlite3_bind_blob(stmt, 2, resBlob, resBytes, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE)
    {
        fprintf(stderr, "\e[1m%s: Could not insert into the Connect 4 solution database -- %s.\e[0m\n", FTW_STR_ERROR_PREFIX, sqlite3_errmsg(_db));
        sqlite3_finalize(stmt);

        return false;
    }

    sqlite3_finalize(stmt);

    return true;
}

///////////////////////////////////////////////////
/// @brief  Database insertion (Connect 4 Pop 10).
/// @param  _db
/// @param  _KEY_A
/// @param  _KEY_B
/// @param  _RES
///////////////////////////////////////////////////
static inline bool SQLite_Connect4_pop10_insert(sqlite3 *const restrict _db, const Board _KEY_A, const Board _KEY_B, const Result *const restrict _RES)
{
    sqlite3_stmt *stmt;

    SQLite_check(sqlite3_prepare_v2(_db, "REPLACE INTO s (k0, k1, r) VALUES (?, ?, ?);", -1, &stmt, nullptr), _db, "\e[1m%s: Could not prepare an insertion statement for Connect 4 Pop 10 -- %s.\e[0m\n");

    uint8_t keyBytes = 0, resBytes = 0;

    memset(keyBlob, 0, sizeof(Board));
    memset(resBlob, 0, sizeof(*resBlob) * 2);

    for (Board key = Connect4_canonicalize(_KEY_A); key; key >>= 8)
    {
        keyBlob[keyBytes++] = key;
    }

    for (uint16_t blob = Result_toScalar(_RES); blob; blob >>= 8)
    {
        resBlob[resBytes++] = blob;
    }

    sqlite3_bind_blob(stmt, 1, keyBlob, keyBytes, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, _KEY_B);
    sqlite3_bind_blob(stmt, 3, resBlob, resBytes, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE)
    {
        fprintf(stderr, "\e[1m%s: Could not insert into the Connect 4 Pop 10 solution database -- %s.\e[0m\n", FTW_STR_ERROR_PREFIX, sqlite3_errmsg(_db));
        sqlite3_finalize(stmt);

        return false;
    }

    sqlite3_finalize(stmt);

    return true;
}

//////////////////////////////////////////////
/// @brief  Writes a Make 7 result to the DB.
/// @param  _db
/// @param  _k0
/// @param  _k1
/// @param  _k2
/// @param  _WM
/// @param  _res
//////////////////////////////////////////////
static inline bool SQLite_Make7_insert(sqlite3 *const restrict _db, uint64_t _k0, uint64_t _k1, uint64_t _k2, const bool _WM, const Result *const restrict _RES)
{
    sqlite3_stmt *stmt;

    SQLite_check(sqlite3_prepare_v2(_db, "REPLACE INTO s (k0, k1, k2, m, r) VALUES (?, ?, ?, ?, ?);", -1, &stmt, nullptr), _db, "\e[1m%s: Could not prepare an insertion statement for Make 7 -- %s.\e[0m\n");

    resBlob[0] = Result_toScalar(_RES);

    Make7_canonicalize(_k0, _k1, _k2, &_k0, &_k1, &_k2);

    sqlite3_bind_int64(stmt, 1, _k0);
    sqlite3_bind_int64(stmt, 2, _k1);
    sqlite3_bind_int64(stmt, 3, _k2);
    sqlite3_bind_int(stmt, 4, _WM);
    sqlite3_bind_blob(stmt, 5, resBlob, 1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE)
    {
        fprintf(stderr, "\e[1m%s: Could not insert into the Make 7 solution database -- %s.\e[0m\n", FTW_STR_ERROR_PREFIX, sqlite3_errmsg(_db));
        sqlite3_finalize(stmt);

        return false;
    }

    sqlite3_finalize(stmt);

    return true;
}

#endif

/*
typedef struct
{
    char sign[4];       // "FTW!"
    uint8_t cols;       // Board columns
    uint8_t rows;       // Board rows
    uint8_t ruleset;    // Game ruleset
    uint8_t keySize;    // Bytes per key
    uint8_t recSize;    // Bytes per record
    uint64_t records;   // Number of records
}
DBHeader;

typedef struct
{
    uint8_t *restrict key;
    uint8_t *restrict res;
}
DBRecord;
*/

#endif // DATABASE_H //
