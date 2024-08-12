//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

// main
#include "ApparanceParametersComponent.h"


// unreal
//#include "Components/BoxComponent.h"
//#include "Components/BillboardComponent.h"
//#include "Components/DecalComponent.h"
//#include "UObject/ConstructorHelpers.h"
//#include "Engine/Texture2D.h"
//#include "ProceduralMeshComponent.h"
//#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"

// module
#include "ApparanceUnreal.h"
#include "ApparanceEntity.h"


//////////////////////////////////////////////////////////////////////////
// AApparanceEntity

UApparanceParametersComponent::UApparanceParametersComponent()
{
}

void UApparanceParametersComponent::SetActorParameters( const Apparance::IParameterCollection* placement_parameters )
{
	//init on demand, ensure storage
	if (!ActorParameters.IsValid())
	{
		ActorParameters = MakeShareable(FApparanceUnrealModule::GetEngine()->CreateParameterCollection());
	}

	//store new parameters
	ActorParameters->Merge(placement_parameters);
}
void UApparanceParametersComponent::AddBlueprintActorParameters( AActor* pbpactor, const Apparance::IParameterCollection* placement_parameters )
{
	if(placement_parameters)
	{
		int count = placement_parameters->BeginAccess();
		placement_parameters->EndAccess();
		if(count > 0) //don't bother if none
		{
			FProceduralPlacement& placement = BlueprintActorParameters.FindOrAdd( pbpactor );

			//ensure parameters
			if(!placement.Parameters.IsValid())
			{
				placement.Parameters = MakeShareable( FApparanceUnrealModule::GetEngine()->CreateParameterCollection() );
			}

			//store
			placement.Parameters->Merge( placement_parameters );
		}
	}
}
void UApparanceParametersComponent::RemoveBlueprintActorParameters(AActor* pbpactor)
{
	BlueprintActorParameters.Remove( pbpactor );
}
void UApparanceParametersComponent::AddStaticMeshParameters( UStaticMeshComponent* pmesh, const Apparance::IParameterCollection* placement_parameters)
{
	FProceduralPlacement& placement = StaticMesheParameters.FindOrAdd( pmesh );

	//ensure parameters
	if(!placement.Parameters.IsValid())
	{
		placement.Parameters = MakeShareable( FApparanceUnrealModule::GetEngine()->CreateParameterCollection() );
	}

	//store
	placement.Parameters->Merge( placement_parameters );
}
void UApparanceParametersComponent::RemoveStaticMeshParameters(UStaticMeshComponent* pmesh)
{
	StaticMesheParameters.Remove(pmesh);
}
void UApparanceParametersComponent::AddInstancedStaticMeshParameters(UInstancedStaticMeshComponent* pmesh, int instance_index, const Apparance::IParameterCollection* placement_parameters)
{
	FProceduralPlacements& placements = InstancedStaticMesheParameters.FindOrAdd( pmesh );
	
	//ensure parameters array
	if (!placements.ParametersArray.IsValid())
	{
		placements.ParametersArray = MakeShareable(new TParametersPtrArray());
	}
	TParametersPtrArray& ptr_array = *placements.ParametersArray.Get();
	
	//add entries for instance
	if (instance_index >= ptr_array.Num())
	{
		ptr_array.SetNum( instance_index+1 );
	}

	//ensure params for storage
	if(!ptr_array[instance_index].IsValid())
	{
		ptr_array[instance_index] = MakeShareable( FApparanceUnrealModule::GetEngine()->CreateParameterCollection() );
	}
	
	//store
	ptr_array[instance_index]->Merge( placement_parameters );
}
void UApparanceParametersComponent::RemoveInstancedStaticMeshParameters(UInstancedStaticMeshComponent* pmesh)
{
	InstancedStaticMesheParameters.Remove( pmesh );
}

const Apparance::IParameterCollection* UApparanceParametersComponent::GetActorParameters() const
{
	//attempt to init if not set
	if(!ActorParameters.IsValid())
	{
		FindActorParameters();
	}

	return ActorParameters.Get(); 
}

const Apparance::IParameterCollection* UApparanceParametersComponent::GetBluepringActorParameters( class AActor* pbpactor ) const
{
	const FProceduralPlacement* placement = BlueprintActorParameters.Find( pbpactor );
	if(placement)
	{
		return placement->Parameters.Get();
	}
	return nullptr;
}

void UApparanceParametersComponent::FindActorParameters() const
{
	AApparanceEntity* pentity = GetTypedOuter<AApparanceEntity>();
	if(pentity)
	{
		const Apparance::IParameterCollection* ent_params = pentity->GetParameters();

		//assign
		if(!ActorParameters.IsValid())
		{
			ActorParameters = MakeShareable( FApparanceUnrealModule::GetEngine()->CreateParameterCollection() );
		}
		ActorParameters->Merge( ent_params );
	}
}
