//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

//local debugging help
#define APPARANCE_DEBUGGING_HELP_AssetDatabase 0
#if APPARANCE_DEBUGGING_HELP_AssetDatabase
PRAGMA_DISABLE_OPTIMIZATION_ACTUAL
#endif
#define ENABLE_TEXTUREGEN_DIAGS 0

// main
#include "AssetDatabase.h"

// unreal
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Engine/Texture2D.h"
#include "TextureResource.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h"

// apparance
#include "ApparanceResourceList.h"
#include "Utility/ApparanceConversion.h"
#include "ApparanceUnrealVersioning.h"

// module
#include "ApparanceUnreal.h"


//////////////////////////////////////////////////////////////////////////
// FAssetDatabase

FAssetDatabase::FAssetDatabase()
	: BadResource(nullptr)
	, DBVersionNumber( 0 )
#if WITH_EDITOR
	, MaterialTrackingCursor( 0 )
#endif
{
	Invalidate();
}

FAssetDatabase::~FAssetDatabase()
{
	Shutdown();
}

void FAssetDatabase::Shutdown()
{
	Reset();
	if(BadResource 
		&& !IsEngineExitRequested())	//had some crashes trying to cleanup on exit
	{
		BadResource->RemoveFromRoot();
		BadResource = nullptr;
	}
}

void FAssetDatabase::Reset()
{
	FScopeLock interlock( &CacheInterlock );
	ResourceIDs.Empty();
	ResourceEntries.Empty();
	NextAssetID = 1;
	BadAssetID = 0;
	DBVersionNumber++;
	MissingAssets.Empty();
}

// init main resource access point
// NOTE: public, thread safe
//
void FAssetDatabase::SetRootResourceList(UApparanceResourceList* presources)
{
	FScopeLock interlock(&CacheInterlock);

	ResourceRoot = presources;

	//ensure we have a missing resource standin
	if(!BadResource)
	{
		//use non-matching base to signify bad asset, each GetX will use appropriate fallback asset type
		BadResource = NewObject<UApparanceResourceListEntry>();
		BadResource->SetName( TEXT("missing resource") );
		BadResource->AddToRoot(); //ensure stay's around
	}
	
	Invalidate();
}

// regular updates/checks
//
void FAssetDatabase::Tick()
{
	CreateDynamicAssets();
	PurgeDynamicAssets();

#if WITH_EDITOR
	Editor_PurgeMaterialMonitors();
	Editor_NotifyChangedTextures();
#endif
}

// reset any cached data to allow re-caching based on updated settings
// NOTE: public, thread safe
//
void FAssetDatabase::Invalidate()
{
	Reset();

	//trigger rebuild of all entities
	if (FApparanceUnrealModule::GetModule())
	{
#if WITH_EDITOR
		FApparanceUnrealModule::GetModule()->Editor_NotifyAssetDatabaseChanged();
#endif
	}
}

// asset cache lookup, triggers search for caching if not cached already
// NOTE: private, not thread safe
//
Apparance::AssetID FAssetDatabase::GetAsset( FName asset_descriptor, const UApparanceResourceListEntry** resource_info_out )
{
	Apparance::AssetID id = Apparance::InvalidID;

	//check cache (fast path)
	Apparance::AssetID* passetid = ResourceIDs.Find( asset_descriptor );
	if(passetid)
	{
		id = *passetid;
	}
	else
	{
		//find and cache (slow path)
		if(ResourceRoot)
		{	
			//search
			TSet<UApparanceResourceList*> visited;
			const UApparanceResourceListEntry* asset_info = SearchResourceLists( asset_descriptor.ToString(), visited, ResourceRoot );
			if(asset_info)
			{
				//found, cache it
				id = CacheAssetInfo( asset_descriptor, asset_info );
				
				//if caller wants the info too, handle below so get ptr to cached version
			}
#if ENABLE_TEXTUREGEN_DIAGS
			UE_LOG( LogApparance, Log, TEXT( "TEXTUREGEN: RESOLVED RESOURCE %i : %s" ), id, *asset_descriptor.ToString() );
#endif
		}
	}

	//use bad-asset entry if not resolved
	if (id == 0)
	{
		//store bad entry for this descriptor
		id = MakeBadAsset(asset_descriptor);

		//record the missing asset
		MissingAssets.Add(asset_descriptor.ToString());
	}

	//want res info too?
	if(resource_info_out)
	{
		if (id != Apparance::InvalidID)
		{
			//lookup
			*resource_info_out = ResourceEntries[id];
		}
		else
		{
			*resource_info_out = nullptr;
		}
	}

	return id;
}

