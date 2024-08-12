//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

#pragma once

//unreal
#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

//module


/// <summary>
/// manage the editor CPU throttling setting when we need a display refresh boost
/// </summary>
class FThrottleOverride
{
	//settings control
	bool bConfigThrottle;

	//current state
	bool bCurrentThrottle;

	//boost active for this long
	float fBoostTimer;

public:
	FThrottleOverride()
		: bConfigThrottle(false)
		, bCurrentThrottle(false)
		, fBoostTimer(0)
	{
	}

	/// <summary>
	/// set up
	/// </summary>
	void Init();
	void Shutdown() {}
	
	/// <summary>
	/// regular updates required
	/// </summary>
	void Tick(float deltatime);

	/// <summary>
	/// request editor perf/responsiveness boost
	/// </summary>
	/// <param name="duration"></param>
	void Boost(float duration = 1.0f);

private:
	void Apply(bool allow_boost);
};
