#include "CppUnitTest.h"
#include "../mu/Ranges.h"

#include <memory>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace mu_core_tests
{
	TEST_CLASS(PointerRangeTests)
	{
	public:
		TEST_METHOD(Size)
		{
			const size_t size = 100;
			int ptr[size] = {};
			{
				auto r = Range(ptr, size);

				Assert::IsTrue(r.HasSize, nullptr, LINE_INFO());
				Assert::AreEqual(size, r.Size(), nullptr, LINE_INFO());
			}
			{
				auto r = Range(ptr, ptr + size);

				Assert::IsTrue(r.HasSize, nullptr, LINE_INFO());
				Assert::AreEqual(size, r.Size(), nullptr, LINE_INFO());
			}
			{
				auto r = Range(ptr);

				Assert::IsTrue(r.HasSize, nullptr, LINE_INFO());
				Assert::AreEqual(size, r.Size(), nullptr, LINE_INFO());
			}
		}

		TEST_METHOD(MovePrimitive)
		{
			int from[] = { 0,1,2,3,4,5,6,7,8,9 };
			int to[20] = {};

			auto source = Range(from);
			auto dest = Range(to);
			Assert::AreEqual(size_t(10), source.Size(), nullptr, LINE_INFO());
			Assert::AreEqual(size_t(20), dest.Size(), nullptr, LINE_INFO());

			auto dest2 = Move(dest, source);
			Assert::AreEqual(dest.Size() - source.Size(), dest2.Size(), nullptr, LINE_INFO());
			Assert::AreEqual(size_t(10), source.Size(), nullptr, LINE_INFO());

			auto dest3 = Move(dest2, source);
			Assert::AreEqual(dest.Size() - source.Size()*2, dest3.Size(), nullptr, LINE_INFO());
			Assert::IsTrue(dest3.IsEmpty(), nullptr, LINE_INFO());
			
			int index = 0;
			for (auto r = Range(to); !r.IsEmpty(); r.Advance(), ++index)
			{
				Assert::AreEqual(from[index % 10], r.Front(), nullptr, LINE_INFO());
			}
		}

		TEST_METHOD(ZipRanges)
		{
			int as[] = { 0,1,2,3,4,5,6,7,8,9 };
			float bs[] = { 9, 8, 7, 6, 5, 4, 3, 2, 1, 0};

			int index = 0;
			auto r = Zip(Range(as), Range(bs));
			Assert::IsTrue(r.HasSize, nullptr, LINE_INFO());
			Assert::IsFalse(r.IsEmpty(), nullptr, LINE_INFO());
			Assert::AreEqual(size_t(10), r.Size(), nullptr, LINE_INFO());

			for (; !r.IsEmpty(); r.Advance(), ++index)
			{
				std::tuple<int&, float&> front = r.Front();
				Assert::AreEqual(as[index], std::get<0>(front), nullptr, LINE_INFO());
				Assert::AreEqual(bs[index], std::get<1>(front), nullptr, LINE_INFO());
			}
		}

		TEST_METHOD(IotaRange)
		{
			size_t i = 0;
			auto r = Iota();
			Assert::IsFalse(r.HasSize);
			for (; !r.IsEmpty() && i < 10; ++i, r.Advance())
			{
				Assert::AreEqual(i, r.Front());
			}
		}

		TEST_METHOD(ZipIotas)
		{
			size_t i = 0;
			auto r = Zip(Iota(), Iota(1));
			Assert::IsFalse(r.HasSize);
			for (; !r.IsEmpty() && i < 10; ++i, r.Advance())
			{
				std::tuple<size_t, size_t> f = r.Front();
				Assert::AreEqual(1+ std::get<0>(f), std::get<1>(f));
			}
		}

		TEST_METHOD(ZipIotaWithFinite)
		{
			float fs[] = { 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 };
			auto frange = Range(fs);
			auto r = Zip(Iota(), frange);

			Assert::IsTrue(r.HasSize);
			Assert::AreEqual(frange.Size(), r.Size());

			int i = 0;
			for (; !r.IsEmpty(); r.Advance(), ++i)
			{
				std::tuple<size_t, float&> f = r.Front();
				Assert::AreEqual(size_t(i), std::get<0>(f));
				Assert::AreEqual(fs[i], std::get<1>(f));
			}
		}
	};
}