// store resolved info about an asset for later use and get an ID to use to refer to it
// NOTE: private, not thread safe
//
int FAssetDatabase::CacheAssetInfo( FName asset_descriptor, const UApparanceResourceListEntry* asset_info )
{
	int id = 0;
	FScopeLock interlock(&CacheInterlock);

	//is variant?
	FString descriptor_string = asset_descriptor.ToString();
	int hash_index = descriptor_string.Find(TEXT("#"));
	const UApparanceResourceListEntry_Variants* pvariants = Cast<UApparanceResourceListEntry_Variants>(asset_info);
	if(hash_index!=INDEX_NONE && pvariants)
	{
		uint32 variant_number = FCString::Atoi( &descriptor_string[hash_index+1] );
		int variant_index = variant_number-1;

		//already assigned?
		id = pvariants->GetVariantID( variant_number, DBVersionNumber );
		if(id==0)
		{
			//assign first time
			NextAssetID = pvariants->SetVariantID( NextAssetID, DBVersionNumber );

			//re-request
			id = pvariants->GetVariantID( variant_number, DBVersionNumber );
			check(id!=0);
		}

		//dereference child for variant		
		if(variant_index<asset_info->Children.Num())
		{
			//child has full asset info
			asset_info = asset_info->Children[variant_index];
		}
	}
	else
	{
		//singular asset
		id = NextAssetID++;
	}

	//found
	ResourceIDs.Add( asset_descriptor, id );
	ResourceEntries.Add( id, asset_info );	//kept as a copy
	return id;
}

// common resource list search needed for entry to be cached
// NOTE: private, not thread safe
//
const UApparanceResourceListEntry* FAssetDatabase::SearchResourceLists( const FString& descriptor, TSet<class UApparanceResourceList*>& visited, class UApparanceResourceList* plist )
{
	//remember where we've looked
	visited.Add( plist );

	//check each list
	const UApparanceResourceListEntry* pinfo = plist->FindResourceEntry( descriptor );
	if(pinfo)
	{
		return pinfo;
	}

	//follow any references to other lists this list has
	if (plist->References)
	{
		for (int i = 0; i < plist->References->Children.Num(); i++)
		{
			UApparanceResourceListEntry_ResourceList* prl = Cast<UApparanceResourceListEntry_ResourceList>(plist->References->Children[i]);
			if (prl)
			{
				UApparanceResourceList* psublist = prl->GetResourceList();
				if (psublist && !visited.Contains(psublist))	//break any accidental loops the user may create
				{
					pinfo = SearchResourceLists(descriptor, visited, psublist);
					if (pinfo)
					{
						return pinfo;
					}
				}
			}
		}
	}

	return nullptr;
}

// asset info extraction helper : bounds
// NOTE: public, thread safe
//
bool FAssetDatabase::GetAssetBounds( const UApparanceResourceListEntry* presource_info, Apparance::Frame& out_bounds )
{
	FScopeLock interlock(&CacheInterlock);
	
	//anything to query?
	if (!presource_info)
	{
		return false;
	}

	FVector centre;
	FVector extents;
	if(presource_info->GetBounds(centre, extents))
	{
		out_bounds.Origin = APPARANCESPACE_FROM_UNREALSPACE( centre-extents );
		out_bounds.Size = APPARANCESPACE_FROM_UNREALSPACE( extents * 2.0f );
		//no orientation
		out_bounds.Orientation.X = Apparance::Vector3(1,0,0);
		out_bounds.Orientation.Y = Apparance::Vector3(0,1,0);
		out_bounds.Orientation.Z = Apparance::Vector3(0,0,1);
		return true;
	}

	return false;
}

// asset info extraction helper : variants
//
bool FAssetDatabase::GetAssetVariants( const UApparanceResourceListEntry* presource_info, int& out_variants )
{
	//try self
	const UApparanceResourceListEntry_Variants* pvariants_info = Cast<UApparanceResourceListEntry_Variants>( presource_info );
	if(pvariants_info)
	{
		out_variants = pvariants_info->GetVariantCount();
		return true;
	}

	//check if it is a variant itself which should know but via parent
	const UApparanceResourceListEntry_Variants* pparant_variants_info = Cast<UApparanceResourceListEntry_Variants>( presource_info->FindParent() );
	if (pparant_variants_info)
	{
		out_variants = pparant_variants_info->GetVariantCount();
		return true;
	}

	out_variants = 0;
	return false;
}


