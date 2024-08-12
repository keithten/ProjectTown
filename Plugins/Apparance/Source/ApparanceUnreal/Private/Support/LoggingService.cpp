//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

// main
#include "LoggingService.h"

// unreal

// module
#include "ApparanceUnreal.h"



//////////////////////////////////////////////////////////////////////////
// FLoggingService

FLoggingService::FLoggingService()
{
}

void FLoggingService::LogMessage( const char* pszmessage )
{
	FUTF8ToTCHAR ws( pszmessage );
	FString s( ws.Get() );
	s.TrimEndInline();
	UE_LOG( LogApparance, Display, TEXT("%s"), *s );
}

void FLoggingService::LogWarning( const char* pszwarning )
{
	FUTF8ToTCHAR ws( pszwarning );
	FString s( ws.Get() );
	s.TrimEndInline();
	UE_LOG( LogApparance, Display, TEXT("%s"), *s );
}

void FLoggingService::LogError( const char* pszerror )
{
	FUTF8ToTCHAR ws( pszerror );
	FString s( ws.Get() );
	s.TrimEndInline();
	UE_LOG( LogApparance, Display, TEXT("%s"), *s );
}
