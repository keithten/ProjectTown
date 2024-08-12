//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

//main
#include "Utility/RateLimiter.h"

//unreal
//#include "Core.h"
//#include "Editor.h"
//#include "Editor/EditorPerformanceSettings.h"

//module
#include "ApparanceEngineSetup.h"

// general declarations

//log state
#define RATE_LIMITER_DEBUG 0

#if RATE_LIMITER_DEBUG
DECLARE_LOG_CATEGORY_EXTERN( LogApparance, Log, All );
#endif


///////////// RATE LIMITER ////////////


/// <summary>
/// setup with certain number of averaging stages
/// NOTE: typically set this to 2x the number of synth threads
/// </summary>
/// <param name="stages"></param>
/// <param name="initial_period">assume this was the update period before</param>
void FRateLimiter::Init( int stages, float initial_period )
{
	Ticks = 0;
	Timer = 0;
	HistoryCursor = 0;
	AsyncChannels = UApparanceEngineSetup::GetThreadCount();

	//pre-heat history with assumed initial period
	OperationHistory.AddUninitialized( stages );
	for(int i = 0; i < stages; i++)
	{
		OperationHistory[i] = initial_period;
	}

	//use history stage count as limit of tracking slots too
	PendingOperations.AddZeroed( stages );
}

/// <summary>
/// Inform of the start of a slow async operation
/// </summary>
/// <param name="id">identifier for matching operations together</param>
void FRateLimiter::Begin( int id )
{
#if RATE_LIMITER_DEBUG
	UE_LOG( LogApparance, Display, TEXT( "ratelimiter %p [%08i] BEGIN: %i" ), this, Ticks, id );
#endif
	
	//add op to tracking list
	int ioldest = 0;
	float longesttime = 0;
	for(int i = 0; i < PendingOperations.Num(); i++)
	{
		if(PendingOperations[i].Time > longesttime)
		{
			longesttime = PendingOperations[i].Time;
			ioldest = i;
		}
	}

	//track new op, reset timer
	PendingOperations[ioldest].ID = id;
	PendingOperations[ioldest].Time = 0;
	
	//reset timer for another period
	Timer = 0;
}

/// <summary>
/// Inform of the end of a slow async operation
/// </summary>
/// <param name="id"></param>
void FRateLimiter::End( int id )
{
	CompletedOperations.Add( id );
}

/// <summary>
/// actually handle completed operations
/// </summary>
/// <param name="id"></param>
void FRateLimiter::DeferredEnd( int id )
{
#if RATE_LIMITER_DEBUG
	UE_LOG( LogApparance, Display, TEXT( "ratelimiter %p [%08i] END: %i" ), this, Ticks, id );
#endif

	//find op
	int iop = -1;
	for(int i = 0; i < PendingOperations.Num(); i++)
	{
		if(PendingOperations[i].ID==id)
		{
			iop = i;
			break;
		}
	}

	//still tracking?
	if(iop != -1)
	{
		//calculate duration
		float duration = PendingOperations[iop].Time;

		//add completed op time to the history
		OperationHistory[HistoryCursor] = duration;
		HistoryCursor = (HistoryCursor + 1) % OperationHistory.Num();	//rotate ring

		//recalc average period
		float average = 0;
		for(int i = 0; i < OperationHistory.Num(); i++)
		{
			average += OperationHistory[i];
		}
		average /= (float)OperationHistory.Num();

		//use average as new period/rate but spread across all available channels
		Period = average / (float)AsyncChannels;

#if RATE_LIMITER_DEBUG
		//show history buffer
		FString s;
		for(int i = 0; i < OperationHistory.Num(); i++)
		{
			s.Appendf( TEXT( "%.3f " ), OperationHistory[i] );
		}
		UE_LOG( LogApparance, Display, TEXT( "  History: %s = %.3f" ), *s, Period );
#endif
	}
}

/// <summary>
/// see if it's ok to begin an async operation, or defer it 'til later (see CheckDispatch)
/// </summary>
/// <returns>true if should defer the operation</returns>
bool FRateLimiter::CheckDefer() const
{
	//wait out timer
	return Timer < Period;
}

/// <summary>
/// Check to see if any pending delayed async operations should now dispatch
/// </summary>
/// <returns>true if any pending operations should be dispatched</returns>
bool FRateLimiter::CheckDispatch( float delta_time )
{
	//endings (handle secondary list)
	if(CompletedOperations2.Num() > 0)
	{
		for(int i = 0; i < CompletedOperations2.Num(); i++)
		{
			DeferredEnd( CompletedOperations2[i] );
		}
		CompletedOperations2.Empty();
	}
	//endings (shuffle primary list to secondary)
	if(CompletedOperations.Num() > 0)
	{
		//move to secondary list
		CompletedOperations2.Append( CompletedOperations );
		CompletedOperations.Empty();
	}

	//tracking
	for(int i = 0; i < PendingOperations.Num(); i++)
	{
		PendingOperations[i].Time += delta_time;
	}

	//timer expired?
	bool expired = Timer>=Period;

	//timing
	Ticks++;
	Timer += delta_time;

	return expired;
}
