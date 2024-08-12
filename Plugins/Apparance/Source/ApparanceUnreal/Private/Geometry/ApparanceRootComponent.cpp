//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

// main
#include "ApparanceRootComponent.h"

// unreal

// module
#include "ApparanceEntity.h"

//std


// notification that our actor has moved (but not details panel transform edit)
//
void UApparanceRootComponent::OnUpdateTransform( EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport )
{
	Super::OnUpdateTransform( UpdateTransformFlags, Teleport );

	//we need this to inform the entity editing system of object transform changes
	AApparanceEntity* pentity = Cast<AApparanceEntity>( GetAttachmentRootActor() );
	if(pentity)
	{
		pentity->NotifyTransformChanged();
	}
}


// catch actor move due to details panel transform edit
//
bool UApparanceRootComponent::MoveComponentImpl( const FVector& Delta, const FQuat& NewRotation, bool bSweep, FHitResult* OutHit, EMoveComponentFlags MoveFlags, ETeleportType Telepor )
{
	bool ret = Super::MoveComponentImpl( Delta, NewRotation, bSweep, OutHit, MoveFlags, Telepor );

	//we need this to inform the entity editing system of object transform changes
	AApparanceEntity* pentity = Cast<AApparanceEntity>( GetAttachmentRootActor() );
	if(pentity)
	{
		pentity->NotifyTransformChanged();
	}

	return ret;
}


