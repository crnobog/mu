#pragma once

#include <initializer_list>
#include <cstdint>

#include "Ranges.h"
#include "Algorithms.h"

template<typename T>
class ArrayView;

template<typename T>
class Array
{
	T* m_data		= nullptr;
	size_t m_num	= 0;
	size_t m_max	= 0;

public:
	Array()
	{
	}

	Array(std::initializer_list<T> init)
	{
		InitEmpty(init.size());
		for (auto&& item : init)
		{
			AddSafe(item);
		}
	}

	template<class RANGE>
	Array(RANGE&& r)
	{
		for (auto&& item : r)
		{
			Add(item);
		}
	}

	Array(const Array& other)
	{
		for (auto&& item : other)
		{
		InitSize(other.m_num);
			Add(item);
		}
	}

	Array(Array&& other)
	{
		std::swap(m_data, other.m_data);
		std::swap(m_num, other.m_num);
		std::swap(m_max, other.m_max);
	}

	Array& operator=(const Array& other)
	{
		// TODO
		return *this;
	}

	Array& operator=(Array&& other)
	{
		// TODO
		return *this;
	}

	~Array()
	{
		Destruct(0, m_num);
		if (m_data) { free(m_data); }
	}

	size_t Add(const T& item)
	{
		EnsureSpace(m_num + 1);
		return AddSafe(item);
	}

	size_t Add(T&& item) 
	{
		EnsureSpace(m_num + 1);
		return AddSafe(item);
	}

	size_t Num() const { return m_num; }
	size_t Max() const { return m_max; }

	const T& operator[](size_t index) const
	{
		return m_data[index];
	}

private:
	void InitEmpty(size_t num)
	{
		m_data = (T*)malloc(sizeof(T) * num);
		m_num = 0;
		m_max = num;
	}

	void EnsureSpace(size_t num)
	{
		if (num > m_max)
		{
			Grow(num > m_max * 2 ? num : m_max * 2);
		}
	}

	void Grow(size_t new_size)
	{
		T* new_data = (T*)malloc(sizeof(T) * new_size);
		auto from = mu::Range(m_data, m_num);
		auto to = mu::Range(new_data, m_num);
		mu::MoveConstruct(from, to);
		m_data = new_data;
		m_max = new_size;
	}

	size_t AddSafe(const T& item)
	{
		new(m_data + m_num) T(item);
		return m_num++;
	}

	void AddSafe(T&& item)
	{
		new(m_data + m_num) T(item);
		return m_num++;
	}

	void Destruct(size_t start, size_t num)
	{
		for (size_t i = start; i < start + num; ++i)
		{
			m_data[i].~T();
		}
	}
};