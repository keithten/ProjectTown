//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

//module
#include "Utility/ApparanceConversion.h"

//unreal
#include "GameFramework/Actor.h"

// apparance space from unreal space, frame to frame conversion
//
void ApparanceFrameFromUnrealWorldspaceFrame( Apparance::Frame& frame )
{
	//origin
	frame.Origin = VECTOR3_APPARANCESCALE_FROM_UNREALSCALE( frame.Origin );
	frame.Origin = VECTOR3_APPARANCEHANDEDNESS_FROM_UNREALHANDEDNESS( frame.Origin );

	//orientation handedness (apparance handedness axes IN apparance space)
	APPARANCE_VALUE_SWAP( frame.Orientation.X, frame.Orientation.Y );
	frame.Orientation.X = VECTOR3_UNREALHANDEDNESS_FROM_APPARANCEHANDEDNESS( frame.Orientation.X );
	frame.Orientation.Y = VECTOR3_UNREALHANDEDNESS_FROM_APPARANCEHANDEDNESS( frame.Orientation.Y );
	frame.Orientation.Z = VECTOR3_UNREALHANDEDNESS_FROM_APPARANCEHANDEDNESS( frame.Orientation.Z );

	//size scale
	frame.Size = VECTOR3_APPARANCESCALE_FROM_UNREALSCALE( frame.Size );
	frame.Size = VECTOR3_APPARANCEHANDEDNESS_FROM_UNREALHANDEDNESS( frame.Size );
}

// unreal space from apparance space, frame to frame conversion
//
void UnrealWorldspaceFrameFromApparanceFrame( Apparance::Frame& frame )
{
	//origin
	frame.Origin = VECTOR3_UNREALSCALE_FROM_APPARANCESCALE( frame.Origin );
	frame.Origin = VECTOR3_UNREALHANDEDNESS_FROM_APPARANCEHANDEDNESS( frame.Origin );

	//orientation handedness (unreal handedness axes IN unreal space)
	APPARANCE_VALUE_SWAP(frame.Orientation.X, frame.Orientation.Y);
	frame.Orientation.X = VECTOR3_UNREALHANDEDNESS_FROM_APPARANCEHANDEDNESS( frame.Orientation.X );
	frame.Orientation.Y = VECTOR3_UNREALHANDEDNESS_FROM_APPARANCEHANDEDNESS( frame.Orientation.Y );
	frame.Orientation.Z = VECTOR3_UNREALHANDEDNESS_FROM_APPARANCEHANDEDNESS( frame.Orientation.Z );

	//size scale
	frame.Size = VECTOR3_UNREALSCALE_FROM_APPARANCESCALE( frame.Size );
	frame.Size = VECTOR3_UNREALHANDEDNESS_FROM_APPARANCEHANDEDNESS( frame.Size );
}

// frame to matrix conversion, optional actor frame of reference and optional output forms (full, rotation)
//
void UnrealTransformsFromApparanceFrame(const Apparance::Frame& in_frame, const AActor* pframe_of_reference, FMatrix* full_transform_out, FMatrix* rotation_transform_out, FVector* offset_out, FVector* scale_out, bool is_worldspace, EApparanceFrameOrigin frame_origin )
{
	//space mapping
	Apparance::Frame frame = in_frame;
	if(!is_worldspace)
	{
		//convert from apparance space to unreal space
		UnrealWorldspaceFrameFromApparanceFrame(frame);
	}
	ApparanceFrameOriginAdjust( frame, frame_origin );

	//type conversion
	FVector local_pos = UNREALVECTOR_FROM_APPARANCEVECTOR3( frame.Origin );
	FVector local_size = UNREALVECTOR_FROM_APPARANCEVECTOR3( frame.Size );
	FVector local_axisx = UNREALVECTOR_FROM_APPARANCEVECTOR3( frame.Orientation.X );
	FVector local_axisy = UNREALVECTOR_FROM_APPARANCEVECTOR3( frame.Orientation.Y );
	FVector local_axisz = UNREALVECTOR_FROM_APPARANCEVECTOR3( frame.Orientation.Z );
	
	//actor local space transform
	FScaleMatrix local_scaling(local_size);
	FMatrix local_rotation = FRotationMatrix::MakeFromXY(local_axisx, local_axisy);
	local_pos += local_rotation.TransformVector( local_size*0.5f );
	FTranslationMatrix local_translation( local_pos );
	
	//world space context?
	FMatrix world_transform;
	if (pframe_of_reference)
	{
		world_transform = pframe_of_reference->GetTransform().ToMatrixWithScale();
	}

	//want transform?
	if (full_transform_out)
	{
		if (pframe_of_reference)
		{
			//actor world space tranform
			*full_transform_out = local_scaling * local_rotation * local_translation * world_transform;
		}
		else
		{ 
			//local transform only
			*full_transform_out = local_scaling * local_rotation * local_translation;
		}
	}

	//want rotation?
	if (rotation_transform_out)
	{
		if (pframe_of_reference)
		{
			//actor world space rotation
			FRotationMatrix world_rotation = FRotationMatrix(pframe_of_reference->GetActorRotation());
			*rotation_transform_out = local_rotation * world_rotation;
		}
		else
		{
			//local rotation only
			*rotation_transform_out = local_rotation;
		}
	}
	
	//want offset?
	if (offset_out)
	{
		//TODO: proper offset conversion
		*offset_out = local_pos;
	}

	if (scale_out)
	{
		//TODO: proper size conversion
		*scale_out = local_size;
	}
}

