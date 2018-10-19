#pragma once
#include <memory>
#include "xxlib_mempool.h"

namespace xxlib
{
	// ���� Dict �򻯶�����˫������( ���ǵ���Ҫ���ָ��ɶ��, �Ͳ��� �����ֵ�ķ����� )
	template <typename T, bool autoRelease = false>
	struct Links : public MPObject
	{
		static_assert(!autoRelease || (std::is_pointer<T>::value && IsMPObject<T>::value), "autoRelease == true cond is T* where T : MPObject");

		typedef T ValueType;
		struct Node
		{
			int             prev;
			int             next;
			T               value;					// ͬʱҲ���ڴ���ڴ浥Ԫ�±�( ����ʹ�� next �Ա��� RemoveAt �� next ���ܶ� ���� foreach ��ɾ )
		};

		int                 freeList;               // ���ɿռ�����ͷ( *(int*)&nodes[index].value ָ����һ��δʹ�õ�Ԫ )
		int                 freeCount;              // ���ɿռ�����
		int                 count;                  // ��ʹ�ÿռ���
		int					header;					// ���1���ڵ���±�
		int					tail;					// β���±�
		int                 nodesLen;				// �ڵ����鳤��
		Node               *nodes;                  // �ڵ�����

	protected:
		friend MemPool;
		Links(MemPool&) : nodes(nullptr) {}
		Links(int capacity = 16)
		{
			freeList = -1;
			freeCount = 0;
			count = 0;
			header = -1;
			tail = -1;
			auto nodesByteLen = Round2n(capacity * sizeof(Node) + 8) - 8;
			nodesLen = (int)(nodesByteLen / sizeof(Node));
			nodes = (Node*)mempool().Alloc(nodesLen * sizeof(Node));
		}
		~Links()
		{
			if (nodes)
			{
				DeleteNodes();
				mempool().Free(nodes);
				nodes = nullptr;
			}
		}

		Links(Links const &o) = delete;
		Links& operator=(Links const &o) = delete;


		template<typename U = T>
		void ItemAddRef(std::enable_if_t<!autoRelease, U> &p) {}
		template<typename U = T>
		void ItemAddRef(std::enable_if_t< autoRelease, U> &p)
		{
			if (p) p->AddRef();
		}
		template<typename U = T>
		void ItemRelease(std::enable_if_t<!autoRelease, U> &p) {}
		template<typename U = T>
		void ItemRelease(std::enable_if_t< autoRelease, U> &p)
		{
			if (p) p->Release();
		}
	public:

		// ׷�ӵ�β��
		template<typename ...VPS>
		int EmplaceBack(VPS &&... vps)
		{
			int index = EmplaceCore(std::forward<VPS>(vps)...);

			nodes[index].prev = tail;
			nodes[index].next = -1;
			if (tail != -1)
			{
				nodes[tail].next = index;
			}
			tail = index;
			if (header == -1) header = index;

			return index;
		}

		// ���뵽ͷ��
		template<typename ...VPS>
		int EmplaceFront(VPS &&... vps)
		{
			int index = EmplaceCore(std::forward<VPS>(vps)...);

			nodes[index].prev = -1;
			nodes[index].next = header;
			if (header != -1)
			{
				nodes[header].prev = index;
			}
			header = index;
			if (tail == -1) tail = index;

			return index;
		}

		int PushBack(T && v)
		{
			return EmplaceBack((T&&)v);
		}
		int PushBack(T const& v)
		{
			return EmplaceBack(v);
		}

		int PushFront(T && v)
		{
			return EmplaceFront((T&&)v);
		}
		int PushFront(T const& v)
		{
			return EmplaceFront(v);
		}

		// �� v ���뵽 tarIdx �ĺ���( tarIdx Ϊ -1 ����뵽��ǰ�� ), �����ڴ���ַ����
		template<typename VT>
		int InsertAt(int tarIdx, VT && v)
		{
			assert(tarIdx >= 0 && tarIdx < count && nodes[tarIdx].prev != -2);
			assert(header == tarIdx && nodes[tarIdx].prev == -1 || header != tarIdx);
			assert(tail == tarIdx && nodes[tarIdx].next == -1 || tail != tarIdx);
			assert(header == tail && tarIdx == header && Count() == 1 || header != tail);

			if (tarIdx == -1) return PushFront(std::forward<VT>(v));
			else if (tarIdx == tail) return PushBack(std::forward<VT>(v));
			else
			{
				int index = EmplaceCore(std::forward<VT>(v));
				nodes[index].prev = tarIdx;
				nodes[index].next = nodes[tarIdx].next;
				if (nodes[tarIdx].next != -1)
				{
					nodes[nodes[tarIdx].next].prev = index;
				}
				nodes[tarIdx].next = index;
				return index;
			}
		}

		// todo: Insert Range?


		void RemoveAt(int index)
		{
			assert(index >= 0 && index < count && nodes[index].prev != -2);
			assert(header == index && nodes[index].prev == -1 || header != index);
			assert(tail == index && nodes[index].next == -1 || tail != index);
			assert(header == tail && index == header && Count() == 1 || header != tail);

			if (nodes[index].next != -1)
			{
				nodes[nodes[index].next].prev = nodes[index].prev;
			}
			if (index == header) header = nodes[index].next;

			if (nodes[index].prev != -1)
			{
				nodes[nodes[index].prev].next = nodes[index].next;
			}
			if (index == tail) tail = nodes[index].prev;

			ItemRelease(nodes[index].value);
			nodes[index].value.T::~T();
			*(int*)&nodes[index].value = freeList;     // ��ǰ�ڵ��ѱ��Ƴ�����, ���� value �����ɽڵ�����ͷ�±�
			freeList = index;
			freeCount++;

		}