// Apparance::Host::IAssetDatabase implementation
// CheckAssetValid
// NOTE: public, thread safe
//
bool FAssetDatabase::CheckAssetValid(const char* pszassetdescriptor, int entity_context)
{
	const UApparanceResourceListEntry* resource_info;
	GetAsset(pszassetdescriptor, &resource_info);
	return resource_info != BadResource;
}

// Apparance::Host::IAssetDatabase implementation
// GetAssetID
// NOTE: public, thread safe
//
Apparance::AssetID FAssetDatabase::GetAssetID(const char* pszassetdescriptor, int entity_context)
{
	FScopeLock interlock(&CacheInterlock);
	
	//work with descriptor as fname
	FUTF8ToTCHAR convert( pszassetdescriptor );
	FName asset_descriptor( convert.Get() );

	//look for asset info
	//we just want the id
	return GetAsset( asset_descriptor );	
}

// Apparance::Host::IAssetDatabase implementation
// GetAssetBounds
// NOTE: public, thread safe
//
bool FAssetDatabase::GetAssetBounds(const char* pszassetdescriptor, int entity_context, Apparance::Frame& out_bounds)
{
	FScopeLock interlock(&CacheInterlock);

	//work with descriptor as fname
	FUTF8ToTCHAR convert( pszassetdescriptor );
	FName asset_descriptor( convert.Get() );

	//look for asset info
	const UApparanceResourceListEntry* presource_info;
	if(GetAsset( asset_descriptor, &presource_info )!=Apparance::InvalidID)
	{
		//determine specific info we want
		if(GetAssetBounds( presource_info, out_bounds ))
		{
			return true;
		}
	}

	return false;
}

// Apparance::Host::IAssetDatabase implementation
// GetAssetVariants
// NOTE: public, thread safe
//
int FAssetDatabase::GetAssetVariants(const char* pszassetdescriptor, int entity_context)
{
	FScopeLock interlock(&CacheInterlock);

	//work with descriptor as fname
	FUTF8ToTCHAR convert( pszassetdescriptor );
	FName asset_descriptor( convert.Get() );

	//look for asset info
	const UApparanceResourceListEntry* presource_info;
	if(GetAsset( asset_descriptor, &presource_info )!=Apparance::InvalidID)
	{
		//determine specific info we want
		int out_variants = 0;
		if(GetAssetVariants( presource_info, out_variants ))
		{
			return out_variants;
		}
	}
	
	return 0;
}

// access any cached material reference by ID (sets output to null if not found)
// NOTE: public, thread safe
//
bool FAssetDatabase::GetMaterial( Apparance::MaterialID material_id, class UMaterialInterface*& pmaterial_out, const class UApparanceResourceListEntry_Material*& pmaterialentry_out, bool* pwant_collision_out )
{
	pmaterial_out = nullptr;
	pmaterialentry_out = nullptr;

	//was successfully given an id from original resolve?
	FScopeLock interlock(&CacheInterlock);

	//find an entry
	const UApparanceResourceListEntry** pentry = ResourceEntries.Find(material_id);
	if (pentry)
	{
		//is it a material?
		const UApparanceResourceListEntry_Material* pmat_info = Cast<UApparanceResourceListEntry_Material>(*pentry);
		if (pmat_info)
		{
			pmaterial_out = pmat_info->GetMaterial();	//note: might not be set if just using for collision
			pmaterialentry_out = pmat_info;
			if(pwant_collision_out)
			{
				*pwant_collision_out = pmat_info->bUseForComplexCollision;
			}
			return true;
		}
	}

	//not resolved, use fallback
	pmaterial_out = FApparanceUnrealModule::GetModule()->GetFallbackMaterial();
	if(pwant_collision_out)
	{
		*pwant_collision_out = false;
	}
	return false;
}

