//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

#pragma once

//shared header for utility

//unreal
#include "CoreMinimal.h"

//module

// general declarations

/// <summary>
/// rate limiter operation tracking
/// </summary>
struct FRateLimiterOp
{
	int ID;
	float Time;
};

/// <summary>
/// Rate limiter for an async multichannel operation to help balance load
/// and keep interactive property edits as smooth and consistent as possible
/// </summary>
struct FRateLimiter
{
	/// <summary>
	/// setup with certain number of averaging stages
	/// NOTE: typically set this to 2x the number of synth threads
	/// </summary>
	/// <param name="stages"></param>
	/// <param name="initial_period">assume this was the update period before</param>
	void Init( int stages, float initial_period=0.1f );

	/// <summary>
	/// Inform of the start of a slow async operation
	/// </summary>
	/// <param name="id">identifier for matching operations together</param>
	void Begin( int id );

	/// <summary>
	/// Inform of the end of a slow async operation
	/// </summary>
	/// <param name="id"></param>
	void End( int id );

	/// <summary>
	/// see if it's ok to begin an asyn operation, or defer it 'til later (see CheckDispatch)
	/// </summary>
	/// <returns>true if should defer the operation</returns>
	bool CheckDefer() const;

	/// <summary>
	/// Check to see if any pending delayed async operations should now dispatch
	/// </summary>
	/// <returns>true if any pending operations should be dispatched</returns>
	bool CheckDispatch( float delta_time );

private:

	/// <summary>
	/// holding list for operation in progress that are being timed
	/// </summary>
	TArray<FRateLimiterOp> PendingOperations;

	/// <summary>
	/// defer completion until next frame so generation side effects on frame rate are included in timing
	/// NOTE: chain of lists, to defer enough
	/// </summary>
	TArray<int> CompletedOperations;
	TArray<int> CompletedOperations2;

	/// <summary>
	/// ring buffer of completed operation times
	/// </summary>
	TArray<float> OperationHistory;

	/// <summary>
	/// where in the history ring buffer to put the next time
	/// </summary>
	int HistoryCursor;

	/// <summary>
	/// record of frames ticking
	/// </summary>
	int Ticks;

	/// <summary>
	/// current request rate, throughput control based on average operation duration
	/// </summary>
	float Period;

	/// <summary>
	/// how many operations can run simultaneously
	/// </summary>
	int AsyncChannels;

	/// <summary>
	/// time since last request was allowed
	/// </summary>
	float Timer;


	/// <summary>
	/// actual end handling
	/// </summary>
	/// <param name="id"></param>
	void DeferredEnd( int id );
};