		// ���������Ƴ�
		int Remove(T const& v)
		{
			for (int i = header; i != -1; i = nodes[i].next)
			{
				if (EqualsTo(nodes[i].value, v))
				{
					RemoveAt(i);
					return i;
				}
			}
			return -1;
		}

		// ֻ֧��û����ʱ���ݻ�ռ��þ�����( �������������, ����ʱ�� ������� �Դ� )
		void Reserve(int capacity = 0)
		{
			assert(count == 0 || count == nodesLen);          // ȷ�����ݺ���ʹ������
			if (capacity == 0) capacity = count * 2;            // 2������
			if (capacity <= nodesLen) return;
			auto nodesByteLen = Round2n(capacity * sizeof(Node) + 8) - 8;	// ���д versionNumber ������
			nodesLen = (int)(nodesByteLen / sizeof(Node));

			if (std::is_trivial<T>::value || MemmoveSupport<T>::value)
			{
				nodes = (Node*)mempool().Realloc(nodes, nodesByteLen);
			}
			else
			{
				auto newNodes = (Node*)mempool().Alloc(nodesByteLen);
				for (int i = 0; i < count; ++i)
				{
					new (&newNodes[i].value) T((T&&)nodes[i].value);
					nodes[i].value.T::~T();
				}
				mempool().Free(nodes);
				nodes = newNodes;
			}
		}

		int Find(T const &v) const
		{
			for (int i = header; i != -1; i = nodes[i].next)
			{
				if (EqualsTo(nodes[i].value, v))
				{
					return i;
				}
			}
			return -1;
		}

		// �ɴ���һ����Դ���պ���������
		void Clear()
		{
			if (!count) return;
			DeleteNodes();
			header = -1;
			tail = -1;
			freeList = -1;
			freeCount = 0;
			count = 0;
		}

	public:

		int Count() const
		{
			return int(count - freeCount);
		}

		bool Empty()
		{
			return count == 0;
		}

		T& At(int index)
		{
			assert(index >= 0 && index < count && nodes[index].prev != -2);
			return nodes[index].value;
		}
		Node& NodeAt(int index)
		{
			assert(index >= 0 && index < count && nodes[index].prev != -2);
			return nodes[index];
		}

		T const& At(int index) const
		{
			return const_cast<Links*>(this)->IndexAtValue(index);
		}
		Node const& NodeAt(int index) const
		{
			return const_cast<Links*>(this)->At(index);
		}

#ifndef NDEBUG
		bool IndexExists(int index) const
		{
			return index >= 0 && index < count && nodes[index].prev != -2;
		}
#endif


		virtual void ToString(String &str) const override;

		// todo: ���л�֧��?


		/*******************************************************************************/
		// ֻ��Ϊ���� for( auto &c : 
		/*******************************************************************************/

		// ע��: ���Ҫ�� for �Ĺ����� RemoveAt + Push, ֱ���� iter ������ȫ.
		// ��: ��� freeCount Ϊ 0, ���ʾ�ڴ��޿ն�. ��������Ļ���ֱ��ɨ����. �� count Ϊ�����޶�
		/*
		if (!list->freeCount)
		{
			// ����ɨ�����. �ʺ�ɨ��֮��.
			for( int i = 0; i < list->count; ++i )
			{
				list->nodes[i].....
			}
		}
		else
		{
			// ˳��, ɾ + �� ��ȫ
			auto i = list->header;
			while (i != -1)
			{
				auto nexti = list->nodes[i].next;	// �ȶ��� next �Ա��⵱ǰ�ڵ�ʧЧ
				// remove at ? add ? ...
				i = nexti;							// ����ɨ
			}
		}
		// ���� RemoveAt(i) �������������� Push/Insert ɶ��
		for (auto& i : *list) list->nodes[i]........
		*/


		struct Iter
		{
			Links& pl;
			int i;
			bool operator!=(Iter const& other) { return i != other.i; }
			Iter& operator++()
			{
				if (i != -1) i = pl.nodes[i].next;
				return *this;
			}
			int& operator*() { return i; }
		};
		Iter begin()
		{
			return Iter{ *this, header };
		}
		Iter end()
		{
			return Iter{ *this, -1 };
		}





	protected:

		template<typename ...VPS>
		int EmplaceCore(VPS &&... vps)
		{
			int index;
			if (freeCount > 0)                          // ��� ���ɽڵ����� ����, ȡһ����������
			{                                           // ��Щ�ڵ����� Remove ����. value ָ����һ��
				index = freeList;
				freeList = *(int*)&nodes[index].value;
				freeCount--;
			}
			else
			{
				if (count == nodesLen)					// ���пսڵ㶼�ù���, Resize
				{
					Reserve();
				}
				index = count;							// ָ�� Resize ����Ŀռ����
				count++;
			}

			new (&nodes[index].value) T(std::forward<VPS>(vps)...);
			ItemAddRef(nodes[index].value);
			return index;
		}

		void DeleteNodes()                                    // ���� ����, Clear
		{
			for (int i = header; i != -1; i = nodes[i].next)
			{
				ItemRelease(nodes[i].value);
				nodes[i].value.T::~T();
#ifndef NDEBUG
				nodes[i].prev = -2;
#endif
			}
		}

	};
}
