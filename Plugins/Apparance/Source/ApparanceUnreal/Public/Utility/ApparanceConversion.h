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

// Unreal
#include "CoreTypes.h"
#include "UObject/ObjectMacros.h"

// origin options for frame use
//
UENUM()
enum class EApparanceFrameOrigin : uint8
{
	Face,		//lower Z, centre XY (default, 0)
	Corner,		//lower XYZ
	Centre,		//centre XYZ
};

//FVector = Unreal
//Vector3 = Apparance

//type
#define APPARANCEVECTOR3_FROM_UNREALVECTOR( a ) (Apparance::Vector3( (a).X, (a).Y, (a).Z ))
#define UNREALVECTOR_FROM_APPARANCEVECTOR3( a ) (FVector( (a).X, (a).Y, (a).Z ))
//scale
#define UNREALSCALE_FROM_APPARANCESCALE( x ) ((x)*100.0f)
#define UNREALSCALE_FROM_APPARANCESCALE3( a ) (FVector( UNREALSCALE_FROM_APPARANCESCALE((a).X), UNREALSCALE_FROM_APPARANCESCALE((a).Y), UNREALSCALE_FROM_APPARANCESCALE((a).Z)))
#define FVECTOR_UNREALSCALE_FROM_APPARANCESCALE( a ) (FVector( UNREALSCALE_FROM_APPARANCESCALE((a).X), UNREALSCALE_FROM_APPARANCESCALE((a).Y), UNREALSCALE_FROM_APPARANCESCALE((a).Z)))
#define VECTOR3_UNREALSCALE_FROM_APPARANCESCALE( a ) (Apparance::Vector3( UNREALSCALE_FROM_APPARANCESCALE((a).X), UNREALSCALE_FROM_APPARANCESCALE((a).Y), UNREALSCALE_FROM_APPARANCESCALE((a).Z)))
#define APPARANCESCALE_FROM_UNREALSCALE( x ) ((x)*0.01f)
#define APPARANCESCALE_FROM_UNREALSCALE3( u ) (Apparance::Vector3( APPARANCESCALE_FROM_UNREALSCALE((u).X), APPARANCESCALE_FROM_UNREALSCALE((u).Y), APPARANCESCALE_FROM_UNREALSCALE((u).Z)))
#define FVECTOR_APPARANCESCALE_FROM_UNREALSCALE( u ) (FVector( APPARANCESCALE_FROM_UNREALSCALE((u).X), APPARANCESCALE_FROM_UNREALSCALE((u).Y), APPARANCESCALE_FROM_UNREALSCALE((u).Z)))
#define VECTOR3_APPARANCESCALE_FROM_UNREALSCALE( u ) (Apparance::Vector3( APPARANCESCALE_FROM_UNREALSCALE((u).X), APPARANCESCALE_FROM_UNREALSCALE((u).Y), APPARANCESCALE_FROM_UNREALSCALE((u).Z)))
//handedness
#define UNREALHANDEDNESS_FROM_APPARANCEHANDEDNESS( a ) (FVector( (a).Y, (a).X, (a).Z ))	//swap XY
#define APPARANCEHANDEDNESS_FROM_UNREALHANDEDNESS( a ) (Apparance::Vector3( (a).Y, (a).X, (a).Z ))	//swap XY
#define FVECTOR_UNREALHANDEDNESS_FROM_APPARANCEHANDEDNESS( a ) (FVector( (a).Y, (a).X, (a).Z ))	//swap XY
#define FVECTOR_APPARANCEHANDEDNESS_FROM_UNREALHANDEDNESS( a ) (FVector( (a).Y, (a).X, (a).Z ))	//swap XY back
#define VECTOR3_UNREALHANDEDNESS_FROM_APPARANCEHANDEDNESS( a ) (Apparance::Vector3( (a).Y, (a).X, (a).Z ))	//swap XY
#define VECTOR3_APPARANCEHANDEDNESS_FROM_UNREALHANDEDNESS( a ) (Apparance::Vector3( (a).Y, (a).X, (a).Z ))	//swap XY back
template<typename T> void APPARANCE_VALUE_SWAP(T& a, T& b) { T t = a; a = b; b = t; }
//transform
#define APPARANCESPACE_FROM_UNREALSPACE( a ) (APPARANCEVECTOR3_FROM_UNREALVECTOR( FVECTOR_APPARANCEHANDEDNESS_FROM_UNREALHANDEDNESS( VECTOR3_APPARANCESCALE_FROM_UNREALSCALE( a ) ) ) )
#define VECTOR3_APPARANCESPACE_FROM_UNREALSPACE( a ) (VECTOR3_APPARANCEHANDEDNESS_FROM_UNREALHANDEDNESS( VECTOR3_APPARANCESCALE_FROM_UNREALSCALE( a ) ) )
#define FVECTOR_APPARANCESPACE_FROM_UNREALSPACE( a ) (FVECTOR_APPARANCEHANDEDNESS_FROM_UNREALHANDEDNESS( FVECTOR_APPARANCESCALE_FROM_UNREALSCALE( a ) ) )
#define UNREALSPACE_FROM_APPARANCESPACE( a ) (FVECTOR_UNREALSCALE_FROM_APPARANCESCALE( FVECTOR_UNREALHANDEDNESS_FROM_APPARANCEHANDEDNESS( UNREALVECTOR_FROM_APPARANCEVECTOR3( a ) ) ) )
#define VECTOR3_UNREALSPACE_FROM_APPARANCESPACE( a ) (VECTOR3_UNREALSCALE_FROM_APPARANCESCALE( VECTOR3_UNREALHANDEDNESS_FROM_APPARANCEHANDEDNESS( a ) ) )
#define FVECTOR_UNREALSPACE_FROM_APPARANCESPACE( a ) (FVECTOR_UNREALSCALE_FROM_APPARANCESCALE( FVECTOR_UNREALHANDEDNESS_FROM_APPARANCEHANDEDNESS( a ) ) )
//colour
#define UNREALLINEARCOLOR_FROM_APPARANCECOLOUR( c ) (FLinearColor( c.R, c.G, c.B, c.A ))
#define UNREALCOLOR_FROM_APPARANCECOLOUR( c ) (FLinearColor( c.R, c.G, c.B, c.A ).ToFColor( false ))
#define UNREALSRGB_FROM_APPARANCECOLOUR( c ) (FLinearColor( c.R, c.G, c.B, c.A ).ToFColor( true ))
#define APPARANCECOLOUR_FROM_UNREALLINEARCOLOR( c ) (Apparance::Colour( c.R, c.G, c.B, c.A ))
#define APPARANCECOLOUR_FROM_UNREALCOLOR( c ) ( ApparanceFColourFromUnrealLinearColor( c.ReinterpretAsLinear() ) )	//linear
#define APPARANCECOLOUR_FROM_UNREALSRGB( c ) ( ApparanceFColourFromUnrealLinearColor( FLinearColor(c) ) )			//srgb space


