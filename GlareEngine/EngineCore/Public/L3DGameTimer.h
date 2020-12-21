#pragma once

class GameTimer
{
public:
	GameTimer();

	float TotalTime()const; // in seconds
	float DeltaTime()const; // in seconds

	void Reset(); // ����Ϣѭ��֮ǰ����
	void Start(); // ȡ����ͣʱ����
	void Stop();  // ��ͣʱ����
	void Tick();  // ÿ֡����

private:
	double mSecondsPerCount;
	double mDeltaTime;

	__int64 mBaseTime;
	__int64 mPausedTime;
	__int64 mStopTime;
	__int64 mPrevTime;
	__int64 mCurrTime;

	bool mStopped;
};