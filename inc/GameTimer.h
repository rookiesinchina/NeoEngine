#pragma once
#include <wrl.h>

class GameTimer
{
private:
    double mSecondsPerCount = 0.0;
    double mDeltaTime = 0.0;

    __int64 mBaseTime = 0;
    __int64 mCurrTime = 0;
    __int64 mPrevTime = 0;
    __int64 mStopTime = 0;
    __int64 mPausedTime = 0;
    bool mStopped = false;
public:
    GameTimer()
    {
        __int64 CountPerSecond;
        QueryPerformanceFrequency((LARGE_INTEGER*)&CountPerSecond);
        mSecondsPerCount = 1.0f / CountPerSecond;
    }
    ~GameTimer() {};
    // This function should be invoked before rendering
    void Tick();
    // When the app is paused,the timer should also be paused 
    void Stop();
    // Restore timer from paused
    void Start();
    // Interval time between every tick.
    double DeltaTime()const;
    // Total time since the app run
    double TotalTime()const;
    // Reset timer
    void Reset();
};