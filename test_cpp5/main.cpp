﻿// todo: SendRequest 返回值改为返回 std::pair<int, uint>

// todo: noexcept 嵌套狂去一波( 编译器无法编译期查出问题, 会直接crash ). 内存不足先用 assert 来解决. 不抛异常
// todo: 用 sg 语法改进各种库
// todo: xx_uv 从 c# 那边复制备注


#include <xx_uv.h>

inline xx::MemPool mp;
inline xx::BBuffer_p bbSend(mp.MPCreate<xx::BBuffer>());
inline xx::Stopwatch sw;

inline void HandlePkg(xx::UvTcpUdpBase* peer, xx::BBuffer& recvData)
{
	xx::Object_p pkg;
	if (recvData.ReadRoot(pkg) || !pkg)
	{
		peer->DisconnectImpl();
		return;
	}
	switch (pkg.GetTypeId())
	{
		case xx::TypeId_v<xx::BBuffer>:
		{
			auto& bb = pkg.As<xx::BBuffer>();
			int v = 0;
			if (bb->Read(v))
			{
				peer->DisconnectImpl();
				return;
			}
			v++;
			if (v >= 100000)
			{
				std::cout << sw() << std::endl;
				return;
			}
			bbSend->Clear();
			bbSend->Write(v);
			peer->Send(bbSend);
		}
		break;
		default:
			break;
	}
}



int main()
{
	xx::MemPool::RegisterInternals();
	xx::UvLoop loop(&mp);
	auto listener = loop.CreateTcpListener();
	listener->Bind("0.0.0.0", 12345);
	listener->OnAccept = [](auto peer)
	{
		peer->OnReceivePackage = [peer](auto& recvData)
		{
			HandlePkg(peer, recvData);
		};
	};
	listener->Listen();

	xx::UvTcpClient client(loop);
	client.OnConnect = [&](auto state)
	{
		if (client.Alive())
		{
			int v = 0;
			bbSend->Clear();
			bbSend->Write(v);
			client.Send(bbSend);
		}
	};
	client.OnReceivePackage = [&client](auto& recvData)
	{
		HandlePkg(&client, recvData);
	};
	client.ConnectEx("127.0.0.1", 12345);

	sw.Reset();

	return loop.Run();
}

