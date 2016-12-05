#pragma once

// Common algorithms operating on ranges
namespace mu
{
	// Move assign elements from the source to the destination
	template<typename DEST_RANGE, typename SOURCE_RANGE>
	auto Move(DEST_RANGE&& in_dest, SOURCE_RANGE&& in_source)
	{
		auto dest = Range(std::forward<DEST_RANGE>(in_dest));
		auto source = Range(std::forward<SOURCE_RANGE>(in_source));
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
	auto MoveConstruct(DEST_RANGE&& in_dest, SOURCE_RANGE&& in_source)
	{
		auto dest = Range(std::forward<DEST_RANGE>(in_dest));
		auto source = Range(std::forward<SOURCE_RANGE>(in_source));
		typedef std::remove_reference<decltype(dest.Front())>::type ELEMENT_TYPE;
		for (; !dest.IsEmpty() && !source.IsEmpty(); dest.Advance(), source.Advance())
		{
			new(&dest.Front()) ELEMENT_TYPE(std::move(source.Front()));
		}
		return dest;
	}

	template<typename RANGE, typename FUNC>
	auto Map(RANGE&& in_r, FUNC&& f)
	{
		for (auto&& r = Range(std::forward<RANGE>(in_r));
			 !r.IsEmpty(); r.Advance())
		{
			f(r.Front());
		}
	}

	template<typename RANGE, typename FUNC>
	auto Find(RANGE&& in_r, FUNC&& f)
	{
		auto&& r = Range(std::forward<RANGE>(in_r));
		for (; !r.IsEmpty(); r.Advance())
		{
			if (f(r.Front()))
			{
				return r;
			}
		}
		return r;
	}

	template<typename RANGE, typename FUNC>
	auto FindNext(RANGE&& in_r, FUNC&& f)
	{
		if (in_r.IsEmpty()) { return in_r; }

		auto&& r = Range(std::forward<RANGE>(in_r));
		r.Advance(); // Skip the last found value
		for (; !r.IsEmpty(); r.Advance())
		{
			if (f(r.Front()))
			{
				return r;
			}
		}
		return r;
	}

	// Fill the range with objects constructs with the given arguments
	template<typename RANGE, typename... ARGS>
	void FillConstruct(RANGE&& in_r, ARGS... args)
	{
		auto&& r = Range(std::forward<RANGE>(in_r));

		typedef std::decay<decltype(r.Front())>::type ITEM_TYPE;

		for (auto& item : r)
		{
			new(&item) ITEM_TYPE(std::forward<ARGS>(args)...);
		}
	}
	// Fill the range with objects constructs with the given arguments
	template<typename RANGE, typename... ARGS>
	void Fill(RANGE&& in_r, ARGS... args)
	{
		auto&& r = Range(std::forward<RANGE>(in_r));

		typedef std::decay<decltype(r.Front())>::type ITEM_TYPE;

		for (auto& item : r)
		{
			item = ITEM_TYPE(std::forward<ARGS>(args)...);
		}
	}
}