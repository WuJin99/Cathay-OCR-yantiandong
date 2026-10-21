#ifndef SYNCQUEUE_H
#define SYNCQUEUE_H

#include <deque>
#include <boost/thread.hpp>

template <class T>
class SyncQueue
{
protected:
	boost::recursive_mutex m_mutex;
	std::deque<T> m_queue;
	typedef void(*ClearFunc)(T& t);
	ClearFunc m_clearFunc;
public:
	SyncQueue()
	{
		m_clearFunc = nullptr;
	}
	virtual ~SyncQueue()
	{
		clear();
	}
	virtual bool add(T pkt)
	{
		boost::recursive_mutex::scoped_lock lock(m_mutex);
		m_queue.push_back(pkt);
		return true;
	}

	bool get(T& pkt)
	{
		boost::recursive_mutex::scoped_lock lock(m_mutex);
		if (!m_queue.size())
		{
			return false;
		}
		pkt = m_queue.front();
        m_queue.pop_front();
		return true;
	}
    int size()
    {
        boost::recursive_mutex::scoped_lock lock(m_mutex);
        return m_queue.size();
    }
	void clear()
	{
		boost::recursive_mutex::scoped_lock lock(m_mutex);
		if (m_clearFunc)
		{
			while (!m_queue.empty())
			{
				T pkt = m_queue.front();
				m_queue.pop_front();
				m_clearFunc(pkt);
			}
		}
		else
		{
			m_queue.clear();
		}
	}
	void setfunc(ClearFunc f)
	{
		m_clearFunc = f;
	}
};
#endif // SYNCQUEUE_H