//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

#pragma once

//unreal
#include "Components/SceneComponent.h"


// auto (last)
#include "ApparanceRootComponent.generated.h"

// Apparance Root Component
// Custom scene component for apparance entity spatial root
//
UCLASS()
class APPARANCEUNREAL_API UApparanceRootComponent
	: public USceneComponent
{
	GENERATED_BODY()
public:

	UApparanceRootComponent()
	{
		bWantsOnUpdateTransform = true;
	}

	//USceneComponent
	virtual void OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport) override;
	virtual bool MoveComponentImpl( const FVector& Delta, const FQuat& NewRotation, bool bSweep, FHitResult* OutHit = NULL, EMoveComponentFlags MoveFlags = MOVECOMP_NoFlags, ETeleportType Teleport = ETeleportType::None ) override;

};
