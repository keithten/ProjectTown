//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

#pragma once


//test/debug
#define TIMESLICE_GEOMETRY_ADD_REMOVE 1
#define LOG_GEOMETRY_ADD_STATS 0
//
#if LOG_GEOMETRY_ADD_STATS
# define GENLOG_INC(a) a++;
# define GENLOG_ACC(a,b) a += b;
#else
# define GENLOG_INC(a)
# define GENLOG_ACC(a,b)
#endif
#if TIMESLICE_GEOMETRY_ADD_REMOVE
extern bool Apparance_IsPendingGeometryAdd();
extern void Apparance_SetTimesliceLimit( float maxMS );
extern int Apparance_GetPendingCount();
#endif

// Apparance API
#include "Apparance.h"
#include "ResourceList/ApparanceResourceListEntry_Material.h"

typedef TMap<Apparance::MaterialID, bool>                                              TCollisionMap;

// profiler stats
DECLARE_STATS_GROUP( TEXT( "Apparance" ), STATGROUP_Apparance, STATCAT_Advanced );
DECLARE_CYCLE_STAT_EXTERN( TEXT( "AddingContent" ), STAT_AddingContent, STATGROUP_Apparance, APPARANCEUNREAL_API );
DECLARE_CYCLE_STAT_EXTERN( TEXT( "RemovingContent" ), STAT_RemovingContent, STATGROUP_Apparance, APPARANCEUNREAL_API );


// distinctly parameterised material info
//
struct FParameterisedMaterial : public TSharedFromThis<FParameterisedMaterial>
{
	//asset
	Apparance::MaterialID                                ID;
	TWeakObjectPtr<UApparanceResourceListEntry_Material> MaterialAsset;
	//instance
	TWeakObjectPtr<class UMaterialInstanceDynamic>       MaterialInstance;
	//parameters
	TSharedPtr<Apparance::IParameterCollection>          Parameters;
	TArray<Apparance::TextureID>                         Textures;
};


typedef TArray<TSharedPtr<FParameterisedMaterial>>                TMaterialList;
typedef TMap<int, TWeakObjectPtr<class UProceduralMeshComponent>> TComponentMap;

// tracking tier specific resources/assets
//
struct FDetailTier : public TSharedFromThis<FDetailTier>
{
	int TierIndex;

	//graphical content
	TMaterialList Materials;	//use to generate visible geometry?
	TCollisionMap Collision;	//use to generate collision to?
	TComponentMap Components;

	//blend parameters
	FLinearColor DetailBlend;

	//material management
	class UMaterialInterface* GetMaterial( class AApparanceEntity* pactor, Apparance::MaterialID material_id, TSharedPtr<Apparance::IParameterCollection> parameters, TArray<Apparance::TextureID>& textures, bool* pwant_collision_out );

};


// Handles view interfacing
//
class FEntityView : public Apparance::Host::IEntityView
{
	class FEntityRendering* m_pEntityRendering;

public:
	FEntityView( FEntityRendering* per );
	virtual ~FEntityView();

	//~ Begin Apparance IEntityRendering Interface
	Apparance::Vector3 GetPosition() const;
	void               SetDetailRange( int tier_index, float near0, float near1, float far1, float far0 );
	//~ End Apparance IEntityRendering Interface

};


// Handles associated entity and rendering setup for it
//
class FEntityRendering : public Apparance::Host::IEntityRendering
{
	// cached
	class AApparanceEntity*		m_pActor;

	// apparance
	Apparance::IEntity*			m_pEntity;
	//int							m_NextGeometryID;
	Apparance::IClosure*		m_pDeferredProcedure;
	TSharedPtr<struct FRateLimiter>	m_pRateLimiter;

	// view info
	FEntityView                 m_View;

	// tiers
	TArray<TSharedPtr<FDetailTier>> m_Tiers;
	FDetailTier                     m_EditingTier;

	//interactive editing
	Apparance::IParameterCollection* m_pEditingParameters;

public:
	FEntityRendering();
	virtual ~FEntityRendering();

	//setup
	void SetOwner( class AApparanceEntity* powner );
	void SetProcedure( Apparance::IClosure* pclosure );
	
	//operations
	void Tick( float DeltaSeconds );
	void Clear();

	//~ Begin Apparance IEntityRendering Interface
	virtual Apparance::GeometryID AddGeometry( Apparance::Host::IGeometry* geometry, int tier_index, Apparance::Vector3 offset, int request_id );
	virtual void                  RemoveGeometry( Apparance::GeometryID geometry_id );
	//support rendering of diagnostics primitives
	virtual Apparance::Host::IDiagnosticsRendering* CreateDiagnosticsRendering();
	virtual void DestroyDiagnosticsRendering( Apparance::Host::IDiagnosticsRendering* p );
	//interactive editing support
	virtual void NotifyBeginEditing();
	virtual void ModifyWorldTransform( Apparance::Vector3 world_origin, Apparance::Vector3 world_unit_right, Apparance::Vector3 world_unit_forward );
	virtual void NotifyEndEditing( Apparance::IParameterCollection* final_parameter_patch );
	virtual void StoreParameters( Apparance::IParameterCollection* parameter_patch ); //update parameters to persist final changes, but don't trigger a rebuild
	//~ End Apparance IEntityRendering Interface

	//access
	struct FDetailTier* GetTier( int tier_index );
	class UMaterialInterface* GetMaterial( Apparance::MaterialID material, TSharedPtr<Apparance::IParameterCollection> parameters, TArray<Apparance::TextureID>& textures, int tier_index, bool* pwant_collision_out=nullptr );
	class AApparanceEntity* GetActor() { return m_pActor; }
	Apparance::IEntity* GetEntityAPI() { return m_pEntity; }

	//testing deferred add/remove
	static int m_NextGeometryID;
	Apparance::GeometryID AddGeometry_Deferred( Apparance::Host::IGeometry* geometry, int tier_index, Apparance::Vector3 offset, int request_id );
	void                  RemoveGeometry_Deferred( Apparance::GeometryID geometry_id );

private:

	void InvalidateMaterials();
	void RemoveContent( Apparance::GeometryID geometry_id );
	void RemoveAllContent();
	void TriggerBuild( Apparance::IClosure* proc );

	friend class AApparanceEntity;


};
