//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

#pragma once


// Apparance API
#include "Apparance.h"


// Adaptor to routes apparance logging to Unreal logging system
//
struct FLoggingService : public Apparance::Host::ILoggingService
{
	// apparance

public:
	FLoggingService();

	//~ Begin Apparance ILoggingService Interface
	virtual void LogMessage( const char* pszmessage ) override;
	virtual void LogWarning( const char* pszwarning ) override;
	virtual void LogError( const char* pszerror ) override;
	//~ End Apparance ILoggingService Interface

	

};

