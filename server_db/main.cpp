#include "xx_uv.h"
#include "xx_helpers.h"
#include "pkg\PKG_class.h"
#include <xx_sqlite.h>
#include <mutex>
#include <thread>

struct Peer;
struct Service;
struct Listener;
struct TaskManager;
struct Dispacher;

/******************************************************************************/

struct Peer : xx::UVServerPeer
{
	Service* service;

	Peer(Listener* listener, Service* service);
	virtual void OnReceivePackage(xx::BBuffer & bb) override;
	virtual void OnDisconnect() override;
};

struct Listener : xx::UVListener
{
	Service* service;

	Listener(xx::UV* uv, int port, int backlog, Service* service);
	virtual xx::UVServerPeer * OnCreatePeer() override;
};

struct TaskManager : xx::MPObject
{
	Service* service;
	Dispacher* dispacher = nullptr;
	std::mutex tasksMutex;
	std::mutex resultsMutex;
	xx::Queue_v<std::function<void()>> tasks;
	xx::Queue_v<std::function<void()>> results;
	void AddTask(std::function<void()>&& f);
	void AddTask(std::function<void()> const& f);
	void AddResult(std::function<void()>&& f);
	void AddResult(std::function<void()> const& f);

	TaskManager(Service* service);
	~TaskManager();
	volatile bool running = true;
	volatile bool stoped = false;
	void ThreadProcess();
};
using TaskManager_v = xx::Dock<TaskManager>;

struct Service : xx::MPObject
{
	xx::UV_v uv;
	xx::SQLite_v sqldb;
	Listener* listener = nullptr;
	TaskManager_v tm;

	Service();
	int Run();
};

struct Dispacher : xx::UVAsync
{
	TaskManager* tm;

	Dispacher(xx::UV* uv, TaskManager* tm);
	virtual void OnFire() override;
};


/******************************************************************************/

Listener::Listener(xx::UV* uv, int port, int backlog, Service* service)
	: xx::UVListener(uv, port, backlog)
	, service(service)
{
}

xx::UVServerPeer* Listener::OnCreatePeer()
{
	return mempool().Create<Peer>(this, service);
}


/******************************************************************************/


Dispacher::Dispacher(xx::UV* uv, TaskManager* tm)
	: xx::UVAsync(uv)
	, tm(tm)
{
}

void Dispacher::OnFire()
{
	while (!tm->results->Empty())
	{
		tm->results->Top()();
		tm->results->Pop();
	}
}

/******************************************************************************/

TaskManager::TaskManager(Service* service)
	: service(service)
	, tasks(mempool())
	, results(mempool())
{
	this->dispacher = service->uv->CreateAsync<Dispacher>(this);
	if (!this->dispacher) throw nullptr;
	std::thread t([this] { ThreadProcess(); });
	t.detach();
}

TaskManager::~TaskManager()
{
	running = false;
	while (!stoped) Sleep(1);
}

void TaskManager::AddTask(std::function<void()>&& f)
{
	std::lock_guard<std::mutex> lock(tasksMutex);
	tasks->Push(std::move(f));
}

void TaskManager::AddTask(std::function<void()> const& f)
{
	std::lock_guard<std::mutex> lock(tasksMutex);
	tasks->Push(std::move(f));
}

void TaskManager::AddResult(std::function<void()>&& f)
{
	{
		std::lock_guard<std::mutex> lock(resultsMutex);
		results->Push(std::move(f));
	}
	dispacher->Fire();
}

void TaskManager::AddResult(std::function<void()> const& f)
{
	{
		std::lock_guard<std::mutex> lock(resultsMutex);
		results->Push(std::move(f));
	}
	dispacher->Fire();
}

void TaskManager::ThreadProcess()
{
	std::function<void()> func;
	while (running)
	{
		bool gotFunc = false;
		{
			std::lock_guard<std::mutex> lock(tasksMutex);
			gotFunc = tasks->TryPop(func);
		}
		if (gotFunc) func();
		else Sleep(1);
	}
	stoped = true;
}

/******************************************************************************/

Service::Service()
	: uv(mempool())
	, sqldb(mempool(), "data.db")
	, tm(mempool(), this)
{
	sqldb->SetPragmas(xx::SQLiteJournalModes::WAL);
}

