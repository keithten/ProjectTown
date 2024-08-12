//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

#pragma once

//unreal
#include "Misc/EngineVersionComparison.h"

//utility
#define UE_VERSION_AT_LEAST(a,b,c) (!UE_VERSION_OLDER_THAN(a,b,c))

//ticker changes
#if UE_VERSION_AT_LEAST(5,0,0)
#define TICKER_HANDLE FTSTicker::FDelegateHandle
#define TICKER_TYPE FTSTicker
#else //older, pre-5.0
#define TICKER_HANDLE FDelegateHandle
#define TICKER_TYPE FTicker
extern bool IsValid( UObject* p );
extern bool IsValidChecked( UObject* p );
#endif

//string cast changes
#if UE_VERSION_AT_LEAST(5,0,0)
#define APPARANCE_UTF8_TO_TCHAR(s) StringCast<TCHAR>( s ).Get()
#else
#define APPARANCE_UTF8_TO_TCHAR(s) UTF8_TO_TCHAR( (const ANSICHAR*)(s) )
#endif