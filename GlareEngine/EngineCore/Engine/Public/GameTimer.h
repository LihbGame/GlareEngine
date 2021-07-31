#pragma once

class GameTimer
{
public:
	GameTimer();

	static float TotalTime(); // in seconds
	static float DeltaTime(); // in seconds

	static void Reset(); // 在消息循环之前调用
	static void Start(); // 取消暂停时调用
	static void Stop();  // 暂停时调用
	static void Tick();  // 每帧调用

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