// access any cached mesh/blueprint reference by ID (sets output to null if not found)
// NOTE: will count as found (returns true) even if no objects assigned to the resource entry
// NOTE: public, thread safe
//
bool FAssetDatabase::GetObject(Apparance::ObjectID object_id, const UApparanceResourceListEntry*& presourceentry_out )
{
	presourceentry_out = nullptr;

	//was successfully given an id from original resolve?
	FScopeLock interlock(&CacheInterlock);

	//look for an entry
	const UApparanceResourceListEntry** pentry = ResourceEntries.Find(object_id);
	if (pentry)
	{
		presourceentry_out = *pentry;
	}

	return false;
}

// access any cached texture reference by ID (sets output to null if not found)
// NOTE: public, thread safe
//
bool FAssetDatabase::GetTexture( Apparance::TextureID texture_id, class UTexture*& ptexture_out )
{
	ptexture_out = nullptr;
	
	//was successfully given an id from original resolve?
	FScopeLock interlock(&CacheInterlock);
	
	//find an entry (static resources)
	const UApparanceResourceListEntry** pentry = ResourceEntries.Find(texture_id);
	if (pentry)
	{
		//is it a material?
		const UApparanceResourceListEntry_Texture* ptex_info = Cast<UApparanceResourceListEntry_Texture>(*pentry);
		if (ptex_info)
		{
			ptexture_out = ptex_info->GetTexture();
#if ENABLE_TEXTUREGEN_DIAGS
			UE_LOG(LogApparance, Log, TEXT("TEXTUREGEN: Get Texture %i : %s"), texture_id, *ptexture_out->GetName());
#endif
			return true;
		}
	}

	//further searches (dynamic resources)
	{
		FScopeLock lock( &DynamicTexturesInterlock );
		UTexture2D** pfoundtexture = DynamicTextures.Find( texture_id );
		if(pfoundtexture)
		{
			ptexture_out = *pfoundtexture;
#if ENABLE_TEXTUREGEN_DIAGS
			UE_LOG(LogApparance, Log, TEXT("TEXTUREGEN: Get Texture %i : %s"), texture_id, *ptexture_out->GetName());
#endif
			return true;
		}
	}

	//not resolved, use fallback
	ptexture_out = FApparanceUnrealModule::GetModule()->GetFallbackTexture();

#if ENABLE_TEXTUREGEN_DIAGS
	UE_LOG(LogApparance, Log, TEXT("TEXTUREGEN: Get Texture %i - FALLBACK"), texture_id);
#endif

	return false;
}
// cache bad asset on demand for cases where asset isn't resolved (no entry for the given descriptor)
// This is so we at least see something, and hopefully it stands out showing the issue
// NOTE: private, not thread safe
//
int FAssetDatabase::MakeBadAsset( FName asset_descriptor )
{
	//NOTE: re-uses same BadResource object because we can't create new ones here due to being called from synth threads

	//cache and obtain ID
	return CacheAssetInfo( asset_descriptor, BadResource );
}


// dynamic resource support, assign an asset ID to a new resource
// Threading Note: Not called on main thread, can be called from more than one thread at once
//
Apparance::AssetID FAssetDatabase::AddDynamicAsset()
{
	Apparance::AssetID asset_id = NextAssetID++;
	return asset_id;
}

