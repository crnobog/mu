#pragma once

#include <array>

namespace mu
{
	namespace dbg
	{
		namespace details
		{
			enum class LogArgType
			{
				C_Str,
				Unsigned
			};

			struct LogArg
			{
				LogArgType m_type;
				union
				{
					const char* m_c_str;
					uint64_t m_uint;
				};

				LogArg(const char* c_str)
					: m_type(LogArgType::C_Str)
					, m_c_str(c_str)
				{}

				LogArg(int32_t i32)
					: m_type(LogArgType::Unsigned)
					, m_uint(i32)
				{}

				LogArg(uint32_t u32)
					: m_type(LogArgType::Unsigned)
					, m_uint(u32)
				{}

				LogArg(size_t s)
					: m_type(LogArgType::Unsigned)
					, m_uint(s)
				{}
			};
		}

		void LogInternal(const details::LogArg*, size_t);
		
		template<typename ...ARGS>
		void Log(ARGS... args)
		{
			auto arr = std::array<details::LogArg, sizeof...(args)>{ {details::LogArg(args)...}};
			LogInternal(arr.data(), arr.size());
		}
	}
}