// colour conversion helper (so only need to construct FLinearColor once in macro expression)
//
Apparance::Colour ApparanceFColourFromUnrealLinearColor( FLinearColor c )
{
	return Apparance::Colour( c.R, c.G, c.B, c.A );
}

// find a displayable name for an Apparance type
//
const TCHAR* ApparanceParameterTypeName(Apparance::Parameter::Type type)
{
	switch (type)
	{
		case Apparance::Parameter::Integer: return TEXT("Integer");
		case Apparance::Parameter::Float:   return TEXT("Float");
		case Apparance::Parameter::Bool:    return TEXT("Bool");
		case Apparance::Parameter::Frame:   return TEXT("Frame");
		case Apparance::Parameter::Vector2: return TEXT("Vector2");
		case Apparance::Parameter::Vector3: return TEXT("Vector3");
		case Apparance::Parameter::Vector4: return TEXT("Vector4");
		case Apparance::Parameter::Colour:  return TEXT("Colour");
		case Apparance::Parameter::String:  return TEXT("String");
		case Apparance::Parameter::List:    return TEXT("List");
		default: return TEXT("Unknown");
	}
}

// is an Apparance type generally numeric
//
bool ApparanceParameterTypeIsNumeric(Apparance::Parameter::Type type)
{
	switch (type)
	{
		case Apparance::Parameter::Integer: return true;
		case Apparance::Parameter::Float:   return true;
		case Apparance::Parameter::Vector2: return true;
		case Apparance::Parameter::Vector3: return true;
		case Apparance::Parameter::Vector4: return true;
		default: return false;
	}
}

// is an Apparance type potentially spatial?
//
bool ApparanceParameterTypeIsSpatial(Apparance::Parameter::Type type)
{
	switch (type)
	{
		case Apparance::Parameter::Integer: return true;
		case Apparance::Parameter::Float:   return true;
		case Apparance::Parameter::Frame:   return true;
		case Apparance::Parameter::Vector2: return true;
		case Apparance::Parameter::Vector3: return true;
		case Apparance::Parameter::Vector4: return true;
		default: return false;
	}
}

// helper to apply a scale to an apparance vector type
//
Apparance::Vector3 ScaleApparanceVector3( const Apparance::Vector3& v, float scale )
{
	return Apparance::Vector3( v.X * scale, v.Y * scale, v.Z * scale );
}

// helper to add two apparance vectors (a+b)
//
Apparance::Vector3 AddApparanceVector3( const Apparance::Vector3& a, const Apparance::Vector3& b )
{
	return Apparance::Vector3( a.X + b.X, a.Y + b.Y, a.Z + b.Z );
}

// helper to subtract two apparance vectors (a-b)
//
Apparance::Vector3 SubtractApparanceVector3( const Apparance::Vector3& a, const Apparance::Vector3& b )
{
	return Apparance::Vector3( a.X - b.X, a.Y - b.Y, a.Z - b.Z );
}

// origin metadata controls how the frame is placed
// * Corner (0) - unaffected, origin matches natural frame origin
// * Face (1) - origin is treated as centre of lower face (good for objects sitting on the floor)
// * Centre (2) - origin is treated as centre of frame in all axes (free floating generally orientable objects)
//
Apparance::Vector3 ApparanceFrameOriginAdjust( Apparance::Frame& frame, EApparanceFrameOrigin origin, bool reverse )
{
	//accumulate required adjustment
	Apparance::Vector3 adjustment;

	const float amount = reverse?0.5f:-0.5f; //move fore/back a half size
	switch(origin)
	{
		case EApparanceFrameOrigin::Corner:
			//as-is, do nothing
			break;
		case EApparanceFrameOrigin::Face:
		{
			//lower centre, move XY back by half size
			adjustment = AddApparanceVector3( adjustment, ScaleApparanceVector3( frame.Orientation.X, frame.Size.X * amount ) );
			adjustment = AddApparanceVector3( adjustment, ScaleApparanceVector3( frame.Orientation.Y, frame.Size.Y * amount ) );
			break;
		}
		case EApparanceFrameOrigin::Centre:
		{
			//complete centre, move XYZ back by half size
			adjustment = AddApparanceVector3( adjustment, ScaleApparanceVector3( frame.Orientation.X, frame.Size.X * amount ) );
			adjustment = AddApparanceVector3( adjustment, ScaleApparanceVector3( frame.Orientation.Y, frame.Size.Y * amount ) );
			adjustment = AddApparanceVector3( adjustment, ScaleApparanceVector3( frame.Orientation.Z, frame.Size.Z * amount ) );
			break;
		}
	}

	//apply
	frame.Origin = AddApparanceVector3( frame.Origin, adjustment );
	return adjustment;
}

// adjust the size of a frame, maintaining centering about the specified location (corner/face/centre)
//
void ApparanceFrameSetSize(Apparance::Frame& frame, Apparance::Vector3 new_size, EApparanceFrameOrigin scaling_centre)
{
	//remove origin offset
	ApparanceFrameOriginAdjust(frame, scaling_centre, true);

	//apply new size
	frame.Size = new_size;

	//re-apply origin offset
	ApparanceFrameOriginAdjust(frame, scaling_centre, false);
}
