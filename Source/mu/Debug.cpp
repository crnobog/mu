#include "Debug.h"

#include <Windows.h>
#include <sstream>

void mu::dbg::LogInternal(const details::LogArg* args, size_t count)
{
	std::wostringstream o;

	for (size_t i =0; i < count; ++i)
	{
		switch (args->m_type)
		{
		case details::LogArgType::C_Str:
			o << args->m_c_str;
			break;
		case details::LogArgType::Unsigned:
			o << args->m_uint;
			break;
		default:
			throw new std::runtime_error("Invalid argument type to log");
		}

		++args;
	}

	o << std::endl;

	OutputDebugStringW(o.str().c_str());
}
