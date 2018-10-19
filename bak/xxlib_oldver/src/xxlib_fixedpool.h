#pragma once
#include "xxlib_list.h"

namespace xxlib
{
	template<typename T>
	struct FixedPool : public List<T>
	{
		static_assert(std::is_pod<T>::value && sizeof(T) >= sizeof(int), "");
		typedef List<T> BaseType;

		int         freeList;					// ���ɿռ�����ͷ( next ָ����һ��δʹ�õ�Ԫ )
		int         freeCount;					// �� Free ��( ��� Alloc �Ǵ� freeList ����, ��ֵ�� -1 )

	protected:
		friend MemPool;

		explicit FixedPool(int capacity = 16)
			: BaseType((int)Round2n(capacity))
			, freeList(-1)
			, freeCount(0)
		{
		}

		FixedPool(FixedPool const &o) = delete;
		FixedPool operator=(FixedPool const &o) = delete;

	public:
		int Alloc()
		{
			int index;
			if (freeCount > 0)                  // ��� ���ɽڵ����� ����, ȡһ����������
			{
				index = freeList;
				freeList = *(int*)&this->At(index);
				freeCount--;
			}
			else
			{
				if (this->dataLen == this->bufLen)          // ���пսڵ㶼�ù���, ����
				{
					Reserve(this->dataLen + 1);
				}
				index = this->dataLen;
				this->dataLen++;
			}
			return index;
		}

		void Free(int index)
		{
			*(int*)&this->At(index) = freeList;
			freeList = index;
			freeCount++;
		}

		int Size() const
		{
			return this->dataLen - freeCount;
		}
	};
}
