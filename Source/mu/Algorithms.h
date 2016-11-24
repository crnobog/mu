#pragma once

// Common algorithms operating on ranges
namespace mu
{
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

	template<typename RANGE, typename FUNC>
	auto Map(RANGE&& r, FUNC&& f)
	{
		for (; !r.IsEmpty(); r.Advance())
		{
			r.Front() = f(r.Front());
		}
	}
}