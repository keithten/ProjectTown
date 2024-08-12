//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

// main
#include "ResourceList/ApparanceResourceListEntry_Material.h"


// unreal
//#include "Components/BoxComponent.h"
//#include "Components/BillboardComponent.h"
//#include "Components/DecalComponent.h"
//#include "UObject/ConstructorHelpers.h"
//#include "Engine/Texture2D.h"
//#include "ProceduralMeshComponent.h"
//#include "Engine/World.h"
//#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

// module
#include "ApparanceUnreal.h"
#include "AssetDatabase.h"
//#include "Geometry.h"


#define LOCTEXT_NAMESPACE "ApparanceUnreal"

//////////////////////////////////////////////////////////////////////////
// UApparanceResourceListEntry_Material

FName UApparanceResourceListEntry_Material::GetIconName( bool is_open )
{
	return StaticIconName( is_open );
}

FName UApparanceResourceListEntry_Material::StaticIconName( bool is_open )
{
	return FName( "MaterialEditor.Tabs.HLSLCode" );
}

FText UApparanceResourceListEntry_Material::GetAssetTypeName() const
{
	return UApparanceResourceListEntry_Material::StaticAssetTypeName();
}

FText UApparanceResourceListEntry_Material::StaticAssetTypeName()
{
	return LOCTEXT( "MaterialAssetTypeName", "Material");
}

#if WITH_EDITOR
void UApparanceResourceListEntry_Material::PostEditChangeProperty(struct FPropertyChangedEvent& e)
{
	FName prop_name = e.GetPropertyName();
	if(prop_name == GET_MEMBER_NAME_CHECKED(UApparanceResourceListEntry_Asset, Asset))
	{
		CacheParameterInfo();
#if WITH_EDITOR
		SyncMaterialParameters();
#endif
	}
	
	Super::PostEditChangeProperty( e );
}
#endif

// load checks
//
void UApparanceResourceListEntry_Material::PostLoad()
{
	Super::PostLoad();

	CacheParameterInfo();
#if WITH_EDITOR
	SyncMaterialParameters();
#endif
}

void UApparanceResourceListEntry_Material::CheckAsset()
{
	Super::CheckAsset();

	//init parameter info
	CacheParameterInfo();
}


// validate texture parameter list
//
void UApparanceResourceListEntry_Material::CheckParameterList()
{
	Super::CheckParameterList();

	//id checks
	int id = 1000000;	//texture param ids kept well away from standard material ids
	for (int i = 0; i < ExpectedTextures.Num(); i++)
	{
		//find current max
		id = FMath::Max(id, ExpectedTextures[i].ID);
	}
	id++;
	for (int i = 0; i < ExpectedTextures.Num(); i++)
	{
		//ensure all have an ID
		if (ExpectedTextures[i].ID == 0)
		{
			ExpectedTextures[i].ID = id;
			id++;
		}
	}
}


// look up parameter info
//
const FApparancePlacementParameter* UApparanceResourceListEntry_Material::FindParameterInfo(int param_id, int* param_index_out) const
{
	for (int i = 0; i < ExpectedTextures.Num(); i++)
	{
		if (ExpectedTextures[i].ID == param_id)
		{
			if (param_index_out)
			{				
				//index textures after parameters
				*param_index_out = ExpectedParameters.Num() + i;
			}
			return &ExpectedTextures[i];
		}
	}

	//not found, try general params
	return Super::FindParameterInfo( param_id, param_index_out );
}

// look up name for an expected parameter
//
const FText UApparanceResourceListEntry_Material::FindParameterNameText(int param_id) const
{
	for (int i = 0; i < ExpectedTextures.Num(); i++)
	{
		if (ExpectedTextures[i].ID==param_id)
		{
			return FText::FromString(ExpectedTextures[i].Name);
		}
	}

	//not found, try general params
	return Super::FindParameterNameText( param_id );
}


// gather info about our materials parameters
//
void UApparanceResourceListEntry_Material::CacheParameterInfo()
{
	UMaterialInterface* imat = GetMaterial();
	if (imat)
	{
		//populate
		imat->GetAllScalarParameterInfo(ScalarParameters, ScalarParameterIDs);
		imat->GetAllVectorParameterInfo(VectorParameters, VectorParameterIDs);
		imat->GetAllTextureParameterInfo(TextureParameters, TextureParameterIDs);
	}
	else
	{
		//clear
		ScalarParameters.Empty();
		ScalarParameterIDs.Empty();
		VectorParameters.Empty();
		VectorParameterIDs.Empty();
		TextureParameters.Empty();
		TextureParameterIDs.Empty();
	}

	//ensure names present (update from legacy format where no name stored)
	for(int i=0 ; i<MaterialBindings.Num() ; i++)
	{
		if(MaterialBindings[i].ParameterName.IsNone())
		{
			auto* pm = FindMaterialParameter(MaterialBindings[i].ParameterID, FName());
			if (pm)
			{
				MaterialBindings[i].ParameterName = pm->Name;
			}
		}
	}
}