int Service::Run()
{
	// todo: 检查 db 看是不是新建的, 是就执行建表脚本
	// todo: 先去写一套生成器 for sqlite

	listener = uv->CreateListener<Listener>(12345, 128, this);
	if (!listener) return -1;
	uv->Run();
	return 0;
}

/******************************************************************************/

Peer::Peer(Listener* listener, Service* service)
	: xx::UVServerPeer(listener)
	, service(service)
{
}

void Peer::OnDisconnect() {}

void Peer::OnReceivePackage(xx::BBuffer& bb)
{
	// todo: 收到包, 解析, 向任务容器压函数, 转到后台线程执行
	// SQLite 走独立的内存池, 和主线程的分离

	// 内存分配 & 回收流程( 基于 SQLite 只占 1 线, 使用 mempool 的情况 ): 
	// 1. uv线程 分配 SQL线程 需要的数据, 后将处理函数压入 tasks
	// 2. SQL线程 执行期间, 分配供 uv线程 处理结果所需数据, 后将处理函数压入 results
	// 3. uv线程 读取结果数据, 回收 第1步分配的内存, 后将 第2步的内存回收函数压入 tasks
	// 4. tasks 执行第2步分配的内存回收

	// 上述流程的 多 work thread 的补充:
	// 1. 做一个线程私有 tasks 队列及 mempool, 标记为 TLS
	// 2. 上述流程第 2 步 记录私有 tasks 指针 并向下传递
	// 3. 上述流程第 3 步 向 私有 tasks 压内存回收函数
	// 4. 工作线程除了扫公共 tasks 以外, 还扫私有 tasks, 扫到就执行

	// 这套方案的问题在于, 多线内存池会损耗比较大, 复用率低下. 



	service->tm->AddTask([service = this->service, peer = xx::MPtr<Peer>(this)/*, args*/]	// 转到 SQL 线程
	{
		// 执行 SQL 语句, 得到结果
		// auto rtv = sqlfs.execxxxxx( args... )

		service->tm->AddResult([service, peer/*, args, rtv */]	// 转到 uv 线程
		{
		// args->Release();
		// handle( rtv )
		if (peer && peer->state == xx::UVPeerStates::Connected)	// 如果 peer 还活着, 做一些回发功能
		{
			//mp->SendPackages
		}
		// service->tm->AddTask([rtv]{ rtv->Release(); })	// 转到 SQL 线程
	});
	});
}

/******************************************************************************/










#include "db\DB_class.h"
#include "db\DB_sqlite.h"

