#pragma once

namespace DX12Lib
{
	class Timer
	{
	public:
		Timer();

		void Reset();
		void Tick();

		inline double DeltaTime() const { return mDeltaTime; }
		inline double TotalTime() const { return mTotalTime; }

	private:
		double mDeltaTime = 0.0;
		double mTotalTime = 0.0;

		double mSecondsPerCount;
		__int64 mCurrTime = 0;
	};
}