// add any missing material parameters
//
void UApparanceResourceListEntry_Material::SyncMaterialParameters()
{
	return; //WHY ARE WE DOING THIS, EFFECTIVELY OVERRIDING ALL MATERIAL DEFINITION DEFAULTS?
#if 0
	//scalars
	for (int i = 0; i < ScalarParameters.Num(); i++)
	{
		const FApparanceMaterialParameterBinding* mb = FindMaterialBinding( ScalarParameterIDs[i] );
		if (!mb)
		{
			AddScalarBinding(i);
		}		
	}

	//vectors
	for (int i = 0; i < VectorParameters.Num(); i++)
	{
		const FApparanceMaterialParameterBinding* mb = FindMaterialBinding( VectorParameterIDs[i] );
		if (!mb)
		{
			AddVectorBinding(i);
		}		
	}
	
	//textures
	for (int i = 0; i < TextureParameters.Num(); i++)
	{
		const FApparanceMaterialParameterBinding* mb = FindMaterialBinding( TextureParameterIDs[i] );
		if (!mb)
		{
			AddTextureBinding(i);
		}		
	}
#endif
}

// material parameter lookup
//
const FMaterialParameterInfo* UApparanceResourceListEntry_Material::FindMaterialParameter(FGuid material_parameter_id, FName material_parameter_name) const
{
	//scan known parameters
	for (int i = 0; i < ScalarParameterIDs.Num(); i++)
	{
		if (ScalarParameterIDs[i] == material_parameter_id)
		{
			return &ScalarParameters[i];
		}
	}
	for (int i = 0; i < VectorParameterIDs.Num(); i++)
	{
		if (VectorParameterIDs[i] == material_parameter_id)
		{
			return &VectorParameters[i];
		}
	}
	for (int i = 0; i < TextureParameterIDs.Num(); i++)
	{
		if (TextureParameterIDs[i] == material_parameter_id)
		{
			return &TextureParameters[i];
		}
	}

	//scan by name if no ID's available
	for (int i = 0; i < ScalarParameters.Num(); i++)
	{
		if (ScalarParameters[i].Name == material_parameter_name)
		{
			return &ScalarParameters[i];
		}
	}
	for (int i = 0; i < VectorParameters.Num(); i++)
	{
		if (VectorParameters[i].Name == material_parameter_name)
		{
			return &VectorParameters[i];
		}
	}
	for (int i = 0; i < TextureParameters.Num(); i++)
	{
		if (TextureParameters[i].Name == material_parameter_name)
		{
			return &TextureParameters[i];
		}
	}

	return nullptr;
}

// binding lookup
//
const FApparanceMaterialParameterBinding* UApparanceResourceListEntry_Material::FindMaterialBinding(FGuid material_parameter_id, FName material_parameter_name ) const
{
	for (int i = 0; i < MaterialBindings.Num(); i++)
	{
		if (MaterialBindings[i].ParameterID.IsValid())
		{
			//match by id if available (editor only)
			if (MaterialBindings[i].ParameterID == material_parameter_id)
			{
				return &MaterialBindings[i];
			}
		}
		else
		{
			//match by name otherwise (cooked)
			if(MaterialBindings[i].ParameterName == material_parameter_name)
			{
				return &MaterialBindings[i];
			}
		}
	}
	return nullptr;
}

// add a binding for a known scalar parameter
//
void UApparanceResourceListEntry_Material::AddScalarBinding(int scalar_parameter_index)
{
	//need to add a binding
	FApparanceMaterialParameterBinding new_mb;
	new_mb.ParameterID = ScalarParameterIDs[scalar_parameter_index];
	
	//find default
	float default_value = 0;
	UMaterialInterface* imat = GetMaterial();
	if (imat)
	{
		FHashedMaterialParameterInfo hmpi(ScalarParameters[scalar_parameter_index]);
		imat->GetScalarParameterValue(hmpi, default_value);
	}		
	
	//apply
	new_mb.Value = NewObject<UApparanceValueSource>(this, UApparanceValueSource::StaticClass(), NAME_None, RF_Transactional);
	new_mb.Value->Type = EApparanceParameterType::Float;
	new_mb.Value->SetConstant( default_value );			
	
	//add
	MaterialBindings.Add(new_mb);	
}

