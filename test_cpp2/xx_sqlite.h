#pragma once
#include <xx_mempool.h>
#include "sqlite3.h"

namespace xx
{
	struct SQLite;
	struct SQLiteQuery;
	struct SQLiteReader;

	enum SQLiteDataTypes : uint8_t
	{
		Integer = 1,
		Float = 2,
		Text = 3,
		Blob = 4,
		Null = 5
	};

	struct SQLite : MPObject
	{
		sqlite3* db = nullptr;
		bool readOnly;
		xx::List_v<SQLiteQuery*> queries;
		int lastErrorCode = 0;
		const char* lastErrorMessage = nullptr;

		SQLite(char const* const& fn, bool readOnly = false);
		~SQLite();

		SQLiteQuery* CreateQuery(char const* const& sql, int const& sqlLen = 0);
		int Execute(char const* const& sql);
	};

	struct SQLiteQuery : MPObject
	{
		SQLite* owner;
		uint32_t owner_queries_index = -1;
		sqlite3_stmt* stmt = nullptr;
		int numParams = 0;
		typedef std::function<void(SQLiteReader& sr)> ReadFunc;

		SQLiteQuery(SQLite* owner, char const* const& sql, int const& sqlLen);

		int SetParameter(int parmIdx, int const& v);
		int SetParameter(int parmIdx, sqlite_int64 const& v);
		int SetParameter(int parmIdx, double const& v);
		int SetParameter(int parmIdx, char const* const& str, int strLen = 0, bool makeCopy = false);
		int SetParameter(int parmIdx, char const* const& buf, size_t const& len, bool makeCopy = false);

		template<typename ... Parameters>
		int SetParameters(Parameters ... const& ps);
		template<typename Parameter, typename ... Parameters>
		int SetParametersCore(int& parmIdx, Parameter const& p, Parameters ... const& ps);

		bool Execute(ReadFunc && rf);
	};

	struct SQLiteReader
	{
		sqlite3_stmt* stmt;
		SQLiteReader(sqlite3_stmt* stmt);
		int GetColumnCount();
		SQLiteDataTypes GetColumnDataType(int colIdx);
		char const* GetColumnName(int colIdx);
		bool IsNull(int colIdx);
		int ReadInt32(int colIdx);
		sqlite_int64 ReadInt64(int colIdx);
		double ReadDouble(int colIdx);
		char const* ReadString(int colIdx);
		std::pair<char const*, int> ReadText(int colIdx);
		std::pair<char const*, int> ReadBlob(int colIdx);
	};

	/******************************************************************************************************/
	// impls
	/******************************************************************************************************/

	/***************************************************************/
	// SQLite

