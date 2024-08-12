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

//module
#include "ApparanceResourceList.h"
#include "EntityRendering.h"

#if WITH_EDITOR
// tracking of material use for dynamic resource updates
struct FApparanceMaterialUse
{
	TWeakObjectPtr<const UApparanceResourceListEntry_Material>	Material;
	TWeakPtr<FParameterisedMaterial>							Parameters;

	FApparanceMaterialUse() {}
	FApparanceMaterialUse( const UApparanceResourceListEntry_Material* pmaterial, TSharedPtr<FParameterisedMaterial> pparams )
		: Material( pmaterial )
		, Parameters( pparams )
	{}

	// still points at living use case?
	bool IsValid() const
	{
		return Material.IsValid() && Parameters.IsValid();
	}

	// a texture changed, re-apply if relevant to this material use case
	//
	void NotifyTextureChanged( Apparance::TextureID texture_id )
	{
		if(IsValid())
		{
			TSharedPtr<FParameterisedMaterial> mat_entry_ptr = Parameters.Pin();
			FParameterisedMaterial* ppm = mat_entry_ptr.Get();
			if(ppm)
			{
				if(ppm->Textures.Contains( texture_id ))
				{
					ReApply();					
				}
			}
		}
	}
private:
	//re-do the material parameters to bring them up to date with any asset changes
	void ReApply()
	{
		if(Material.IsValid() && Parameters.IsValid())
		{
			TSharedPtr<FParameterisedMaterial> mat_entry_ptr = Parameters.Pin();
			FParameterisedMaterial* mat_entry = mat_entry_ptr.Get();
			Material.Get()->Apply( mat_entry->MaterialInstance.Get(), mat_entry->Parameters.Get(), mat_entry->Textures );
		}
	}
};
#endif

// capture info about a new texture to add (deferred to main thread)
//
struct FDeferredTextureCreation
{
	FTexturePlatformData* PlatformData;
	FTexture2DMipMap* Mip;
	Apparance::TextureID TextureID;
	bool bFirstTime;

	//do
	void Apply( struct FAssetDatabase* asset_db );
};


// Adaptor to routes apparance asset requests to the Unreal resources lists
//
struct FAssetDatabase : public Apparance::Host::IAssetDatabase
{
	//- static state -
	
	//root of asset database
	class UApparanceResourceList* ResourceRoot;

	//shared resource entry to stand in for missing resources
	class UApparanceResourceListEntry* BadResource;

	int BadAssetID;

	//- dynamic state -
	FCriticalSection CacheInterlock;	//protect dynamic state from multi-thread access (e.g. synth threads)

	int DBVersionNumber;	//database change tracking
	int NextAssetID;		//assigning of ID's

	//asset id assignment
	TMap<FName,Apparance::AssetID> ResourceIDs;
	
	//asset use
	TMap<Apparance::AssetID,const class UApparanceResourceListEntry*> ResourceEntries;

	//dynamic assets
	TMap<Apparance::TextureID, class UTexture2D*> DynamicTextures;
	FCriticalSection DynamicTexturesInterlock;
	TSet<Apparance::TextureID> RetiredTextures;	//shares dynamic textures interlock (due to deadlock issue)
	TSet<Apparance::TextureID> ChangedTextures;
	FCriticalSection ChangedTexturesInterlock;
	TArray<FDeferredTextureCreation> NewTextures;
	FCriticalSection NewTexturesInterlock;
#if WITH_EDITOR
	TArray<FApparanceMaterialUse> MaterialUseTracking;
	int MaterialTrackingCursor;
#endif

	//missing assets
	TArray<FString> MissingAssets;
	
public:
	FAssetDatabase();
	virtual ~FAssetDatabase();
	void Shutdown();

	//access

	//control
	void SetRootResourceList(class UApparanceResourceList* presources);
	void Tick();
	void Invalidate();
	void Reset();

	//~ Begin Apparance IAssetDatabase Interface
	virtual bool CheckAssetValid(const char* pszassetdescriptor, int entity_context) override;
	virtual Apparance::AssetID GetAssetID(const char* pszassetdescriptor, int entity_context) override;
	virtual bool GetAssetBounds(const char* pszassetdescriptor, int entity_context, Apparance::Frame& out_bounds) override;
	virtual int GetAssetVariants(const char* pszassetdescriptor, int entity_context) override;
	//dynamic procedural resource support
	virtual Apparance::AssetID AddDynamicAsset() override;
	virtual void SetDynamicAssetTexture( Apparance::AssetID id, const void* pdata, int width, int height, int channels, Apparance::ImagePrecision::Type precision ) override;
	virtual void RemoveDynamicAsset( Apparance::AssetID id ) override;
	//~ End Apparance IAssetDatabase Interface

	//internal asset access
	bool GetMaterial( Apparance::MaterialID material_id, class UMaterialInterface*& pmaterial_out, const class UApparanceResourceListEntry_Material*& pmaterialentry_out, bool* pwant_collision_out=nullptr );
	bool GetObject(Apparance::ObjectID object_id, const UApparanceResourceListEntry*& presourceentry_out );
	bool GetTexture( Apparance::TextureID texture_id, class UTexture*& ptexture_out );
	
#if WITH_EDITOR
	//editor-only tracking
	void Editor_TrackMaterialUse( const UApparanceResourceListEntry_Material* pmaterial, TSharedPtr<FParameterisedMaterial> pparams )
	{
		MaterialUseTracking.Emplace( pmaterial, pparams );
	}
#endif

private:
	//asset search
	Apparance::AssetID GetAsset( FName asset_descriptor, const class UApparanceResourceListEntry** resource_info_out=nullptr );
	const class UApparanceResourceListEntry* SearchResourceLists( const FString& descriptor, TSet<class UApparanceResourceList*>& visited, class UApparanceResourceList* plist=nullptr );
	int CacheAssetInfo( FName asset_descriptor, const class UApparanceResourceListEntry* asset_info );
		
	//asset info
	bool GetAssetBounds( const class UApparanceResourceListEntry* presource_info, Apparance::Frame& out_bounds );
	bool GetAssetVariants( const class UApparanceResourceListEntry* presource_info, int& out_variants );

	//helpers
	int MakeBadAsset( FName asset_descriptor );
	
	//dynamic helpers
	void PurgeDynamicAssets();
	void CreateDynamicAssets();

#if WITH_EDITOR
	void Editor_PurgeMaterialMonitors();
	void Editor_NotifyTextureChanged( Apparance::TextureID texture_id );
	void Editor_NotifyChangedTextures();
#endif
};