// image conversion utility
// incoming image data is planar, and needs interleaving for texture use
// also, some cases require filling in of the alpha channel where no alpha source channel is available
//
static void InterleaveImageData( 
	const void *src_data, int width, int height, int src_channels, Apparance::ImagePrecision::Type src_precision, 
	uint8* dst_data, EPixelFormat dst_format )
{
	//derived
	int bpc = 0;
	int bpp = 0;
	bool pad_alpha = false;
	if(src_precision == Apparance::ImagePrecision::Byte)
	{
		//U8 per pixel
		bpc = sizeof( unsigned char );
		bpp = src_channels * bpc;
	}
	else if(src_precision == Apparance::ImagePrecision::Float)
	{
		//F32 per pixel
		bpc = sizeof( float );
		bpp = src_channels * bpc;
	}
	int npixels = width * height;
	int nbytes = npixels * bpp;
	int planebytes = npixels * bpc;

	//checks
	switch(dst_format)
	{
		case PF_G8:
		case PF_A8:
		case PF_L8:
		case PF_R8_UINT:
			//1 x uint8
			check( bpc == 1 && src_channels == 1 );
			break;
		case PF_R8G8B8A8:
		case PF_R8G8B8A8_UINT:
			//4 x uint8
			check( bpc == 1 && (src_channels == 3 || src_channels == 4) );
			pad_alpha = src_channels==3;
			break;
		case PF_R32_FLOAT:
			//1 x float
			check( bpc == 4 && src_channels == 1 );
			break;
		case PF_A32B32G32R32F:
			//4 x float
			check( bpc == 4 && (src_channels == 3 || src_channels == 4) );
			pad_alpha = src_channels==3;
			break;
		default:
			check( false );
	}

	//copy/interleave/pad
	if(src_precision == Apparance::ImagePrecision::Byte)
	{
		//byte interleave
		const uint8* psrc = (uint8*)src_data;
		uint8* pdst = dst_data;

		//compose
		for(int pix = 0; pix < npixels; pix++)
		{
			for(int cha = 0; cha < src_channels; cha++)
			{
				*pdst++ = psrc[cha * npixels];
			}
			if(pad_alpha)
			{
				*pdst++ = 255;
			}
			psrc++;
		}
	}
	else if(src_precision == Apparance::ImagePrecision::Float)
	{
		//float interleave (binary copied as int)
		const float* psrc = (float*)src_data;
		float* pdst = (float*)dst_data;

		//compose
		for(int pix = 0; pix < npixels; pix++)
		{
			for(int cha = 0; cha < src_channels; cha++)
			{
				*pdst++ = psrc[cha * npixels];
			}
			if(pad_alpha)
			{
				*pdst++ = 1.0f;
			}
			psrc++;
		}
	}	
}

// associate texture data with a dynamic texture resource
// Threading Note: Not called on main thread, can be called from more than one thread at once
//
void FAssetDatabase::SetDynamicAssetTexture( Apparance::AssetID id, const void* pdata, int width, int height, int channels, Apparance::ImagePrecision::Type precision )
{
#if ENABLE_TEXTUREGEN_DIAGS
	UE_LOG( LogApparance, Display, TEXT( "TEXTUREGEN: SetDynamicAssetTexture %i (%i x %i x %i)" ), id, width, height, channels );
#endif

	//derived texure info
	EPixelFormat format = PF_Unknown;
	int src_bpp = 0;
	int bpc = 0;
	int dst_channels = 0;
	if (precision==Apparance::ImagePrecision::Byte)
	{
		//U8 per pixel
		bpc = sizeof(unsigned char);
		src_bpp = channels*bpc;
		if (channels == 1)
		{
			//U8 R
			//PF_A8 or PF_R8_UINT?
			format = PF_A8;
			dst_channels = 1;
		}
		else if (channels == 3)
		{
			//U8 RGB
			//no byte triplets, needs up-conversion to quad bytes (1 alpha)
			format = PF_R8G8B8A8;
			dst_channels = 4;
		}
		else if (channels == 4)
		{
			//U8 RGBA
			//PF_R8G8B8A8 or PF_B8G8R8A8 or PF_R8G8B8A8_UINT?
			format = PF_R8G8B8A8;
			dst_channels = 4;
		}
	}
	else if(precision==Apparance::ImagePrecision::Float)
	{
		//F32 per pixel
		bpc = sizeof(float);
		src_bpp = channels*bpc;
		if (channels == 1)
		{
			//F32 R
			format = PF_R32_FLOAT;	//single float
			dst_channels = 1;
		}
		else if (channels == 3)
		{
			//F32 RGB
			//no float triplets, needs up-conversion to quad floats (1 alpha)
			format = PF_A32B32G32R32F;
			dst_channels = 4;
		}
		else if (channels == 4)
		{
			//F32 RGBA
			format = PF_A32B32G32R32F;	//quad float
			dst_channels = 4;
		}
	}
	check(format != PF_Unknown);
	int dst_bpp = dst_channels*bpc;
	int npixels = width*height;
	int dst_bytes = npixels* dst_bpp;
	int dst_pitch = width*dst_bpp;
	
	//locate existing texture
	UTexture2D** pfoundtexture = nullptr;
	UTexture2D* p = nullptr;
	{
		FScopeLock lock( &DynamicTexturesInterlock );
		pfoundtexture = DynamicTextures.Find( id );
		if(pfoundtexture)
		{
			p = *pfoundtexture;

			//veto any pending retirement
			RetiredTextures.Remove( id );
		}
	}
	bool first_time = p == nullptr;

	//matching texture type?
	if (p)
	{
#if UE_VERSION_AT_LEAST(5,0,0)
		FTexturePlatformData* pd = p->GetPlatformData();
		if (!pd
			|| pd->SizeX != width
			|| pd->SizeY != height
			|| pd->PixelFormat != format
			|| pd->GetNumSlices() != 1
			)
#else //old API
		if(p->GetSizeX() != width
			|| p->GetSizeY() != height
			|| p->GetPixelFormat() != format
			)
#endif
		{
			//needs rebuilding, release
			p->RemoveFromRoot();
			p = nullptr;
		}
	}

	//create/update texture
	if (!p)
	{
		//creation of new objects needs to be deferred to main thread (otherwise can clash with GC)
		FDeferredTextureCreation defer;
		defer.TextureID = id;
		defer.bFirstTime = first_time;

		//format
		defer.PlatformData = new FTexturePlatformData();
		defer.PlatformData->SizeX = width;
		defer.PlatformData->SizeY = height;
		defer.PlatformData->SetNumSlices(1);
		defer.PlatformData->PixelFormat = format;

		//add first mip
		defer.Mip = new FTexture2DMipMap();
		defer.Mip->SizeX = width;
		defer.Mip->SizeY = height;
		
		//lock mip
		defer.Mip->BulkData.Lock(LOCK_READ_WRITE);
		uint8* TextureData = (uint8*)defer.Mip->BulkData.Realloc( dst_bytes );
		//FMemory::Memcpy(TextureData, pdata, nbytes);
		InterleaveImageData( /*in*/ pdata, width, height, channels, precision, /*out*/ TextureData, format );
		defer.Mip->BulkData.Unlock();
	
		//queue up
		{
			FScopeLock lock( &NewTexturesInterlock );
			NewTextures.Add( defer );
		}
	}
	else
	{
		//incremental update
		FUpdateTextureRegion2D* pregion = new FUpdateTextureRegion2D();
		pregion->SrcX = 0;
		pregion->SrcY = 0;
		pregion->DestX = 0;
		pregion->DestY = 0;
		pregion->Width = width;
		pregion->Height = height;

		//convert to texture compatible format
		uint8* pimagedata = new uint8[dst_bytes];
		InterleaveImageData( /*in*/ pdata, width, height, channels, precision, /*out*/ pimagedata, format);
		p->UpdateTextureRegions( 0, 1, pregion, dst_pitch, dst_bpp, (uint8*)pimagedata, []( uint8* pd, const FUpdateTextureRegion2D* pr ) { delete pr; delete[] pd; } );
	}
}


