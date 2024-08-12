//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

#pragma once

// unreal
#include "Components/ActorComponent.h"
#include "Components/InstancedStaticMeshComponent.h"

// apparance
#include "Apparance.h"

// module

// auto (last)
#include "ApparanceParametersComponent.generated.h"

typedef TSharedPtr<Apparance::IParameterCollection> TParametersPtr;
typedef TArray<TParametersPtr> TParametersPtrArray;
typedef TSharedPtr<TParametersPtrArray> TParametersPtrArrayPtr;

// single object responsible for single placement
//
USTRUCT()
struct FProceduralPlacement
{
	GENERATED_BODY()
	
	TParametersPtr	Parameters;	
};

// single object responsible for multiple placements
//
USTRUCT()
struct FProceduralPlacements
{
	GENERATED_BODY()
		
	//array of parameters, one for each placed instance
	TParametersPtrArrayPtr	ParametersArray;
};


// container for placement procedural parameters, use cases:
// 1. procedurally placed actor with parameters from the placement proc
// 2. any procedural entities own input parameters can be accessed in a consistent way
// 3. any placed mesh component (or instance) might have data associated with it
//
UCLASS()
class APPARANCEUNREAL_API UApparanceParametersComponent
	: public UActorComponent
{
	GENERATED_BODY()

	// transient

	//the root actor parameters (if it is procedural)
	mutable TParametersPtr										ActorParameters;

	//the procedurally placed blueprint actors
	TMap<AActor*, FProceduralPlacement>							BlueprintActorParameters;
	
	//the procedurally placed meshes
	TMap<UStaticMeshComponent*, FProceduralPlacement>			StaticMesheParameters;

	//the procedurally placed instanced meshes
	TMap<UInstancedStaticMeshComponent*, FProceduralPlacements>	InstancedStaticMesheParameters;

public:

private:

public:
	UApparanceParametersComponent();

	// parameter storage
	void SetActorParameters(const Apparance::IParameterCollection* placement_parameters);
	void AddBlueprintActorParameters( class AActor* pbpactor, const Apparance::IParameterCollection* placement_parameters);
	void RemoveBlueprintActorParameters( class AActor* pbpactor );
	void AddStaticMeshParameters( class UStaticMeshComponent* pmesh, const Apparance::IParameterCollection* placement_parameters);
	void RemoveStaticMeshParameters( class UStaticMeshComponent* pmesh );
	void AddInstancedStaticMeshParameters( class UInstancedStaticMeshComponent* pmesh, int instance_index, const Apparance::IParameterCollection* placement_parameters);
	void RemoveInstancedStaticMeshParameters( class UInstancedStaticMeshComponent* pmesh );
	
	// parameter access
	const Apparance::IParameterCollection* GetActorParameters() const;
	const Apparance::IParameterCollection* GetBluepringActorParameters( class AActor* pbpactor ) const;
	//const Apparance::IParameterCollection* GetStaticMeshParameters( class UStaticMeshComponent* pmesh ) const;
	//const Apparance::IParameterCollection* GetInstancedStaticMeshParameters( class UInstancedStaticMeshComponent* pmesh, int instance_index ) const;


public:
	//~ Begin UObject Interface
	//~ End UObject Interface

	//~ Begin UActorComponent Interface
	//~ End AActor Interface


private:

	void FindActorParameters() const;

};