// add a binding for a known vector parameter
//
void UApparanceResourceListEntry_Material::AddVectorBinding(int vector_parameter_index)
{
	//need to add a binding
	FApparanceMaterialParameterBinding new_mb;
	new_mb.ParameterID = VectorParameterIDs[vector_parameter_index];
	
	//find default
	FLinearColor default_value = FLinearColor::Black;
	UMaterialInterface* imat = GetMaterial();
	if (imat)
	{
		FHashedMaterialParameterInfo hmpi(VectorParameters[vector_parameter_index]);
		imat->GetVectorParameterValue(hmpi, default_value);
	}		
	
	//apply
	new_mb.Value = NewObject<UApparanceValueSource>(this, UApparanceValueSource::StaticClass(), NAME_None, RF_Transactional);
	new_mb.Value->Type = EApparanceParameterType::Colour;
	new_mb.Value->SetConstant( default_value );			
	
	//add
	MaterialBindings.Add(new_mb);	
}

// add a binding for a known texture parameter
//
void UApparanceResourceListEntry_Material::AddTextureBinding(int texture_parameter_index)
{
	//need to add a binding
	FApparanceMaterialParameterBinding new_mb;
	new_mb.ParameterID = TextureParameterIDs[texture_parameter_index];
	
	//find default
	class UTexture* default_value = nullptr;
	UMaterialInterface* imat = GetMaterial();
	if (imat)
	{
		FHashedMaterialParameterInfo hmpi(TextureParameters[texture_parameter_index]);
		imat->GetTextureParameterValue(hmpi, default_value);
	}		
	
	//apply
	new_mb.Value = NewObject<UApparanceValueSource>(this, UApparanceValueSource::StaticClass(), NAME_None, RF_Transactional);
	new_mb.Value->Type = EApparanceParameterType::Texture;
	new_mb.Value->SetConstant( default_value );			
	
	//add
	MaterialBindings.Add(new_mb);	
}

// binding list edit, attempt to add
//
void UApparanceResourceListEntry_Material::AddBinding(FGuid material_parameter_id)
{
	//exists?
	for (int i = 0; i < MaterialBindings.Num(); i++)
	{
		if (MaterialBindings[i].ParameterID == material_parameter_id)
		{
			//already got this one
			return;
		}
	}

	//scan known parameters for one to add
	for (int i = 0; i < ScalarParameterIDs.Num(); i++)
	{
		if (ScalarParameterIDs[i] == material_parameter_id)
		{
			AddScalarBinding(i);
			return;
		}
	}
	for (int i = 0; i < VectorParameterIDs.Num(); i++)
	{
		if (VectorParameterIDs[i] == material_parameter_id)
		{
			AddVectorBinding(i);
			return;
		}
	}
	for (int i = 0; i < TextureParameterIDs.Num(); i++)
	{
		if (TextureParameterIDs[i] == material_parameter_id)
		{
			AddTextureBinding(i);
			return;
		}
	}
}


// apply parameter and texture mapping using out apparance parameter to material parameter mapping info
//
void UApparanceResourceListEntry_Material::Apply( class UMaterialInstanceDynamic* pmaterial, Apparance::IParameterCollection* parameters, TArray<Apparance::TextureID>& textures ) const
{
	if(pmaterial) //TODO: investigate that we seem to get dead mat's sometimes when editing rapidly
	{
		//work through all the bindings
		if(parameters)
		{
			parameters->BeginAccess();
		}
		for (int b = 0; b < MaterialBindings.Num(); b++)
		{
			const FApparanceMaterialParameterBinding& binding = MaterialBindings[b];
			if(binding.Value)
			{
				//which material parameter?
				const FMaterialParameterInfo* info = FindMaterialParameter(binding.ParameterID, binding.ParameterName);
				if (info)
				{
					//source type
					switch(binding.Value->Type)
					{
						case EApparanceParameterType::Float:
						{
							float value = binding.Value->GetFloatValue(parameters, this);
							pmaterial->SetScalarParameterValueByInfo(*info, value);
							break;
						}
						case EApparanceParameterType::Colour:
						{
							FLinearColor value = binding.Value->GetColourValue(parameters, this);
							pmaterial->SetVectorParameterValueByInfo(*info, value);
							break;
						}
						case EApparanceParameterType::Texture:
						{
							class UTexture* value = binding.Value->GetTextureValue(textures, this);
							pmaterial->SetTextureParameterValueByInfo(*info, value);
							break;
						}
					}
				}
			}
		}
		if(parameters)
		{
			parameters->EndAccess();
		}
	}
}