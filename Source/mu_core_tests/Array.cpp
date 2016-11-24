#include "CppUnitTest.h"
#include "../mu/Array.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace mu_core_tests_array
{
	static int ConstructCount = 0;
	static int DestructCount = 0;
	static int CopyCount = 0;
	static int MoveCount = 0;

	static void ResetCounts()
	{
		ConstructCount = 0;
		DestructCount = 0;
		CopyCount = 0;
		MoveCount = 0;
	}

	struct Element
	{
		int32_t data;

		Element() : data(0)
		{
			++ConstructCount;
		}
		Element(int32_t d) : data(d)
		{
			++ConstructCount;
		}
		Element( const Element& other ) : data(other.data)
		{
			++CopyCount;
		}
		Element(Element&& other) : data(other.data)
		{
			++MoveCount;
		}
		Element& operator=(const Element& other)
		{
			++CopyCount;
			return *this;
		}
		Element& operator=(Element&& other)
		{
			++MoveCount;
			return *this;
		}
		~Element()
		{
			++DestructCount;
		}
	};

	TEST_CLASS(ArrayTests)
	{
	public:
		TEST_METHOD_INITIALIZE(MethodInit)
		{
			ResetCounts();
		}

		TEST_METHOD(TestEmpty)
		{
			{
				Array<Element> arr;
			}
			Assert::AreEqual(ConstructCount, 0, nullptr, LINE_INFO());
			Assert::AreEqual(DestructCount, 0, nullptr, LINE_INFO());
		}

		TEST_METHOD(TestInitializerList)
		{
			{
				Array<Element> arr{ Element(17) };
				Assert::AreEqual(1, ConstructCount, nullptr, LINE_INFO());
				Assert::AreEqual(1, CopyCount, nullptr, LINE_INFO());
				Assert::AreEqual(1, DestructCount, nullptr, LINE_INFO());
				Assert::AreEqual((size_t)1, arr.Num(), nullptr, LINE_INFO());
				Assert::AreEqual(17, arr[0].data, nullptr, LINE_INFO());
			}

			Assert::AreEqual(1, ConstructCount, nullptr, LINE_INFO());
			Assert::AreEqual(2, DestructCount, nullptr, LINE_INFO());
		}

		TEST_METHOD(TestAdd)
		{
			auto e = Element();
			{
				Assert::AreEqual(1, ConstructCount, nullptr, LINE_INFO());
				Assert::AreEqual(0, CopyCount, nullptr, LINE_INFO());
				Assert::AreEqual(0, MoveCount, nullptr, LINE_INFO());

				ResetCounts();
				Array<Element> arr;
				arr.Reserve(10);
				Assert::AreEqual((size_t)0, arr.Add(e), nullptr, LINE_INFO());
				Assert::AreEqual(0, ConstructCount, nullptr, LINE_INFO());
				Assert::AreEqual(1, CopyCount, nullptr, LINE_INFO());
				Assert::AreEqual(0, MoveCount, nullptr, LINE_INFO());

				ResetCounts();
				Assert::AreEqual((size_t)1, arr.Add(Element()), nullptr, LINE_INFO());
				Assert::AreEqual(1, ConstructCount, nullptr, LINE_INFO());
				Assert::AreEqual(0, CopyCount, nullptr, LINE_INFO());
				Assert::AreEqual(1, MoveCount, nullptr, LINE_INFO());

				Assert::AreEqual((size_t)2, arr.Num(), nullptr, LINE_INFO());
			}

			Assert::AreEqual(3, DestructCount, nullptr, LINE_INFO());
		}
	};
}