// apparance space from unreal space, frame to frame conversion
//
APPARANCEUNREAL_API void ApparanceFrameFromUnrealWorldspaceFrame( Apparance::Frame& frame );

// unreal space from apparance space, frame to frame conversion
//
APPARANCEUNREAL_API void UnrealWorldspaceFrameFromApparanceFrame( Apparance::Frame& frame );

// frame to matrix conversion, optional actor frame of reference and optional output forms (full, rotation)
//
APPARANCEUNREAL_API void UnrealTransformsFromApparanceFrame(
	const Apparance::Frame& frame, 
	const class AActor* pframe_of_reference=nullptr, 
	FMatrix* full_transform_out = nullptr, 
	FMatrix* rotation_transform_out = nullptr, 
	FVector* offset_out=nullptr, 
	FVector* scale_out=nullptr, 
	bool is_worldspace=false, 
	EApparanceFrameOrigin frame_origin=EApparanceFrameOrigin::Corner );

// colour conversion helper (so only need to construct FLinearColor once in macro expression)
//
APPARANCEUNREAL_API Apparance::Colour ApparanceFColourFromUnrealLinearColor( FLinearColor c );

// find a displayable name for an Apparance type
//
APPARANCEUNREAL_API const TCHAR* ApparanceParameterTypeName(Apparance::Parameter::Type type);

// is an Apparance type potentially numeric
//
APPARANCEUNREAL_API bool ApparanceParameterTypeIsNumeric(Apparance::Parameter::Type type);

// is an Apparance type potentially spatial?
//
APPARANCEUNREAL_API bool ApparanceParameterTypeIsSpatial(Apparance::Parameter::Type type);

// adjust frame origin (corner, face, centre)
// NOTE: moves the frame so that the requested location is where the origin used to be
// NOTE: if 'reverse' is true then it moves the frame so that the origin is where the requested location used to be
// Returns applied origin adjustment that has been applied
//
APPARANCEUNREAL_API Apparance::Vector3 ApparanceFrameOriginAdjust( Apparance::Frame& frame, EApparanceFrameOrigin origin, bool reverse=false );

// adjust the size of a frame, maintaining centering about the specified location (corner/face/centre)
//
APPARANCEUNREAL_API void ApparanceFrameSetSize( Apparance::Frame& frame, Apparance::Vector3 new_size, EApparanceFrameOrigin scaling_centre=EApparanceFrameOrigin::Centre );

// helper to apply a scale to an apparance vector type
//
APPARANCEUNREAL_API Apparance::Vector3 ScaleApparanceVector3( const Apparance::Vector3& v, float scale );

// helper to add two apparance vectors (a+b)
//
APPARANCEUNREAL_API Apparance::Vector3 AddApparanceVector3( const Apparance::Vector3& a, const Apparance::Vector3& b );

// helper to add subtract apparance vectors (a-b)
//
APPARANCEUNREAL_API Apparance::Vector3 SubtractApparanceVector3( const Apparance::Vector3& a, const Apparance::Vector3& b );
