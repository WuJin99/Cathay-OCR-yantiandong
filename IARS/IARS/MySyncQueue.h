#pragma once
#include"SyncQueue.h"

template <class T>
class MySyncQueue : public SyncQueue<T>
{
public:
	MySyncQueue()
		:isAdd(true)
	{}
	~MySyncQueue() 
	{}
private:
	bool isAdd; //是否可以添加数据
public:
	virtual bool add(T pkt)
	{
		if (isAdd)
		{
			return SyncQueue<T>::add(pkt);
		}
		return false;
	}
	void setIsAdd(bool b)
	{
		isAdd = b;
	}
};

