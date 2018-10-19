#pragma once
#include "xxlib_podstack.h"
#include "xxlib_mpobject.h"
#include <array>
#include <cassert>
#include <cstring>

namespace xxlib
{
	// todo: �� MemPool ����� template<typename ... Types> ��̬, �Ӷ����ⷱ�ӵ� register ����. ����ν"���յ�"���ػ� <T, ENABLE = void> д��, ������һЩ�������ߺ���


	// ���׿�ĺ����ڴ�������. �� 2^N �ߴ绮���ڴ������Ϊ, �� free ��ָ����� stack ���渴��
	// ���ڷ���������ڴ�, ���� �汾�� ������� ָ�� -8 ��. �����ж�ָ���Ƿ���ʧЧ
	// MPObject ����ʹ�� Create / Release ������������
	struct MemPool
	{
		static const uint32_t stacksCount = sizeof(size_t) * 8;
		PodStack<void*>* stacks;
		uint64_t versionNumber;

		// ������ֵ���ڴ�����ʧ��
		explicit MemPool()
			: stacks(new PodStack<void*>[stacksCount])
			, versionNumber(0)
		{
		}
		MemPool(MemPool const &o) = delete;
		MemPool& operator=(MemPool const &o) = delete;
		MemPool(MemPool &&o)
		{
			operator=((MemPool &&)o);
		}
		MemPool& operator=(MemPool &&o)
		{
			std::swap(stacks, o.stacks);
			std::swap(versionNumber, o.versionNumber);
			return *this;
		}
		~MemPool()
		{
			ClearStack();
			delete[] stacks;
			stacks = nullptr;

			if (typeId2CreateFuncs)
			{
				delete typeId2CreateFuncs;
				typeId2CreateFuncs = nullptr;
			}
			if (typeId2ParentTypeIds)
			{
				delete typeId2ParentTypeIds;
				typeId2ParentTypeIds = nullptr;
			}
		}

		inline void ClearStack()
		{
			for (uint32_t i = 0; i < stacksCount; ++i)
			{
				auto& stack = stacks[i];
				for (uint32_t i = 0; i < stack.dataLen; ++i)
				{
					::free(stack[i]);
				}
				stack.Clear();
			}
		}

		inline bool Empty() const
		{
			for (uint32_t i = 0; i < stacksCount; ++i)
			{
				auto& stack = stacks[i];
				if (stack.dataLen) return false;
			}
			return true;
		}

		/**********************************************************************/
		// ֱ�����ڴ��
		/**********************************************************************/

		// �ò���������ͷ��������� MPObjectHeader ������, ����ƫ�ƺ��ָ��
		// ʾ��: auto siz = Round2n(capacity * sizeof(T) + 8) - 8;	 xxx = (T*) mempool().Alloc( siz );   len = (uint32_t)(siz / sizeof(T));
		
		inline void* Alloc(size_t siz)
		{
			siz += sizeof(MPObjectHeader);		// �ճ����� MPObjectHeader �ĵض�
			auto idx = Calc2n(siz);
			if (siz > (size_t(1) << idx)) siz = size_t(1) << ++idx;
			if (idx >= stacksCount) return nullptr;

			void* rtv;
			if (!stacks[idx].TryPop(rtv)) rtv = (MPObjectHeader*)malloc(siz);

			auto p = (MPObjectHeader*)rtv;
			p->versionNumber = (++versionNumber) | ((uint64_t)idx << 56);		// �������±긽�����λ��, Free Ҫ��
			return p + 1;						// ָ�� header ��������򷵻�
		}

		inline void Free(void* p)
		{
			if (!p) return;
			auto h = (MPObjectHeader*)p - 1;	// ��ԭ��ԭʼָ��
			assert(h->versionNumber);			// �����Ͻ� free ��ʱ����汾�Ų�Ӧ���� 0. ����������ظ� Free
			stacks[h->mpIndex].Push(h);			// ���
			h->versionNumber = 0;				// ��հ汾��
		}

		inline void* Realloc(void *p, size_t newSize, size_t dataLen = std::numeric_limits<size_t>::max())
		{
			auto rtv = Alloc(newSize);
			auto oldSize = (size_t(1) << ((MPObjectHeader*)p - 1)->mpIndex) - sizeof(MPObjectHeader);
			memcpy(rtv, p, std::min(oldSize, dataLen));
			Free(p);
			return rtv;
		}