//namespace DB
//{
//	// 模拟生成的函数长相
//
//	struct SQLiteFuncs
//	{
//		xx::SQLite* sqlite;
//		xx::MemPool& mp;
//		xx::SQLiteString_v s;
//		bool hasError = false;
//		int const& lastErrorCode() { return sqlite->lastErrorCode; }
//		const char* const& lastErrorMessage() { return sqlite->lastErrorMessage; }
//
//		SQLiteFuncs(xx::SQLite* sqlite) : sqlite(sqlite), mp(sqlite->mempool()), s(mp) {}
//
//		xx::SQLiteQuery_p query_CreateAccountTable;
//		void CreateAccountTable()
//		{
//			hasError = true;
//			auto& q = query_CreateAccountTable;
//			if (!q)
//			{
//				q = sqlite->CreateQuery(R"=-=(
//create table [account]
//(
//    [id] integer primary key autoincrement, 
//    [username] text(64) not null unique, 
//    [password] text(64) not null
//)
//)=-=");
//			}
//			if (!q) return;
//			if (!q->Execute()) return;
//			hasError = false;
//		}
//
//		xx::SQLiteQuery_p query_AddAccount;
//		void AddAccount(DB::Account const* const& a)
//		{
//			hasError = true;
//			auto& q = query_AddAccount;
//			if (!q)
//			{
//				q = sqlite->CreateQuery(R"=-=(
//insert into [account] ([username], [password])
//values (?, ?)
//)=-=");
//			}
//			if (!q) return;
//			if (q->SetParameters(a->username, a->password)) return;
//			if (!q->Execute()) return;
//			hasError = false;
//		}
//
//		void AddAccount2(char const* const& username, char const* const& password)
//		{
//			hasError = true;
//			auto& q = query_AddAccount;
//			if (!q)
//			{
//				q = sqlite->CreateQuery(R"=-=(
//insert into [account] ([username], [password])
//values (?, ?)
//)=-=");
//			}
//			if (!q) return;
//			if (q->SetParameters(username, password)) return;
//			if (!q->Execute()) return;
//			hasError = false;
//		}
//
//		xx::SQLiteQuery_p query_GetAccountByUsername;
//		Account_p GetAccountByUsername(char const* const& username)
//		{
//			hasError = true;
//			auto& q = query_GetAccountByUsername;
//			if (!q)
//			{
//				q = sqlite->CreateQuery(R"=-=(
//select [id], [username], [password]
//  from [account]
// where [username] = ?
//)=-=");
//			}
//			Account_p rtv;
//			if (!q) return rtv;
//			if (q->SetParameters(username)) return rtv;
//			if (!q->Execute([&](xx::SQLiteReader& sr)
//			{
//				rtv.Create(mp);
//				rtv->id = sr.ReadInt64(0);
//				rtv->username.Create(mp, sr.ReadString(1));	// 如果不创建默认实例
//				*rtv->password.Create(mp) = sr.ReadString(2);//*rtv->password = sr.ReadString(2); // 如果创建默认实例的 string 可以这样,  也可直接 Assign. BBuffer 同理
//			})) return rtv;
//			hasError = false;
//			return rtv;
//		}
//
//		xx::SQLiteQuery_p query_GetAccountsByUsernames;
//		xx::List_p<Account_p> GetAccountsByUsernames(xx::List_p<xx::String_p> const& usernames)
//		{
//			hasError = true;
//			auto& q = query_GetAccountsByUsernames;
//			{
//				s->Clear();
//				s->Append(R"=-=(
//select [id], [username], [password]
//  from [account]
// where [username] in )=-=");
//				s->SQLAppend(usernames);
//				q = sqlite->CreateQuery(s->C_str(), s->dataLen);
//			}
//			xx::List_p<Account_p> rtv;
//			if (!q) return rtv;
//			rtv.Create(mp);
//			if (!q->Execute([&](xx::SQLiteReader& sr)
//			{
//				auto& r = rtv->EmplaceMP();
//				r->id = sr.ReadInt64(0);
//				if (sr.IsDBNull(1)) r->username = nullptr; else *r->username.Create(mp) = sr.ReadString(1);	// 如果创建默认实例就要这样设 null, 后面 Create(mp) 不要
//				if (!sr.IsDBNull(2)) *r->password.Create(mp, sr.ReadString(2));
//			}))
//			{
//				rtv = nullptr;
//				return rtv;
//			}
//			hasError = false;
//			return rtv;
//		}
//
//		xx::SQLiteQuery_p query_GetAccountIdsByUsernames;
//		xx::List_p<int64_t> GetAccountIdsByUsernames(xx::List_p<xx::String_p> const& usernames)
//		{
//			hasError = true;
//			auto& q = query_GetAccountIdsByUsernames;
//			{
//				s->Clear();
//				s->Append(R"=-=(
//select [id]
//  from [account]
// where [username] in )=-=");
//				s->SQLAppend(usernames);
//				q = sqlite->CreateQuery(s->C_str(), s->dataLen);
//			}
//			xx::List_p<int64_t> rtv;
//			if (!q) return rtv;
//			rtv.Create(mp);
//			if (!q->Execute([&](xx::SQLiteReader& sr)
//			{
//				rtv->Add(sr.ReadInt64(0));
//			}))
//			{
//				rtv = nullptr;
//				return rtv;
//			}
//			hasError = false;
//			return rtv;
//		}
//
//		xx::SQLiteQuery_p query_GetAccountIdsByUsernames2;
//		xx::List_p<std::optional<int64_t>> GetAccountIdsByUsernames2(xx::List_p<xx::String_p> const& usernames)
//		{
//			hasError = true;
//			auto& q = query_GetAccountIdsByUsernames2;
//			{
//				s->Clear();
//				s->Append(R"=-=(
//select [id]
//  from [account]
// where [username] in )=-=");
//				s->SQLAppend(usernames);
//				q = sqlite->CreateQuery(s->C_str(), s->dataLen);
//			}
//			xx::List_p<std::optional<int64_t>> rtv;
//			if (!q) return rtv;
//			rtv.Create(mp);
//			if (!q->Execute([&](xx::SQLiteReader& sr)
//			{
//				if (sr.IsDBNull(0)) rtv->Emplace();
//				else rtv->Add(sr.ReadInt64(0));
//			}))
//			{
//				rtv = nullptr;
//				return rtv;
//			}
//			hasError = false;
//			return rtv;
//		}
//	};
//}

