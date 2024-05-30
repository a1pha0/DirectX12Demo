#include "DX12Lib/Timer.h"
#include <windows.h>

namespace DX12Lib
{
	Timer::Timer()
	{
		__int64 countsPerSec;
		QueryPerformanceFrequency((LARGE_INTEGER*)&countsPerSec);
		mSecondsPerCount = 1.0 / (double)countsPerSec;
	}

	void Timer::Reset()
	{
		mDeltaTime = 0.0;
		mTotalTime = 0.0;

		__int64 currTime;
		QueryPerformanceCounter((LARGE_INTEGER*)&currTime);
		mCurrTime = currTime;
	}

	void Timer::Tick()
	{
		__int64 currTime;
		QueryPerformanceCounter((LARGE_INTEGER*)&currTime);

		// Time difference between this frame and the previous.
		mDeltaTime = (currTime - mCurrTime) * mSecondsPerCount;
		mTotalTime += mDeltaTime;

		mCurrTime = currTime;
	}
}
