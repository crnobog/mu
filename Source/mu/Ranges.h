#pragma once

#include <tuple>
#include <limits>

#include "Functors.h"

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

namespace mu
{
	// Functions to automatically construct ranges from pointers/arrays
	template<typename T>
	auto Range(T* ptr, size_t num)
	{
		return Range(ptr, ptr + num);
	}

	template<typename T>
	auto Range(T* start, T* end)
	{
		return ranges::PointerRange<T>(start, end);
	}

	template<typename T, size_t SIZE>
	auto Range(T(&arr)[SIZE])
	{
		return Range(arr, arr + SIZE);
	}
	
	template<typename... RANGES>
	auto Zip(RANGES... ranges) { return ranges::ZipRange<RANGES...>(ranges...); }

	template<typename T=size_t>
	inline auto Iota(T start = 0) { return ranges::IotaRange<T>(start); }

	namespace ranges
	{
		using mu::functor::Fold;
		using mu::functor::FoldOr;
		using mu::functor::FMap;
		using mu::functor::FMapVoid;

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
			const T& Front() const { return *m_start; }
			size_t Size() const { return m_end - m_start; }
		};

		// A linear infinite range over an integral type
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

		// ZipRange combines multiple ranges and iterates them in lockstep
		template<typename... RANGES>
		class ZipRange
		{
			std::tuple<RANGES...> m_ranges;

		public:
			static constexpr bool HasSize = FoldOr(RANGES::HasSize...);

			ZipRange(RANGES... ranges) : m_ranges(ranges...)
			{
			}

			bool IsEmpty() const
			{
				return FoldOr(FMap<details::RangeIsEmpty>(m_ranges));
			}

			void Advance()
			{
				FMapVoid<details::RangeAdvance>(m_ranges);
			}

			auto Front()
			{
				return FMap<details::RangeFront>(m_ranges);
			}

			template<typename T = ZipRange, typename std::enable_if<T::HasSize, int>::type = 0>
			size_t Size() const
			{
				return Fold<details::RangeMinSizeFolder>(
					std::numeric_limits<size_t>::max(), m_ranges);
			}
		};

		namespace details
		{
			// Helpers for calling members in variadic template expansion
			template<typename RANGE>
			struct RangeIsEmpty { bool operator()(const RANGE& r) { return r.IsEmpty(); } };

			template<typename RANGE>
			struct RangeAdvance { void operator()(RANGE& r) { r.Advance(); } };

			template<typename RANGE>
			struct RangeFront
			{
				auto operator()(RANGE& r) -> decltype(r.Front())
				{
					return r.Front();
				}
			};

			// Functor for folding over ranges of finite/infinite size and picking the minimum size
			template<typename RANGE>
			struct RangeMinSizeFolder
			{
				template<typename T = RANGE, typename std::enable_if<T::HasSize, int>::type = 0>
				constexpr size_t operator()(size_t s, const RANGE& r)
				{
					size_t rs = r.Size();
					return rs < s ? rs : s;
				}

				template<typename T = RANGE, typename std::enable_if<!T::HasSize, int>::type = 0>
				constexpr size_t operator()(size_t s, const RANGE&)
				{
					return s;
				}
			};

			template<typename RANGE>
			using RangeFrontType = decltype(std::declval<RANGE>().Front());
		}
	}
}