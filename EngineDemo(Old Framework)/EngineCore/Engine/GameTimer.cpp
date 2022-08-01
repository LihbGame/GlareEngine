#include <windows.h>
#include "GameTimer.h"


double GameTimer::mSecondsPerCount=0.0f;
double GameTimer::mDeltaTime=-1.0f;
__int64 GameTimer::mBaseTime=0;
__int64 GameTimer::mPausedTime=0;
__int64 GameTimer::mStopTime=0;
__int64 GameTimer::mPrevTime=0;
__int64 GameTimer::mCurrTime=0;
bool GameTimer::mStopped=false;


GameTimer::GameTimer()
{

}

//返回自调用Reset()以来经过的总时间，不计算时钟停止的任何时间。
float GameTimer::TotalTime()
{
	// 如果我们停止了，不要计算自我们停止以来已经过去的时间。
	//此外，如果我们之前已经暂停，则距离mStopTime - mBaseTime包括暂停时间，
	//我们不想计算。 要纠正这个问题，我们可以从mStopTime中减去暂停时间：
	//
	//                     |<--paused time-->|
	// ----*---------------*-----------------*------------*------------*------> time
	//  mBaseTime       mStopTime        startTime     mStopTime    mCurrTime

	if( mStopped )
	{
		return (float)(((mStopTime - mPausedTime)-mBaseTime)*mSecondsPerCount);
	}

	//距离mCurrTime - mBaseTime包括暂停时间，我们不想计算。
	//要纠正这个问题，我们可以从mCurrTime中减去暂停时间：
	//
	//  (mCurrTime - mPausedTime) - mBaseTime 
	//
	//                     |<--paused time-->|
	// ----*---------------*-----------------*------------*------> time
	//  mBaseTime       mStopTime        startTime     mCurrTime
	
	else
	{
		return (float)(((mCurrTime-mPausedTime)-mBaseTime)*mSecondsPerCount);
	}
}

float GameTimer::DeltaTime()
{
	return (float)mDeltaTime;
}

void GameTimer::Reset()
{
	__int64 countsPerSec;
	QueryPerformanceFrequency((LARGE_INTEGER*)&countsPerSec);
	mSecondsPerCount = 1.0 / (double)countsPerSec;


	__int64 currTime;
	QueryPerformanceCounter((LARGE_INTEGER*)&currTime);

	mBaseTime = currTime;
	mPrevTime = currTime;
	mStopTime = 0;
	mStopped  = false;
}

void GameTimer::Start()
{
	__int64 startTime;
	QueryPerformanceCounter((LARGE_INTEGER*)&startTime);


	// 累计停止和启动对之间经过的时间。
	//
	//                     |<-------d------->|
	// ----*---------------*-----------------*------------> time
	//  mBaseTime       mStopTime        startTime     

	if( mStopped )
	{
		mPausedTime += (startTime - mStopTime);	

		mPrevTime = startTime;
		mStopTime = 0;
		mStopped  = false;
	}
}

void GameTimer::Stop()
{
	if( !mStopped )
	{
		__int64 currTime;
		QueryPerformanceCounter((LARGE_INTEGER*)&currTime);

		mStopTime = currTime;
		mStopped  = true;
	}
}

void GameTimer::Tick()
{
	if( mStopped )
	{
		mDeltaTime = 0.0;
		return;
	}

	__int64 currTime;
	QueryPerformanceCounter((LARGE_INTEGER*)&currTime);
	mCurrTime = currTime;

	//此帧与前一帧之间的时差
	mDeltaTime = (mCurrTime - mPrevTime)*mSecondsPerCount;

	// 准备下一帧
	mPrevTime = mCurrTime;

	//强制非负。 DXSDK的CDXUTTimer提到，
	//如果处理器进入省电模式或者我们被拖到另一个处理器，
	//那么mDeltaTime可能是负数。
	if(mDeltaTime < 0.0)
	{
		mDeltaTime = 0.0;
	}
}