		/**********************************************************************/
		// ������ MPObject �Ķ��󴴽�
		/**********************************************************************/

		// �ò���������ͷ����� MPObjectHeaderEx
		template<typename T, typename ...Args>
		std::enable_if_t<std::is_base_of<MPObject, T>::value, T*> Create(Args &&... args)
		{
			// ���д��� ������ Alloc ����С��
			auto siz = sizeof(T) + sizeof(MPObjectHeaderEx);
			auto idx = Calc2n(siz);
			if (siz > (size_t(1) << idx)) siz = size_t(1) << ++idx;
			if (idx >= stacksCount) return nullptr;

			void* rtv;
			if (!stacks[idx].TryPop(rtv)) rtv = malloc(siz);

			auto p = (MPObjectHeaderEx*)rtv;
			p->versionNumber = (++versionNumber) | ((uint64_t)idx << 56);
			p->mempool = this;
			p->refCount = 1;
			p->typeId = TypeId<T>::value;
			p->tsFlags = 0;
			return new (p + 1) T(std::forward<Args>(args)...);
		}

		template<typename ...TS>
		void Release(TS &...ps)
		{
			std::initializer_list<int>{ (ReleaseCore(ps), 0)... };
		}

	protected:
		void ReleaseCore(MPObject* p)
		{
			if (p) p->Release();
		}
		template<typename T>
		void ReleaseCore(MPtr<T>& p)
		{
			if (p) p->Release();
		}

		friend MPObject;
		// ��Ӧ����� Create
		inline void FreeMPObject(void* p)
		{
			if (!p) return;

			auto h = (MPObjectHeaderEx*)p - 1;	// ��ԭ��ԭʼָ��
			assert(h->versionNumber);			// �����Ͻ� free ��ʱ����汾�Ų�Ӧ���� 0. ����������ظ� Free
			stacks[h->mpIndex].Push(h);			// ���
			h->versionNumber = 0;				// ��հ汾��
		}


		/**********************************************************************/
		// ���������� MPObject �Ķ��󴴽�
		/**********************************************************************/


		template<typename T>
		std::enable_if_t<!std::is_base_of<MPObject, T>::value> ReleaseCore(T* p)
		{
			if (!p) return;
			p->T::~T();
			Free(p);
		}
	public:

		// �ò���������ͷ����� MPObjectHeader
		template<typename T, typename ...Args>
		std::enable_if_t<!std::is_base_of<MPObject, T>::value, T*> Create(Args &&... args)
		{
			return new (Alloc(sizeof(T))) T(std::forward<Args>(args)...);
		}

		// һЩ����ʹ�õ�����

		template<typename T, typename ...Args>
		MPtr<T> CreateMPtr(Args &&... args)
		{
			return Create<T>(std::forward<Args>(args)...);
		}

		template<typename T, typename ...Args>
		void CreateTo(T*& outPtr, Args &&... args)
		{
			outPtr = Create<T>(std::forward<Args>(args)...);
		}
		template<typename T, typename ...Args>
		void CreateTo(MPtr<T>& outPtr, Args &&... args)
		{
			outPtr = CreateMPtr<T>(std::forward<Args>(args)...);
		}


		/**********************************************************************/
		// ��������ע�����( �����Ͻ����Ծ�̬��. �Ժ���˵ )
		/**********************************************************************/

		// Ǳ����, ����һ������, �� typeId ������������ӳ��
		std::array<void*(*)(MemPool*), 1 << sizeof(uint16_t) * 8>* typeId2CreateFuncs = nullptr;

		// �� ��ǰ typeId Ϊ�±�, ���ݴ� ���� typeId, �Դ��жϼ̳й�ϵ. 
		std::array<uint16_t, 1 << sizeof(uint16_t) * 8>* typeId2ParentTypeIds = nullptr;

	protected:

