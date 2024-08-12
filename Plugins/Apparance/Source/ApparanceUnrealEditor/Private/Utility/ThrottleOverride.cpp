//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

//main
#include "ThrottleOverride.h"

//unreal
//#include "Core.h"
#include "Editor.h"
#include "Editor/EditorPerformanceSettings.h"

//module


/// <summary>
/// set up
/// </summary>
void FThrottleOverride::Init()
{
	//start based on config settings
	const UEditorPerformanceSettings* Settings = GetDefault<UEditorPerformanceSettings>();
	bConfigThrottle = Settings->bThrottleCPUWhenNotForeground;
	bCurrentThrottle = bConfigThrottle;
}

/// <summary>
/// regular updates required
/// </summary>
void FThrottleOverride::Tick(float deltatime)
{
	//boost decay
	if (fBoostTimer > 0)
	{
		fBoostTimer -= deltatime;
		if (fBoostTimer <= 0)
		{
			//boost ended
			fBoostTimer = 0;
			Apply(true);
		}
	}

	//check for user change of settings
	const UEditorPerformanceSettings* Settings = GetDefault<UEditorPerformanceSettings>();
	bool config_check = Settings->bThrottleCPUWhenNotForeground;
	if (config_check != bCurrentThrottle)
	{
		//user has changed settings
		bConfigThrottle = config_check;

		Apply(true);
	}
}

/// <summary>
/// request editor perf/responsiveness boost
/// </summary>
/// <param name="duration"></param>
void FThrottleOverride::Boost(float duration)
{
	//UE_LOG(LogUtility, Display, TEXT("BOOST: Requesting a boost if %f seconds"), duration);
	fBoostTimer = duration;
	Apply(true);

	//also force view refresh
	FViewport* viewport = GEditor->GetActiveViewport();
	if (viewport)
	{
		viewport->InvalidateDisplay();
	}
}

/// <summary>
/// 
/// </summary>
/// <param name="allow_boost"></param>
void FThrottleOverride::Apply(bool allow_boost)
{
	bool set = false;
	bool throttle = false;

	if (allow_boost)
	{
		bool want_boost = fBoostTimer > 0;
		const UEditorPerformanceSettings* Settings = GetDefault<UEditorPerformanceSettings>();
		bool actual_throttle = Settings->bThrottleCPUWhenNotForeground;
		bool want_throttle = !want_boost;

		//user changed settings?
		if (actual_throttle != bCurrentThrottle)
		{
			//UE_LOG(LogUtility, Display, TEXT("THROTTLE: User changed throttle to %s"), actual_throttle ? TEXT("ON") : TEXT("OFF"));
			//track change
			bConfigThrottle = actual_throttle;
			bCurrentThrottle = actual_throttle;
		}

		//apply boost?
		if (want_boost && bCurrentThrottle)
		{
			//suppress throttle
			bCurrentThrottle = false;
			set = true;
			throttle = false;
			//UE_LOG(LogUtility, Display, TEXT("THROTTLE: Boost changed throttle to OFF"));
		}
		else if (!want_boost && !bCurrentThrottle)
		{
			//restore throttle
			bCurrentThrottle = bConfigThrottle;
			set = true;
			throttle = bConfigThrottle;
			//UE_LOG(LogUtility, Display, TEXT("THROTTLE: Boost restored throttle to %s"), throttle ? TEXT("ON") : TEXT("OFF"));
		}
	}
	else
	{
		//no boost, set back to user pref
		set = true;
		throttle = bConfigThrottle;
	}

	if (set)
	{
		UEditorPerformanceSettings* Settings = GetMutableDefault<UEditorPerformanceSettings>();
		Settings->bThrottleCPUWhenNotForeground = throttle;
	}
}


