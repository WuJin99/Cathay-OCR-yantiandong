#pragma once
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
//ThreadQueue�������µ��ص㣺
//�����������(Reader) �Ͷ��д��(Writer) �����ط��ʶ��С�
//Ԫ�س��еĲ�������������ֱ�����в�Ϊ�ա�
//Ԫ�س���ʱ��֤�쳣��ȫ�ԡ�
template <typename T, typename Container = std::queue<T>>
class ThreadQueue
{
public:
	using container_type = Container;
	using value_type = typename Container::value_type;
	using reference = typename Container::reference;
	using const_reference = typename Container::const_reference;
	using size_type = typename Container::size_type;
	using mutex_type = std::mutex;
	using condition_variable_type = std::condition_variable;
private:
	Container                queue_;
	mutable mutex_type       mutex_;
	condition_variable_type  cond_;
	typedef void(*ClearFunc)(T& t);
	ClearFunc m_clearFunc;

public:
	ThreadQueue() {
		m_clearFunc = nullptr;
	};
	~ThreadQueue(){
		clear();
	}
	ThreadQueue(const ThreadQueue &) = delete;
	ThreadQueue &operator=(const ThreadQueue &) = delete;
	// Ҫ�Ƕ����ǿյģ���ôpop()������������
	void get(reference elem)
	{
		std::unique_lock<mutex_type> lock(mutex_);
		cond_.wait(lock, [this]() {  return !queue_.empty();  });
		elem = std::move(queue_.front());
		queue_.pop();
	}
	// Ҫ�Ƕ����ǿյģ�����try_pop()�����������ض���������
	bool try_get(reference elem)
	{
		std::unique_lock<mutex_type> lock(mutex_);
		if (queue_.empty()) {
			return false;
		}
		elem = std::move(queue_.front());
		queue_.pop();
		return true;
	}

	bool empty() const
	{
		std::lock_guard<mutex_type> lock(mutex_);
		return queue_.empty();
	}

	size_type size() const
	{
		std::lock_guard<mutex_type> lock(mutex_);
		return queue_.size();
	}

	/*void clear()
	{
	std::lock_guard<mutex_type> lock(mutex_);
	Container empty;
	swap(empty, queue_);

	}*/

	void clear()
	{
		std::lock_guard<mutex_type> lock(mutex_);
		if (m_clearFunc)
		{
			while (!queue_.empty())
			{
				auto eleml = std::move(queue_.front());
				queue_.pop();
				m_clearFunc(eleml);
			}
		}
		else
		{
			Container empty;
			swap(empty, queue_);
		}
	}

	void setfunc(ClearFunc f)
	{
		m_clearFunc = f;
	}

	void add(const value_type &elem)
	{
		{
			std::lock_guard<mutex_type> lock(mutex_);
			queue_.push(elem);
		}
		cond_.notify_one();
	}
	void add(value_type &&elem)
	{
		{
			std::lock_guard<mutex_type> lock(mutex_);
			queue_.push(std::move(elem));
		}
		cond_.notify_one();
	}
};