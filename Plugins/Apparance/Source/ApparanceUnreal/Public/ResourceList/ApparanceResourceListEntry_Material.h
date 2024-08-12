//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

#pragma once

// unreal
#include "CoreMinimal.h"
#include "Materials/MaterialInterface.h"

// apparance
#include "Apparance.h"

// module
#include "ApparanceResourceListEntry_Asset.h"
#include "ApparanceResourceListEntry_Component.h"

// auto (last)
#include "ApparanceResourceListEntry_Material.generated.h"


// parameter binding record
//
USTRUCT()
struct FApparanceMaterialParameterBinding
{
	GENERATED_BODY()

	FApparanceMaterialParameterBinding() : Value(nullptr) {}

	//material property to bind to (standalone build)
	UPROPERTY()
	FName ParameterName;

	//material property to bind to (editor build)
	UPROPERTY()
	FGuid ParameterID;

	//source of value
	UPROPERTY()
	UApparanceValueSource* Value;
};


// entry for mapping resource name to material asset
//
UCLASS()
class APPARANCEUNREAL_API UApparanceResourceListEntry_Material
	: public UApparanceResourceListEntry_Asset
{
	GENERATED_BODY()

public:
	
	UPROPERTY( EditAnywhere, meta = (DisplayName = "Generates Collision", ToolTip = "Meshes using this material will generate complex collision instead of visible geometry (if material set then will still generate visible geometry)"), Category = Apparance )
	bool bUseForComplexCollision;

	//Expected incoming texture parameters
	UPROPERTY( EditAnywhere, Category = Apparance )
	TArray<FApparancePlacementParameter> ExpectedTextures;

	//How to bind incoming parameters and textures to the exposed material parameters
	UPROPERTY( EditAnywhere, Category = Apparance )
	TArray<FApparanceMaterialParameterBinding> MaterialBindings;

	//transient, material param info cache
	TArray<FMaterialParameterInfo> ScalarParameters;
	TArray<FGuid>                  ScalarParameterIDs;
	TArray<FMaterialParameterInfo> VectorParameters;
	TArray<FGuid>                  VectorParameterIDs;
	TArray<FMaterialParameterInfo> TextureParameters;
	TArray<FGuid>                  TextureParameterIDs;
	
public:
	UMaterialInterface* GetMaterial() const { return Cast<UMaterialInterface>( GetAsset() ); };

public:
	
	/* Begin UObject */
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	virtual void PostLoad() override;
	/* End UObject */
	
	//access
	virtual UClass* GetAssetClass() const override { return UMaterialInterface::StaticClass(); }
	virtual FText GetAssetTypeName() const override;
	virtual FName GetIconName( bool is_open=false ) override;

	static FText StaticAssetTypeName();
	static FName StaticIconName( bool is_open=false );

	//parameter binding
	const FMaterialParameterInfo* FindMaterialParameter( FGuid material_parameter_id, FName material_parameter_name ) const;
	const FApparanceMaterialParameterBinding* FindMaterialBinding( FGuid material_parameter_id, FName material_parameter_name ) const;
	void AddBinding(FGuid material_parameter_id);

	//material params
	virtual void CheckParameterList() override;	
	virtual const FApparancePlacementParameter* FindParameterInfo( int parameter_id, int* param_index_out=nullptr ) const override;
	virtual const FText FindParameterNameText( int param_id ) const override;
	void CacheParameterInfo();
	void SyncMaterialParameters();
	
	virtual void CheckAsset() override;

	//parameterisation
	void Apply( class UMaterialInstanceDynamic* pmaterial, Apparance::IParameterCollection* parameters, TArray<Apparance::TextureID>& textures ) const;

private:
	void AddScalarBinding(int scalar_parameter_index);
	void AddVectorBinding(int vector_parameter_index);
	void AddTextureBinding(int texture_parameter_index);
	
};