	inline SQLite::SQLite(char const* const& fn, bool readOnly)
		: readOnly(readOnly)
		, queries(mempool())
	{
		int r = 0;
		if (readOnly)
		{
			r = sqlite3_open_v2(fn, &db, SQLITE_OPEN_READONLY, nullptr);
		}
		else
		{
			r = sqlite3_open_v2(fn, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
		}
		if (r != SQLITE_OK) throw r;
	}

	inline SQLite::~SQLite()
	{
		for (int i = (int)queries->dataLen - 1; i >= 0; --i)
		{
			mempool().Release(queries->At(i));
		}
		sqlite3_close(db);
		db = nullptr;
	}

	inline SQLiteQuery* SQLite::CreateQuery(char const* const& sql, int const& sqlLen)
	{
		return mempool().Create<SQLiteQuery>(this, sql, sqlLen ? sqlLen : strlen(sql));
	}

	inline int SQLite::Execute(char const* const& sql)
	{
		return sqlite3_exec(db, sql, nullptr, nullptr, (char**)&lastErrorMessage);
	}




	/***************************************************************/
	// SQLiteQuery

	inline SQLiteQuery::SQLiteQuery(SQLite* owner, char const* const& sql, int const& sqlLen)
		: owner(owner)
	{
		auto r = sqlite3_prepare_v2(owner->db, sql, sqlLen, &stmt, nullptr);
		if (r != SQLITE_OK) throw r;

		auto s = sql;
		while (s = strchr(s, '?')) ++numParams;

		owner_queries_index = owner->queries->dataLen;
		owner->queries->Add(this);
	}

	inline SQLiteQuery::~SQLiteQuery()
	{
		sqlite3_finalize(stmt);
		XX_LIST_SWAP_REMOVE(owner->queries, this, owner_queries_index);
	}

	inline int SQLiteQuery::SetParameter(int parmIdx, int const& v)
	{
		return sqlite3_bind_int(stmt, parmIdx, v);
	}
	inline int SQLiteQuery::SetParameter(int parmIdx, sqlite_int64 const& v)
	{
		return sqlite3_bind_int64(stmt, parmIdx, v);
	}
	inline int SQLiteQuery::SetParameter(int parmIdx, double const& v)
	{
		return sqlite3_bind_double(stmt, parmIdx, v);
	}
	inline int SQLiteQuery::SetParameter(int parmIdx, char const* const& str, int strLen = 0, bool makeCopy)
	{
		if (!str) sqlite3_bind_null(stmt, parmIdx);
		return sqlite3_bind_text(stmt, parmIdx, str, strLen ? strLen : (int)strlen(str), makeCopy ? SQLITE_TRANSIENT : SQLITE_STATIC);
	}
	inline int SQLiteQuery::SetParameter(int parmIdx, char const* const& buf, size_t const& len, bool makeCopy)
	{
		if (!buf) sqlite3_bind_null(stmt, parmIdx);
		return sqlite3_bind_blob(stmt, parmIdx, buf, (int)len, makeCopy ? SQLITE_TRANSIENT : SQLITE_STATIC);
	}

	template<typename ... Parameters>
	int SQLiteQuery::SetParameters(Parameters ... const& ps)
	{
		int parmIdx = 0;
		return SetParametersCore(parmIdx, ps...);
	}

	template<typename Parameter, typename ... Parameters>
	int SQLiteQuery::SetParametersCore(int& parmIdx, Parameter const& p, Parameters ... const& ps)
	{
		if (int r = SetParameter(parmIdx, p)) return r;
		return SetParametersCore(++parmIdx, ps...);
	}

	inline bool SQLiteQuery::Execute(ReadFunc && rf)
	{
		int r = sqlite3_step(stmt);
		if (!(r == SQLITE_OK || r == SQLITE_DONE || r == SQLITE_ROW))
		{
			owner->lastErrorCode = r;
			owner->lastErrorMessage = sqlite3_errmsg(owner->db);
			return false;
		}
		if (rf)
		{
			SQLiteReader dr(stmt);
			while (r == SQLITE_ROW)
			{
				rf(dr);
				r = sqlite3_step(stmt);
			}
		}
		r = sqlite3_reset(stmt);
		if (r == SQLITE_OK || r == SQLITE_DONE) return true;
		owner->lastErrorCode = r;
		owner->lastErrorMessage = sqlite3_errmsg(owner->db);
		return false;
	}



	/***************************************************************/
	// SQLiteReader

	inline SQLiteReader::SQLiteReader(sqlite3_stmt* stmt) : stmt(stmt) {}

	inline int SQLiteReader::GetColumnCount()
	{
		return sqlite3_column_count(stmt);
	}

	inline SQLiteDataTypes SQLiteReader::GetColumnDataType(int colIdx)
	{
		assert(colIdx >= 0 && colIdx < GetColumnCount());
		return (SQLiteDataTypes)sqlite3_column_type(stmt, colIdx);
	}

	inline char const* SQLiteReader::GetColumnName(int colIdx)
	{
		assert(colIdx >= 0 && colIdx < GetColumnCount());
		return sqlite3_column_name(stmt, colIdx);
	}

	inline bool SQLiteReader::IsNull(int colIdx)
	{
		assert(colIdx >= 0 && colIdx < GetColumnCount());
		return GetColumnDataType(colIdx) == SQLiteDataTypes::Null;
	}

	inline char const* SQLiteReader::ReadString(int colIdx)
	{
		assert(colIdx >= 0 && colIdx < GetColumnCount());
		if (IsNull(colIdx)) return nullptr;
		return (char const*)sqlite3_column_text(stmt, colIdx);
	}

	inline int SQLiteReader::ReadInt32(int colIdx)
	{
		assert(colIdx >= 0 && colIdx < GetColumnCount() && !IsNull(colIdx));
		return sqlite3_column_int(stmt, colIdx);
	}

	inline sqlite_int64 SQLiteReader::ReadInt64(int colIdx)
	{
		assert(colIdx >= 0 && colIdx < GetColumnCount() && !IsNull(colIdx));
		return sqlite3_column_int64(stmt, colIdx);
	}

	inline double SQLiteReader::ReadDouble(int colIdx)
	{
		assert(colIdx >= 0 && colIdx < GetColumnCount() && !IsNull(colIdx));
		return sqlite3_column_double(stmt, colIdx);
	}

	inline std::pair<char const*, int> SQLiteReader::ReadText(int colIdx)
	{
		auto ptr = (char const*)sqlite3_column_text(stmt, colIdx);
		auto len = sqlite3_column_bytes(stmt, colIdx);
		return std::make_pair(ptr, len);
	}

	inline std::pair<char const*, int> SQLiteReader::ReadBlob(int colIdx)
	{
		auto ptr = (char const*)sqlite3_column_blob(stmt, colIdx);
		auto len = sqlite3_column_bytes(stmt, colIdx);
		return std::make_pair(ptr, len);
	}
}