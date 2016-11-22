#pragma once

#include <tuple>
#include <limits>

#include "Metaprogramming.h"

// Prototype of a forward range:
//	template<typename T>
//	class ForwardRange
//	{
//		enum {HasSize = ?};
//
//		void Advance();
//		bool IsEmpty();
//		T& Front();
//		size_t Size(); // if HasSize == 1
//	};

// Functions to automatically construct ranges from pointers/arrays
template<typename T>
auto Range(T* ptr, size_t num)
{
	return Range(ptr, ptr + num);
}

template<typename T>
auto Range(T* start, T* end)
{
	return std::move(PointerRange<T>(start, end));
}

// A linear forward range over raw memory of a certain type
template<typename T>
class PointerRange
{
	T* m_start, *m_end;
public:
	enum { HasSize = 1 };

	PointerRange(T* start, T* end)
		: m_start(start)
		, m_end(end)
	{}

	void Advance()
	{
		++m_start;
	}

	bool IsEmpty() const { return m_start >= m_end; }
	T& Front() { return *m_start; }
	size_t Size() const { return m_end - m_start; }
};

// Move assign elements from the source to the destination
template<typename DEST_RANGE, typename SOURCE_RANGE>
auto Move(DEST_RANGE dest, SOURCE_RANGE source)
{
	for (; !dest.IsEmpty() && !source.IsEmpty(); dest.Advance(), source.Advance())
	{
		dest.Front() = std::move(source.Front());
	}
	return dest;
}

// Move CONSTRUCT elements from the source into the destination.
// Assumes the destination is uninitialized or otherwise does not 
//	require destructors/assignment operators to be called.
template<typename DEST_RANGE, typename SOURCE_RANGE>
auto MoveConstruct(DEST_RANGE dest, SOURCE_RANGE source)
{
	typedef std::remove_reference<decltype(dest.Front())>::type ELEMENT_TYPE;
	for (; !dest.IsEmpty() && !source.IsEmpty(); dest.Advance(), source.Advance())
	{
		new(&dest.Front()) ELEMENT_TYPE(std::move(source.Front()));
	}
	return dest;
}

template<typename T>
class IotaRange
{
	T m_it = 0;
public:
	enum { HasSize = 0 };

	IotaRange() {}
	IotaRange(T start = 0) : m_it(start)
	{
	}

	void Advance() { ++m_it; }
	bool IsEmpty() const { return false; }
	T Front() { return m_it; }
};

inline IotaRange<size_t> Iota(size_t start=0) { return IotaRange<size_t>(start); }


template<bool HEAD, bool... TAIL>
struct AllOfImpl
{
	static constexpr bool Value = HEAD && AllOfImpl<TAIL...>::Value;
};

template<bool HEAD>
struct AllOfImpl<HEAD>
{
	static constexpr bool Value = HEAD;
};

template<bool... VALS>
using AllOf = AllOfImpl<VALS...>;

template<typename... RANGES>
class ZipRange
{
	std::tuple<RANGES...> m_ranges;

public:
	enum { HasSize = FoldOr(RANGES::HasSize...) };

	ZipRange(RANGES... ranges) : m_ranges(ranges...)
	{
	}

	bool IsEmpty() const 
	{
		return FoldOr(FMap<RangeIsEmpty>(m_ranges));
	}

	void Advance()
	{
		FMapVoid<RangeAdvance>(m_ranges);
	}

	auto Front()
	{
		return FMap<RangeFront>(m_ranges);
	}

	template<typename T=ZipRange, typename std::enable_if<T::HasSize, int>::type=0>
	size_t Size() const
	{
		return Fold<RangeMinSizeFolder>(std::numeric_limits<size_t>::max(), m_ranges);
	}
};

template<typename... RANGES>
auto Zip(RANGES... ranges) { return ZipRange<RANGES...>(ranges...); }



// Helpers for calling members in variadic template expansion
template<typename RANGE>
struct RangeIsEmpty { bool operator()(const RANGE& r) { return r.IsEmpty(); } };

template<typename RANGE>
struct RangeAdvance { void operator()(RANGE& r) { r.Advance(); } };

template<typename RANGE>
struct RangeFront 
{ 
	// TODO: check return type works with const& and value
	auto operator()(RANGE& r) -> decltype(r.Front())
	{ 
		return r.Front();
	} 
};

template<typename RANGE>
struct RangeMinSizeFolder
{
	// TODO: Would be nice not to need std::decay here, is something in Fold wrong?
	template<typename T=RANGE, typename std::enable_if<std::decay<T>::type::HasSize, int>::type=0>
	constexpr size_t operator()(size_t s, const RANGE& r)
	{
		size_t rs = r.Size();
		return rs < s ? rs : s;
	}

	template<typename T = RANGE, typename std::enable_if<!std::decay<T>::type::HasSize, int>::type=0>
	constexpr size_t operator()(size_t s, const RANGE&)
	{
		return s;
	}
};