		template<size_t Index, typename Tuple>
		void RegisterTypeIdsCore(Tuple const* const& t)
		{
			typedef std::tuple_element_t<Index, Tuple> T;
			(*typeId2CreateFuncs)[TypeId<T>::value] = [](MemPool* mp) { return (void*)mp->Create<T>(*(MemPool*)nullptr); };	// T ��Ҫ�ṩ MemPool& ���캯���Ա��ڽ����������л����
		}
		template<size_t... Indexs, typename Tuple>
		void RegisterTypeIdsCore(std::index_sequence<Indexs...>, Tuple const* const& t)
		{
			std::initializer_list<int>{ (RegisterTypeIdsCore<Indexs, Tuple>(t), 0)... };
		}


		template<size_t... Indexs, typename Tuple, typename CT>
		void FillParentTypeId(std::index_sequence<Indexs...> const& idxs, Tuple const* const& t, CT* ct)
		{
			uint16_t parentTypeId = 0;
			std::initializer_list<int>{ ((
				std::is_base_of<std::tuple_element_t<Indexs, Tuple>, CT>::value && !std::is_same<std::tuple_element_t<Indexs, Tuple>, CT>::value
				? (parentTypeId = TypeId<std::tuple_element_t<Indexs, Tuple>>::value) : 0
				), 0)... };
			(*typeId2ParentTypeIds)[TypeId<CT>::value] = parentTypeId;
		}
		template<size_t... Indexs, typename Tuple>
		void FillParentTypeId(std::index_sequence<Indexs...> const& idxs, Tuple const* const& t)
		{
			std::initializer_list<int>{ (FillParentTypeId(idxs, t, (std::tuple_element_t<Indexs, Tuple>*)nullptr), 0)... };
		}

	public:
		// �� mempool ע�� typeId �����ʹ���������ӳ�� �Լ����ӹ�ϵ��ѯ��
		template<typename ...Types>
		void RegisterTypes()
		{
			if (!typeId2CreateFuncs)
			{
				typeId2CreateFuncs = new std::remove_pointer_t<decltype(typeId2CreateFuncs)>();
			}
			if (!typeId2ParentTypeIds)
			{
				typeId2ParentTypeIds = new std::remove_pointer_t<decltype(typeId2ParentTypeIds)>();
			}
			typeId2CreateFuncs->fill(0);
			typeId2ParentTypeIds->fill(0);

			RegisterTypeIdsCore(std::index_sequence_for<MPObject, Types...>(), (std::tuple<MPObject, Types...>*)nullptr);
			FillParentTypeId(std::index_sequence_for<MPObject, Types...>(), (std::tuple<MPObject, Types...>*)nullptr);
		}

		// ���� type id ����������ʵ��( ��Ҫ��Ŀ�����ʹ���Ĭ�Ϲ��캯�� )
		void* CreatePtrByTypeId(uint16_t typeId)
		{
			return (*typeId2CreateFuncs)[typeId] ? (*typeId2CreateFuncs)[typeId](this) : nullptr;
		}

		// ���� typeid �жϸ��ӹ�ϵ
		bool IsBaseOf(uint16_t baseTypeId, uint16_t typeId)
		{
			for (; typeId != baseTypeId; typeId = (*typeId2ParentTypeIds)[typeId])
			{
				if (!typeId) return false;
			}
			return true;
		}

		// ���� ���� �жϸ��ӹ�ϵ( ����ͬһ mempool ע�� )
		template<typename BT, typename T>
		bool IsBaseOf()
		{
			return IsBaseOf(TypeId<BT>::value, TypeId<T>::value);
		}

		// �ж� Ŀ������ �Ƿ�ӻ���ָ������( ĳ����ָ���Ƿ���ת����Ŀ������ ) ��ȡ�� dynamic_cast
		template<typename T>
		bool IsInheritOf(MPObject* base)
		{
			return IsBaseOf(base->typeId(), TypeId<T>::value);
		}
	};


	/**********************************************************************/
	// �������Ҫ�õ� MemPool �Ĺ��ܹ�ʵ��д������
	/**********************************************************************/
	inline void MPObject::Release()
	{
		assert(versionNumber());
		if (refCount() == 0 || --refCount()) return;
		this->~MPObject();
		// ��Ϊ Free ���ܵ��ǽ����� MPObjectHeader ƫ�Ƶ� offset �ʵ���
		mempool().FreeMPObject(this);
	}
}
