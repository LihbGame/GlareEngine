#pragma once

class GameTimer
{
public:
	GameTimer();

	static float TotalTime(); // in seconds
	static float DeltaTime(); // in seconds

	static void Reset(); // ����Ϣѭ��֮ǰ����
	static void Start(); // ȡ����ͣʱ����
	static void Stop();  // ��ͣʱ����
	static void Tick();  // ÿ֡����

private:
	static double mSecondsPerCount;
	static double mDeltaTime;

	static __int64 mBaseTime;
	static __int64 mPausedTime;
	static __int64 mStopTime;
	static __int64 mPrevTime;
	static __int64 mCurrTime;

	static bool mStopped;
};