// do a texture creation that has been deferred
//
void FDeferredTextureCreation::Apply( FAssetDatabase* asset_db )
{
	//new texture object
	UTexture2D* p = NewObject<UTexture2D>( GetTransientPackage(), NAME_None, RF_Transient );
	p->AddToRoot();

	{
		FScopeLock lock( &asset_db->DynamicTexturesInterlock );
		asset_db->DynamicTextures.Add( TextureID, p );
	}

#if UE_VERSION_AT_LEAST(5,0,0)
	p->SetPlatformData( PlatformData );
	p->GetPlatformData()->Mips.Add( Mip );
#else //old API
	*p->PlatformData = *PlatformData;
	p->PlatformData->Mips.Add( Mip );
#endif
	//done
	//p->Source.Init(width, height, 1, 1, ETextureSourceFormat::TSF_BGRA8, Pixels);
	p->UpdateResource();

	//notification to update materials using this texture
#if WITH_EDITOR
	//if(bFirstTime)
	{
		FScopeLock lock( &asset_db->ChangedTexturesInterlock );
		asset_db->ChangedTextures.Add( TextureID );
	}
#endif
}

// retire a dynmaic resource that is nolonger needed
// Threading Note: Not called on main thread, can be called from more than one thread at once
//
void FAssetDatabase::RemoveDynamicAsset( Apparance::AssetID id )
{
#if ENABLE_TEXTUREGEN_DIAGS
	UE_LOG(LogApparance, Display, TEXT("TEXTUREGEN: RemoveDynamicAsset %i"), id);
#endif
	FScopeLock lock( &DynamicTexturesInterlock );
	RetiredTextures.Add(id);
}