int main()
{
	PKG::AllTypesRegister();
	xx::MemPool mp;
	xx::SQLite_v sql(mp, "data.db");
	DB::SQLiteFuncs fs(sql);

	//auto r = sql->TableExists("account");
	//if (r < 0)
	//{
	//	mp.Cout("errCode = ", sql->lastErrorCode, "errMsg = ", sql->lastErrorMessage, "\n");
	//	goto LabEnd;
	//}
	//else if (r == 0)
	//{
	//	fs.CreateAccountTable();
	//	assert(!fs.hasError);

	//	DB::Account_p a(mp);

	//	*a->username.Create(mp) = "a";
	//	*a->password.Create(mp) = "1";
	//	fs.AddAccount(a);
	//	assert(!fs.hasError);

	//	auto b = std::move(a);

	//	fs.AddAccount2("b", "2");
	//	assert(!fs.hasError);
	//}

	//{
	//	auto a = fs.GetAccountByUsername("a");
	//	if (fs.hasError)
	//	{
	//		mp.Cout("errCode = ", fs.lastErrorCode(), "errMsg = ", fs.lastErrorMessage(), "\n");
	//		goto LabEnd;
	//	}
	//	else if (a == nullptr)
	//	{
	//		mp.Cout("can't find account a!\n");
	//	}
	//	else
	//	{
	//		mp.Cout("found account a! id = ", a->id, " password = ", a->password, "\n");
	//	}
	//}
	//{
	//	xx::List_p<xx::String_p> ss(mp);
	//	ss->EmplaceMP("a");
	//	ss->EmplaceMP("b");
	//	auto as = fs.GetAccountsByUsernames(ss);
	//	for (auto& a : *as)
	//	{
	//		mp.Cout(a, "\n");
	//	}
	//}

LabEnd:
	std::cin.get();
	return 0;
}

//xx::Dock<Service> s(mp);
//s->Run();


//struct Foo : xx::MPObject
//{
//	xx::String_p str;
//	Foo() : str(mempool()) {}
//	Foo(Foo&&) = default;
//	Foo(Foo const&) = delete;
//	Foo& operator=(Foo const&) = delete;
//	~Foo()
//	{
//	}
//};
//using Foo_p = xx::Ptr<Foo>;
//using Foo_v = xx::Dock<Foo>;
//namespace xx
//{
//	template<>
//	struct MemmoveSupport<Foo_v>
//	{
//		static const bool value = true;
//	};
//}
//
//Foo_p GetFoo(xx::MemPool& mp)
//{
//	Foo_p rtv;
//	rtv.Create(mp);
//	*rtv->str = "xxx";
//	return rtv;
//}
//



//{
//	xx::Dict_v<xx::String_p, xx::String_v> ss(mp);
//	ss->Add(xx::String_p(mp, "aa"), xx::String_v(mp, "2"));
//	ss->Add(xx::String_p(mp, "bbb"), xx::String_v(mp, "3"));
//	xx::String_p k(mp, "cccc");
//	xx::String_v v(mp, "4");
//	ss->Add(std::move(k), std::move(v));
//	k.Create(mp, "bbb");
//	auto idx = ss->Find(k);
//	mp.Cout(ss->ValueAt(idx));
//}

//{
//	auto foo = GetFoo(mp);
//}
//{
//	Foo_p foo;
//	foo.Create(mp);
//	*foo->str = "xx";
//	*foo.Create(mp)->str = "xxx";
//	foo = nullptr;
//}



//{
//	xx::List_v<Foo_p> foos(mp);
//	foos->Add(Foo_p(mp));
//	foos->Add(Foo_p(mp));
//}




