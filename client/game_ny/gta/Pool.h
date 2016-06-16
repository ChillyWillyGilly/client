#pragma once

class GAMESPEC_EXPORT CPool
{
public:
	void* m_pObjects;
	char* m_pFlags;
	int m_nCount;
	int m_nEntrySize;
	int m_nTop;
	int m_nUsed;
	bool m_bAllocated;

public:
	template <typename T>
	T* GetAt(int index)
	{
		if (index >= m_nCount)
		{
			return nullptr;
		}

		if (m_pFlags[index] < 0)
		{
			return nullptr;
		}

		return (T*)(((char*)m_pObjects) + (index * m_nEntrySize));
	}

	int GetCount()
	{
		return m_nCount;
	}

	int GetIndex(void* pointer)
	{
		int index = (((uintptr_t)pointer) - ((uintptr_t)m_pObjects)) / m_nEntrySize;

		if (index < 0 || index >= m_nCount)
		{
			FatalError("Invalid pool pointer passed");
		}

		return index;
	}
};

template <class T, class TBase>
class CPoolExtensions
{
private:
	T* m_pObjects;
	CPool* m_basePool;

public:
	CPoolExtensions(CPool* basePool)
		: m_basePool(basePool)
	{
		m_pObjects = (T*)(operator new(sizeof(T) * basePool->GetCount()));
	}

	~CPoolExtensions()
	{
		delete m_pObjects;
	}

	T* GetAt(int index)
	{
		if (index >= m_basePool->GetCount())
		{
			return nullptr;
		}

		if (!m_basePool->IsValidIndex(index))
		{
			return nullptr;
		}

		return &m_pObjects[index];
	}

	T* GetAtPointer(TBase* baseEntry)
	{
		return &m_pObjects[m_basePool->GetIndex(baseEntry)];
	}
};

class GAMESPEC_EXPORT CPools
{
private:
	static CPool*& ms_pBuildingPool;

public:
	static inline CPool* GetBuildingPool() { return ms_pBuildingPool; }
};