// purge any dynamic assets that are finished with
//
void FAssetDatabase::PurgeDynamicAssets()
{
	FScopeLock lock( &DynamicTexturesInterlock );

	//release any finished with textures
	if (RetiredTextures.Num() > 0)
	{
		for (Apparance::TextureID id : RetiredTextures)
		{
#if ENABLE_TEXTUREGEN_DIAGS
			UE_LOG(LogApparance, Display, TEXT("TEXTUREGEN: PurgeDynamicAsset %i"), id);
#endif
			UTexture2D** pfoundtexture = DynamicTextures.Find(id);
			if (pfoundtexture)
			{
				//clean up and remove
				UTexture2D* p = *pfoundtexture;
				if (p)
				{
					//done with texture, release holding reference (kept via root)
					p->RemoveFromRoot();
				}

				//remove
				DynamicTextures.Remove(id);
			}
		}
		RetiredTextures.Empty();
	}
}

// do any deferred dynamic asset creation
//
void FAssetDatabase::CreateDynamicAssets()
{
	FScopeLock lock( &NewTexturesInterlock );

	//create any pending texures
	if(NewTextures.Num() > 0)
	{
#if ENABLE_TEXTUREGEN_DIAGS
		UE_LOG(LogApparance, Display, TEXT("TEXTUREGEN: CreateDynamicAssets x %i"), NewTextures.Num());
#endif
		for(int i = 0; i < NewTextures.Num(); i++)
		{
			NewTextures[i].Apply( this );
		}
		NewTextures.Empty();
	}
}

#if WITH_EDITOR

// notifications need to be on main thread
//
void FAssetDatabase::Editor_NotifyChangedTextures()
{
	FScopeLock lock( &ChangedTexturesInterlock );
	for(Apparance::TextureID id : ChangedTextures)
	{
#if ENABLE_TEXTUREGEN_DIAGS
		UE_LOG(LogApparance, Display, TEXT("TEXTUREGEN: Editor_NotifyChangedTexture %i"), id);
#endif
		Editor_NotifyTextureChanged( id );
	}
	ChangedTextures.Empty();
}

// purge dead material monitors
//
void FAssetDatabase::Editor_PurgeMaterialMonitors()
{
	//watch for dead trackers and remove gradually
	for(int i = 0; i < 1; i++)	//may need to do more rapidly, we'll see
	{
		if(MaterialUseTracking.Num() > 0)
		{
			MaterialTrackingCursor++;
			MaterialTrackingCursor = MaterialTrackingCursor % MaterialUseTracking.Num();

			//check
			if(!MaterialUseTracking[MaterialTrackingCursor].IsValid())
			{
				auto& mut = MaterialUseTracking[MaterialTrackingCursor];
#if ENABLE_TEXTUREGEN_DIAGS
				UE_LOG(LogApparance, Display, TEXT("TEXTUREGEN: Editor_PurgeMaterialMonitors all %s : %s : %i params"), (mut.IsValid()?TEXT("Valid"):TEXT("INVALID")), ((mut.Material!=nullptr)?(*mut.Material->GetName()):TEXT("")), ((mut.Parameters.IsValid())?mut.Parameters.Pin()->ID:-1));
#endif

				MaterialUseTracking.RemoveAt( MaterialTrackingCursor );
			}
		}
	}
}

//a texture changed, see if any materials need updating
//
void FAssetDatabase::Editor_NotifyTextureChanged( Apparance::TextureID texture_id )
{
	//scan for materials using this texture
	for(int i = 0; i < MaterialUseTracking.Num(); i++)
	{
		auto& mut = MaterialUseTracking[i];
#if ENABLE_TEXTUREGEN_DIAGS
		UE_LOG(LogApparance, Display, TEXT("TEXTUREGEN: Editor_NotifyTextureChanged all %s : %s : %i params"), (mut.IsValid() ? TEXT("Valid") : TEXT("INVALID")), ((mut.Material != nullptr) ? (*mut.Material->GetName()) : TEXT("")), ((mut.Parameters.IsValid()) ? mut.Parameters.Pin()->ID : -1));
#endif
		MaterialUseTracking[i].NotifyTextureChanged( texture_id );
	}
}

#endif

#if APPARANCE_DEBUGGING_HELP_AssetDatabase
PRAGMA_ENABLE_OPTIMIZATION_ACTUAL
#endif