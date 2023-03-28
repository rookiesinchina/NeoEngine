#include "GameTimer.h"

void GameTimer::Tick()
{
    if (mStopped)
    {
        mDeltaTime = 0.0;
        return;
    }

    __int64 currenttime;
    QueryPerformanceCounter((LARGE_INTEGER*)&currenttime);

    mCurrTime = currenttime;
    mDeltaTime = (currenttime - mPrevTime) * mSecondsPerCount;
    mPrevTime = currenttime;

    if (mDeltaTime < 0.0)
    {
        mDeltaTime = 0.0;
    }
}

void GameTimer::Stop()
{
    if (mStopped)
        return;

    __int64 currenttime;
    QueryPerformanceCounter((LARGE_INTEGER*)&currenttime);

    mStopTime = currenttime;
    mStopped = true;
}

void GameTimer::Start()
{
    if (!mStopped)
        return;

    __int64 currenttime;
    QueryPerformanceCounter((LARGE_INTEGER*)&currenttime);

    mPrevTime = currenttime;
    mPausedTime += currenttime - mStopTime;
    mStopped = false;
}

double GameTimer::DeltaTime()const
{
    return mDeltaTime;
}

double GameTimer::TotalTime()const
{
    if (mStopped)
    {
        return (mStopTime - mPausedTime - mBaseTime) * mSecondsPerCount;
    }

    return (mCurrTime - mPausedTime - mBaseTime) * mSecondsPerCount;
}

void GameTimer::Reset()
{
    __int64 currenttime;
    QueryPerformanceCounter((LARGE_INTEGER*)&currenttime);
    
    mBaseTime = currenttime;
    mStopTime = 0;
    mCurrTime = currenttime;
    mPrevTime = currenttime;
    mPausedTime = 0;
    mDeltaTime = 0.0f;
    mStopTime = false;
}
