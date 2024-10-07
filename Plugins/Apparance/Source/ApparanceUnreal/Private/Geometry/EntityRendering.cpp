//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

//local debugging help
#define APPARANCE_DEBUGGING_HELP_EntityRendering 0
#if APPARANCE_DEBUGGING_HELP_EntityRendering
PRAGMA_DISABLE_OPTIMIZATION_ACTUAL
#endif

// main
#include "EntityRendering.h"


// unreal
#include "Materials/MaterialInstanceDynamic.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "ProceduralMeshComponent.h"
#include "ProfilingDebugging/ScopedTimers.h"
#include "Engine/BlueprintGeneratedClass.h"

// module
#include "ApparanceUnreal.h"
#include "ApparanceEntity.h"
#include "Geometry.h"
#include "AssetDatabase.h"
#include "ApparanceParametersComponent.h"
#include "ApparanceEngineSetup.h"
#include "Utility/RateLimiter.h"

//std
#include <list>

DEFINE_STAT( STAT_AddingContent );
DEFINE_STAT( STAT_RemovingContent );


//////////////////////////////////////////////////////////////////////////
// FEntityDiagnosticsRendering

class FEntityDiagnosticsRendering : public Apparance::Host::IDiagnosticsRendering
{
	FEntityRendering* m_pEntityRendering;

public:
	FEntityDiagnosticsRendering( FEntityRendering* per )
		: m_pEntityRendering( per )
	{}

	// Begin IDiagnosticsRendering
	virtual void Begin() {}
	virtual void DrawLine( Apparance::Vector3 start, Apparance::Vector3 end, Apparance::Colour colour )
	{
		FVector position1 = UNREALSPACE_FROM_APPARANCESPACE( start );
		FVector position2 = UNREALSPACE_FROM_APPARANCESPACE( end );
		
		//actor world space tranform
		FMatrix world_transform = m_pEntityRendering->GetActor()->GetTransform().ToMatrixWithScale();
		position1 = FVector( world_transform.TransformPosition( position1 ) );
		position2 = FVector( world_transform.TransformPosition( position2 ) );

		UWorld* pworld = m_pEntityRendering->GetActor()->GetWorld();	
		DrawDebugLine( pworld, position1, position2, UNREALCOLOR_FROM_APPARANCECOLOUR( colour ) );
	}
	virtual void DrawBox( Apparance::Frame frame, Apparance::Colour colour )
	{
#if 1
		FMatrix transform_rst;
		FMatrix transform_r;
		UnrealTransformsFromApparanceFrame(frame, m_pEntityRendering->GetActor(), &transform_rst, &transform_r);

		//not tested, but replaces this...
#else
		FVector local_pos = UNREALSPACE_FROM_APPARANCESPACE( frame.Origin );
		FVector local_size = UNREALSPACE_FROM_APPARANCESPACE( frame.Size );
		FVector local_axisx = UNREALHANDEDNESS_FROM_APPARANCEHANDEDNESS( frame.Orientation.Y );
		FVector local_axisy = UNREALHANDEDNESS_FROM_APPARANCEHANDEDNESS( frame.Orientation.X );
		FVector local_axisz = UNREALHANDEDNESS_FROM_APPARANCEHANDEDNESS( frame.Orientation.Z );

		//actor local space transform
		FScaleMatrix local_scaling( local_size );
		FMatrix local_rotation = FRotationMatrix::MakeFromXY( local_axisx, local_axisy );
		FTranslationMatrix local_translation( local_pos );
		//actor world space tranform
		FMatrix world_transform = m_pEntityRendering->GetActor()->GetTransform().ToMatrixWithScale();
		FRotationMatrix world_rotation = FRotationMatrix( m_pEntityRendering->GetActor()->GetActorRotation() );

		//combine
		FMatrix transform_rst = local_scaling * local_rotation * local_translation * world_transform;
		FMatrix transform_r = local_rotation * world_rotation;
#endif

		FVector origin = transform_rst.GetOrigin();
		FVector size = transform_rst.GetScaleVector();
		FVector extent = size*0.5f;
		FVector extent_rot = FVector( transform_r.TransformPosition( extent ) );
		FQuat rotation( transform_r );
		FColor c = UNREALCOLOR_FROM_APPARANCECOLOUR( colour );

		UWorld* pworld = m_pEntityRendering->GetActor()->GetWorld();
		DrawDebugBox( pworld, origin + extent_rot, extent, rotation, c );
	}
	virtual void DrawSphere( Apparance::Vector3 centre, float radius, Apparance::Colour colour )
	{
		FVector unreal_centre = UNREALSPACE_FROM_APPARANCESPACE( centre );
		float unreal_radius = UNREALSCALE_FROM_APPARANCESCALE( radius );

		//actor world space tranform
		FMatrix world_transform = m_pEntityRendering->GetActor()->GetTransform().ToMatrixWithScale();
		unreal_centre = FVector( world_transform.TransformPosition( unreal_centre ) );
		unreal_radius *= m_pEntityRendering->GetActor()->GetActorScale().X;	//non-uniform spheres not supported

		UWorld* pworld = m_pEntityRendering->GetActor()->GetWorld();	
		DrawDebugSphere( pworld, unreal_centre, unreal_radius, 32, UNREALCOLOR_FROM_APPARANCECOLOUR( colour ) );
	}
	virtual void End() {}
	// End IDiagnosticsRendering
};


//shared, global id gen
int FEntityRendering::m_NextGeometryID = Apparance::InvalidID + 1;

//////////////////////////////////////////////////////////////////////////
// FEntityRendering

FEntityRendering::FEntityRendering()
	: m_pActor( nullptr )
	, m_pEntity( nullptr )
	, m_pDeferredProcedure( nullptr )
	, m_View( this )
	, m_pEditingParameters( nullptr )
{
#if WITH_EDITOR
	m_pRateLimiter = MakeShareable( new FRateLimiter() );
	m_pRateLimiter->Init( 32 );
#endif
}

#if TIMESLICE_GEOMETRY_ADD_REMOVE
void Apparance_NotifyEntityRenderingDelete( FEntityRendering* per );
#endif

FEntityRendering::~FEntityRendering()
{
	delete m_pDeferredProcedure;
#if TIMESLICE_GEOMETRY_ADD_REMOVE
	Apparance_NotifyEntityRenderingDelete( this );
#endif
}

// Init
//
void FEntityRendering::SetOwner( AApparanceEntity* powner )
{
	if (FApparanceUnrealModule::GetEngine())
	{
		if (powner)
		{
			//setup
			check(m_pActor == nullptr);
			m_pActor = powner;

			check( m_pEntity == nullptr );

			//create us an apparance entity
			bool is_play_mode = FApparanceUnrealModule::GetModule()->IsGameRunning();
			FString world_name = FApparanceUnrealModule::GetModule()->MakeWorldIdentifier( powner );
			FString entity_debug_name = powner->GetName();
			m_pEntity = FApparanceUnrealModule::GetEngine()->CreateEntity( this, is_play_mode, (const char*)StringCast<UTF8CHAR>(*world_name).Get(), (const char*)StringCast<UTF8CHAR>(*entity_debug_name).Get() );

			//ensure new set up for smart editing
			if (FApparanceUnrealModule::GetModule()->IsEditingEnabled( powner->GetWorld() ))
			{
				m_pEntity->EnableSmartEditing();
			}

//			UE_LOG( LogApparance, Log, TEXT( "Entity Actor %p, Apparance Entity %p : Created for Entity Rendering %p" ), m_pActor, m_pEntity, this );
		}
		else
		{
			//shutdown (if needed)
			if (m_pActor || m_pEntity)
			{
//				UE_LOG( LogApparance, Log, TEXT( "Entity Actor %p, Apparance Entity %p : Destroyed for Entity Rendering %p" ), m_pActor, m_pEntity, this );

				//no-longer want entity
				FApparanceUnrealModule::GetEngine()->DestroyEntity( m_pEntity );
				m_pEntity = nullptr;
				m_pActor = nullptr;
			}
		}
	}
}

void FEntityRendering::SetProcedure( Apparance::IClosure* proc )
{
	if(ensure( proc!=nullptr ) && m_pEntity)
	{
		//defer for a bit?
		if(m_pRateLimiter.IsValid() && m_pRateLimiter->CheckDefer())
		{
			//store proc request for later
			delete m_pDeferredProcedure; //(replace any still waiting)
			m_pDeferredProcedure = proc;
		}
		else
		{
			//trigger build now
			TriggerBuild( proc );

			//ensure to remove any old deferred
			delete m_pDeferredProcedure;
			m_pDeferredProcedure = nullptr;
		}
	}
	else
	{
		//not used, discard
		delete proc;
	}
}

// actually request that the entity rebuilds it's content now
//
void FEntityRendering::TriggerBuild( Apparance::IClosure* proc )
{
	int request_id = m_pEntity->Build( proc, false );
	if(m_pRateLimiter.IsValid())
	{
		m_pRateLimiter->Begin( request_id );
	}

	//change of proc/rebuild invalidates cached materials
	InvalidateMaterials();
}

// Updates
//
void FEntityRendering::Tick( float DeltaSeconds )
{
	if(m_pEntity)
	{
		//deferred updates
		if(m_pRateLimiter.IsValid())
		{
			if(m_pRateLimiter->CheckDispatch( DeltaSeconds ))
			{
				if(m_pDeferredProcedure)
				{					
					TriggerBuild( m_pDeferredProcedure );
					m_pDeferredProcedure = nullptr;
				}
			}
		}

		//TODO: use actual viewports
		struct Apparance::Host::IEntityView* views[1] = { &m_View };
		m_pEntity->Update( views, 1, DeltaSeconds );
	}
}

// clear out and remove all generated content, fresh start for re-gen
// e.g. needed pre-undo to ensure nothing interferes with the undo/redo process leaving cruft (orphaned blueprints)
//
void FEntityRendering::Clear()
{
	//reset components cached in tier structure and actor
	RemoveAllContent();
}


//enable logging of content generated during play
#if LOG_GEOMETRY_ADD_STATS

int nGenLogFrameCounter;
double nGenLogDuration;
int nGenLogAdd;
int nGenLogRemove;
int nGenLogParts;
int nGenLogTriangles;
int nGenLogVertices;
int nGenLogCollisionParts;
int nGenLogCollisionTriangles;
int nGenLogCollisionVertices;
int nGenLogMeshes;
int nGenLogInstances;
int nGenLogBlueprints;
int nGenLogComponents;
FILE* pGenLogFile = nullptr;
void ResetGenLog()
{
	nGenLogDuration = 0;
	nGenLogAdd = 0;
	nGenLogRemove = 0;
	nGenLogParts = 0;
	nGenLogTriangles = 0;
	nGenLogVertices = 0;
	nGenLogCollisionParts = 0;
	nGenLogCollisionTriangles = 0;
	nGenLogCollisionVertices = 0;
	nGenLogMeshes = 0;
	nGenLogInstances = 0;
	nGenLogBlueprints = 0;
	nGenLogComponents = 0;
}
void Apparance_StartGenLog()
{
	fopen_s( &pGenLogFile, "C:\\temp\\Salix\\gen_log.csv", "w" );
	if(pGenLogFile)
	{
		fprintf( pGenLogFile, "Frame,Time,Adds,Removes,Parts,Tris,Verts,CParts,CTris,CVerts,Meshes,Instances,BPs,Comps\n" );
		nGenLogFrameCounter = 0;
		ResetGenLog();
	}
}
void Apparance_StopGenLog()
{
	fclose( pGenLogFile );
	pGenLogFile = nullptr;
}
void Apparance_TickGenLog()
{
	nGenLogFrameCounter++;
	if(pGenLogFile)
	{
		fprintf( pGenLogFile, "%i,%.3f,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i\n",
			nGenLogFrameCounter,
			(float)nGenLogDuration,
			nGenLogAdd,
			nGenLogRemove,
			nGenLogParts,
			nGenLogTriangles,
			nGenLogVertices,
			nGenLogCollisionParts,
			nGenLogCollisionTriangles,
			nGenLogCollisionVertices,
			nGenLogMeshes,
			nGenLogInstances,
			nGenLogBlueprints,
			nGenLogComponents
		);
		fflush( pGenLogFile );

		ResetGenLog();

		if(!FApparanceUnrealModule::GetModule()->IsGameRunning())
		{
			Apparance_StopGenLog();
		}
	}
	else
	{
		if(FApparanceUnrealModule::GetModule()->IsGameRunning())
		{
			Apparance_StartGenLog();
		}
	}
}
#else
void Apparance_TickGenLog()
{}
#endif


#if TIMESLICE_GEOMETRY_ADD_REMOVE
struct TimesliceRecord
{
	bool IsAdd;
	//add/remove
	FEntityRendering* EntityRendering;
	int  GeometryId;
	//add
	struct Apparance::Host::IGeometry* Geometry;
	int TierIndex;
	Apparance::Vector3 Offset;
	int RequestId;
};
std::list<TimesliceRecord*> Timeslicer;

bool bPendingGeometry = false;
double dGeomTime = 0;
FDurationTimer geomTimer( dGeomTime );
double dMaxTimesliceTime = 0;

void NotifyPendingState( bool pending )
{
	bPendingGeometry = pending;
	if(bPendingGeometry)
	{
		//UE_LOG( LogApparance, Display, TEXT( "++++++++ APPARANCE: BUSY! ++++++++" ) );
		dGeomTime = 0;
		geomTimer.Start();
	}
	else
	{
		geomTimer.Stop();
		//UE_LOG( LogApparance, Display, TEXT( "-------- APPARANCE: DONE (%.3f s)--------" ), (float)dGeomTime );
	}
}


#endif


// New geometry 
//
Apparance::GeometryID FEntityRendering::AddGeometry( struct Apparance::Host::IGeometry* geometry, int tier_index, Apparance::Vector3 offset, int request_id )
{
#if TIMESLICE_GEOMETRY_ADD_REMOVE
	if(Timeslicer.empty())
	{
		//if not busy, can do immediately
		return AddGeometry_Deferred( geometry, tier_index, offset, request_id );
	}
	else
	{
		int id = m_NextGeometryID++;
		//deferred add
		TimesliceRecord* pr = new TimesliceRecord();
		pr->EntityRendering = this;
		pr->IsAdd = true;
		pr->GeometryId = id;
		pr->Geometry = geometry;
		pr->TierIndex = tier_index;
		pr->Offset = offset;
		pr->RequestId = request_id;
		if(Timeslicer.empty())
		{
			NotifyPendingState( true );
	}
		Timeslicer.push_back( pr );
		return Apparance::GeometryID( id );
}
#else
	//immediate pass-through
	return AddGeometry_Deferred( geometry, tier_index, offset, request_id );
#endif
}

#if TIMESLICE_GEOMETRY_ADD_REMOVE
void Apparance_NotifyEntityRenderingDelete( FEntityRendering* per )
{
	//must clear any pending geometry for this er
	for(std::list<TimesliceRecord*>::iterator it = Timeslicer.begin(); it != Timeslicer.end(); ++it)
	{
		const TimesliceRecord* pr = *it;
		if(pr->EntityRendering==per)
		{
			//remove instead of deferring a remove
			delete pr;
			Timeslicer.erase( it );
			return;
		}
	}
}

bool Apparance_IsPendingGeometryAdd()
{
	return bPendingGeometry;
}

int Apparance_GetPendingCount()
{
	return (int)Timeslicer.size();
}

void Apparance_SetTimesliceLimit(float maxMS)
{
	dMaxTimesliceTime = maxMS/1000.0f;	//s
}

bool Apparance_NotifyGeometryDestruction( struct Apparance::Host::IGeometry* pgeometry )
{
	for(std::list<TimesliceRecord*>::iterator it = Timeslicer.begin(); it != Timeslicer.end(); ++it)
	{
		const TimesliceRecord* pr = *it;
		if(pr->Geometry == pgeometry && pr->IsAdd)
		{
			Timeslicer.erase( it );
			return true;
		}
	}
	return false;
}

#endif

void FEntityRendering::RemoveGeometry( Apparance::GeometryID geometry_id )
{
#if TIMESLICE_GEOMETRY_ADD_REMOVE
	//corresponding add still pending?
	for(std::list<TimesliceRecord*>::iterator it=Timeslicer.begin() ; it!=Timeslicer.end() ; ++it)
	{
		const TimesliceRecord* pr = *it;
		if(pr->GeometryId == geometry_id && pr->IsAdd)
		{
			//remove instead of deferring a remove
			delete pr;
			Timeslicer.erase( it );
			return;
		}
	}
#endif

#if 0//IMMEDIATE REMOVE for now TIMESLICE_GEOMETRY_ADD_REMOVE
	//deferred remove
	TimesliceRecord* pr = new TimesliceRecord();
	pr->EntityRendering = this;
	pr->IsAdd = false;
	pr->GeometryId = (int)geometry_id;
	if(Timeslicer.empty())
	{
		NotifyPendingState( true );
	}
	Timeslicer.push_back( pr );
#else
	//immediate pass-through
	RemoveGeometry_Deferred( geometry_id );
#endif
}

void Apparance_TickTimeslicedGeometryHandling()
{
#if TIMESLICE_GEOMETRY_ADD_REMOVE
	if(!Timeslicer.empty())
	{
		const double start_time = FPlatformTime::Seconds();
		do {
			//pop next
			TimesliceRecord* pr = Timeslicer.front();
			Timeslicer.pop_front();
			if(Timeslicer.empty())
			{
				NotifyPendingState( false );
			}

			if(pr->EntityRendering->GetActor()) //still in use?
			{
				if(pr->IsAdd)
				{
					//add
					const int preserve_id = FEntityRendering::m_NextGeometryID;
					FEntityRendering::m_NextGeometryID = pr->GeometryId; //ensure use id originally returned
					pr->EntityRendering->AddGeometry_Deferred( pr->Geometry, pr->TierIndex, pr->Offset, pr->RequestId );
					FEntityRendering::m_NextGeometryID = preserve_id;
				}
				else
				{
					//remove
					pr->EntityRendering->RemoveGeometry_Deferred( pr->GeometryId );
				}
			}
			delete pr;

			//cleared all?
			if(Timeslicer.empty())
			{
				break;
			}
			//or spend too long?
		} while(((FPlatformTime::Seconds() - start_time)) < dMaxTimesliceTime);
	}
#endif
}


// New geometry 
//
Apparance::GeometryID FEntityRendering::AddGeometry_Deferred( struct Apparance::Host::IGeometry* geometry, int tier_index, Apparance::Vector3 offset, int request_id )
{
	SCOPE_CYCLE_COUNTER( STAT_AddingContent );
	GENLOG_INC(nGenLogAdd)
#if LOG_GEOMETRY_ADD_STATS
	FScopedDurationTimer timer( nGenLogDuration );
#endif

	int id = m_NextGeometryID++;
	//UE_LOG( LogApparance, Log, TEXT("Apparance Entity %p : AddGeometry( %p, %i, %f,%f,%f ) = %i"), m_pEntity, geometry, tier_index, offset.X, offset.Y, offset.Z, id );

	FApparanceGeometry* pmygeometry = (FApparanceGeometry*)geometry; //upcast to known internal type

	//---- GEOMETRY ----
	m_pActor->BeginGeometryUpdate();

	//ensure tier

	//any triangle geometry?
	FVector unreal_offset = UNREALSPACE_FROM_APPARANCESPACE( offset );
	if(geometry->GetPartCount()>0)
	{
		//get actor to generate component to host geometry
		class UProceduralMeshComponent* pcollisioncomponent = nullptr;
		class UProceduralMeshComponent* pgeometrycomponent = m_pActor->AddGeometry( geometry, tier_index, unreal_offset, pcollisioncomponent );

		//add to component lookup
		if(pgeometrycomponent)
		{
			m_pActor->GeometryCache.Add( id, pgeometrycomponent );
		}
		if(pcollisioncomponent)
		{
			m_pActor->CollisionCache.Add( id, pcollisioncomponent );
		}

		//add to tier (by ID)
		if(pgeometrycomponent)
		{
			FDetailTier* ptier = GetTier( tier_index );
			ptier->Components.Add( id, pgeometrycomponent );
		}
	}

	//---- MESHES/BLUEPRINTS ----
	auto& object_list = pmygeometry->GetObjects();
	for (int i = 0; i < object_list.Num(); i++)
	{
		Apparance::ObjectID object_type = object_list[i].ID;
		Apparance::IParameterCollection* placement_parameters = object_list[i].Parameters;

		//resolve type to resource entry
		const UApparanceResourceListEntry* presource = nullptr;
		FApparanceUnrealModule::GetAssetDatabase()->GetObject(object_type, presource);
		const UApparanceResourceListEntry_3DAsset* p3dresource = Cast<const UApparanceResourceListEntry_3DAsset>( presource );
		const UApparanceResourceListEntry_Component* pcomponentresource = Cast<const UApparanceResourceListEntry_Component>( presource );
		const UApparanceResourceListEntry_StaticMesh* pstaticmeshresource = Cast<const UApparanceResourceListEntry_StaticMesh>( presource );
		const UApparanceResourceListEntry_Blueprint* pblueprintresource = Cast<const UApparanceResourceListEntry_Blueprint>( presource );
		
		//extract information about what type of object we are placing
		const UApparanceResourceListEntry_Component* pcomponent_template = pcomponentresource;
		UStaticMesh* pbasemesh = pstaticmeshresource?pstaticmeshresource->GetMesh():nullptr;
		UBlueprintGeneratedClass* pblueprintclass = pblueprintresource?pblueprintresource->GetBlueprintClass():nullptr;
		const FMatrix assettransform_normalised = p3dresource?p3dresource->GetAssetTransform( true ):FMatrix::Identity;
		const FMatrix assettransform_noscale = p3dresource?p3dresource->GetAssetTransform( false ):FMatrix::Identity;
		
		//unresolved resource? show as fallback mesh
		if(!presource)
		{
			pbasemesh = FApparanceUnrealModule::GetModule()->GetFallbackMesh();
		}

		//determine placement of mesh
		FVector translation = FVector::ZeroVector;
		FVector scale = FVector::OneVector;
		bool use_normalised_asset = true;
		FMatrix rotationtransform = FMatrix::Identity;
		if(p3dresource)
		{
			placement_parameters->BeginAccess();

			//obtain placement position
			int offset_parameter_id = p3dresource->PlacementOffsetParameter;
			if (offset_parameter_id != 0)
			{
				int parameter_index = 0;
				if (p3dresource->FindParameterInfo(offset_parameter_id, &parameter_index))
				{
					//attempt frame
					Apparance::Frame f;
					if (UApparanceResourceListEntry_Component::GetFrameParameter(placement_parameters, parameter_index, f))
					{
						//get centre of placement frame
						ApparanceFrameOriginAdjust(f, EApparanceFrameOrigin::Centre, true);
						translation = UNREALSPACE_FROM_APPARANCESPACE(f.Origin);					
					}
					else
					{
						//attempt vector						
						translation = UApparanceResourceListEntry_Component::GetFVectorParameter(placement_parameters, parameter_index);
					}
				}
			}
			else
			{
				translation = p3dresource->CustomPlacementOffset;
			}

			//obtain placement scale
			int scale_parameter_id = p3dresource->PlacementScaleParameter;
			bool is_parameter_scale = false;
			if (scale_parameter_id != 0)
			{
				is_parameter_scale = true;
				int parameter_index = 0;
				if (p3dresource->FindParameterInfo(offset_parameter_id, &parameter_index))
				{
					//attempt frame
					Apparance::Frame f;
					if (UApparanceResourceListEntry_Component::GetFrameParameter(placement_parameters, parameter_index, f))
					{
						//use size
						scale = UNREALHANDEDNESS_FROM_APPARANCEHANDEDNESS(f.Size);
					}
					else
					{
						//attempt vector
						Apparance::Vector3 v = UApparanceResourceListEntry_Component::GetVector3Parameter(placement_parameters, parameter_index);
						scale = UNREALHANDEDNESS_FROM_APPARANCEHANDEDNESS( v );
					}
				}
			}
			else
			{
				scale = p3dresource->CustomPlacementScale;
			}
			//relative to what?
			switch (p3dresource->PlacementSizeMode)
			{
				case EApparanceSizeMode::Size:
					//scale up to world size
					if (is_parameter_scale)
					{
						//incoming parameters are in apparance space
						scale = FVECTOR_UNREALSCALE_FROM_APPARANCESCALE(scale);
					}
					use_normalised_asset = true;					
					break;
				case EApparanceSizeMode::Scale:
				{
					//leave as purely scaling factor
					use_normalised_asset = false;
					break;
				}
			}
					
			//obtain placement rotation
			int rotation_parameter_id = p3dresource->PlacementRotationParameter;
			if (rotation_parameter_id != 0)
			{
				int parameter_index = 0;
				if (p3dresource->FindParameterInfo(offset_parameter_id, &parameter_index))
				{
					//attempt frame
					Apparance::Frame f;
					if (UApparanceResourceListEntry_Component::GetFrameParameter(placement_parameters, parameter_index, f))
					{
						//use size
						FMatrix m = FMatrix::Identity;
						FVector xaxis = UNREALHANDEDNESS_FROM_APPARANCEHANDEDNESS(f.Orientation.Y); //also swap XY axes for conversion of spaces
						FVector yaxis = UNREALHANDEDNESS_FROM_APPARANCEHANDEDNESS(f.Orientation.X);
						FVector zaxis = UNREALHANDEDNESS_FROM_APPARANCEHANDEDNESS(f.Orientation.Z);
						m.SetAxes(&xaxis, &yaxis, &zaxis);
						rotationtransform = m;
					}
					else
					{
						//attempt vector (Euler)
						Apparance::Vector3 v = UApparanceResourceListEntry_Component::GetVector3Parameter(placement_parameters, parameter_index, false );
						FRotator r(v.X, v.Z, v.Y);	//pitch x, yaw z, roll y (apparance space)
						rotationtransform = FRotationMatrix::Make(r);						
					}
				}
			}
			else
			{
				FVector v = p3dresource->CustomPlacementRotation;
				FRotator r(v.Y, v.Z, v.X);	//pitch y, yaw z, roll x (unreal space)
				rotationtransform = FRotationMatrix::Make(r);						
			}			

			placement_parameters->EndAccess();
		}

		//frame -> transform
		FMatrix offsettransform = FTranslationMatrix::Make(translation);
		FMatrix scaletransform = FScaleMatrix::Make(scale);
		FMatrix placementtransform = scaletransform * rotationtransform * offsettransform;

		//asset transform
		const FMatrix& assettransform = use_normalised_asset?assettransform_normalised:assettransform_noscale;
		
		//compose final transform
		FMatrix objecttransform = assettransform * placementtransform;

		//create object
		if (pblueprintclass)
		{
			GENLOG_INC(nGenLogBlueprints)
			//are we placing another procedural object?
			AApparanceEntity* ptemplate_entity = pblueprintclass?Cast<AApparanceEntity>( pblueprintclass->GetDefaultObject() ):nullptr;
			bool is_proc_object = ptemplate_entity != nullptr;

			//transform for placed bp
			FMatrix& bp_transform = objecttransform;
			if(is_proc_object)
			{
				//assumption here is that placing a procedural object it will use the frame internally to generate it's content
				//and doesn't want any offset relative parent
				//TODO: review this because it's not ideal for large scenes as accuracy will suffer at distance
				//better is to offset the actor and have its generated content (and passed frame) offset to the location of the content
				//this needs to be optional though as it makes assumptions about the frame parameter
				//...or does it, procedurally placed objects always have a frame as first param...
				//thought needed
				
//TEST			bp_transform = FMatrix::Identity;
			}

			//blueprints aren't scaled, this just messes with any built in generation/calculations
			AActor* pactor = m_pActor->AddBlueprint_Begin( pblueprintclass, bp_transform );
			if (pactor)
			{
				//ensure place to store meshes
				FActorCacheEntry* pactorcacheentry = m_pActor->ActorCache.Find(id);
				if (!pactorcacheentry)
				{
					pactorcacheentry = &m_pActor->ActorCache.Emplace( id );
				}
				pactorcacheentry->Actors.Add(pactor);

				//is entity?
				AApparanceEntity* pentity = Cast<AApparanceEntity>(pactor);
				if (pentity)
				{
					pentity->MarkAsProcedurallyPlaced();
				}

				//inject placement paramters for entities
				if(pentity && ptemplate_entity)
				{
					//look up parameters blueprint is expecting
					Apparance::ProcedureID proc_id = ptemplate_entity->ProcedureID;
					const Apparance::ProcedureSpec* proc_spec = FApparanceUnrealModule::GetLibrary()->FindProcedureSpec( proc_id );
					const Apparance::IParameterCollection* spec_params = proc_spec?proc_spec->Inputs:nullptr;
					if (spec_params)
					{
						//we need this as (currently) incoming placement parameters don't have any ID info with them (list a list)
						//TODO: placement parameters could do with having IDs too, but this requires a bunch of editor support (typed lists/structures/interfaces)
						int num_s_params = spec_params->BeginAccess();

						//the incoming params
						int num_p_params = placement_parameters->BeginAccess();

						//scan props to make sure the types match
						int n = (num_p_params < num_s_params)?num_p_params:num_s_params;
						for (int j = 0; j < n; j++)
						{
							//from
							Apparance::Parameter::Type src_type = spec_params->GetType(j);
							
							//to
							Apparance::Parameter::Type dst_type = placement_parameters->GetType(j);

							if (src_type != dst_type)
							{
								UE_LOG(LogApparance, Error, TEXT("Placement parameter %i type mismatch, expected %s, got %s for procedure '%s.%s' of '%s'"), j, ApparanceParameterTypeName(dst_type), ApparanceParameterTypeName(src_type), UTF8_TO_TCHAR( proc_spec->Category ), UTF8_TO_TCHAR( proc_spec->Name ), *m_pActor->GetName() );
								//truncate update at first bad prop
								n = j;
								break;
							}
						}
						placement_parameters->EndAccess();
						spec_params->EndAccess(); //need to release as entity needs access below during unpack
						
						//apply value
						Apparance::IParameterCollection* ent_params = pentity->BeginEditingParameters();
						placement_parameters->BeginAccess();
						int num_params = spec_params->BeginAccess();
						for (int j = 0; j < num_params; j++)
						{
							//gather id for param (from spec)
							Apparance::ValueID param_id = spec_params->GetID(j);
							Apparance::Parameter::Type param_type = spec_params->GetType(j);

							//apply							
							switch (param_type)
							{
								case Apparance::Parameter::Integer:
								{
									int value = 0;
									if (placement_parameters->GetInteger( j, &value))
									{
										if (!ent_params->ModifyInteger(param_id, value))
										{
											int index = ent_params->AddParameter(param_type, param_id);
											ent_params->SetInteger(index, value);
										}
									}
									break;
								}
								case Apparance::Parameter::Float:
								{
									float value = 0;
									if (placement_parameters->GetFloat( j, &value))
									{
										if (!ent_params->ModifyFloat(param_id, value))
										{
											int index = ent_params->AddParameter(param_type, param_id);
											ent_params->SetFloat(index, value);
										}
									}
									break;
								}
								case Apparance::Parameter::Bool:
								{
									bool value = false;
									if (placement_parameters->GetBool( j, &value))
									{
										if (!ent_params->ModifyBool(param_id, value))
										{
											int index = ent_params->AddParameter(param_type, param_id);
											ent_params->SetBool(index, value);
										}
									}
									break;
								}
								case Apparance::Parameter::Frame:
								{
									Apparance::Frame value;
									value.Size = Apparance::Vector3(1, 1, 1);
									if (placement_parameters->GetFrame( j, &value))
									{
										//remapping needed for placement frame (first one)
										if(j == 0)
										{
											//As we are placing the BP according to the incoming frame, the forwarded frame should be relative to that, i.e. same size, centred, but no translation/rotation
											value.Orientation.X = Apparance::Vector3( 1, 0, 0 );
											value.Orientation.Y = Apparance::Vector3( 0, 1, 0 );
											value.Orientation.Z = Apparance::Vector3( 0, 0, 1 );
											value.Origin.X = -value.Size.X * 0.5f;
											value.Origin.Y = -value.Size.Y * 0.5f;
											value.Origin.Z = -value.Size.Z * 0.5f;

											//must use frame/bounds as-is
											pentity->DisableParameterMapping();
										}

										//set
										if (!ent_params->ModifyFrame(param_id, &value))
										{
											int index = ent_params->AddParameter(param_type, param_id);
											ent_params->SetFrame(index, &value);
										}
									}
									break;
								}
								case Apparance::Parameter::Vector3:
								{
									Apparance::Vector3 value;
									if (placement_parameters->GetVector3( j, &value))
									{
										if (!ent_params->ModifyVector3(param_id, &value))
										{
											int index = ent_params->AddParameter(param_type, param_id);
											ent_params->SetVector3(index, &value);
										}
									}
									break;
								}
								case Apparance::Parameter::Colour: 
								{
									Apparance::Colour value;
									if (placement_parameters->GetColour( j, &value))
									{
										if (!ent_params->ModifyColour(param_id, &value))
										{
											int index = ent_params->AddParameter(param_type, param_id);
											ent_params->SetColour(index, &value);
										}
									}
									break;
								}
								case Apparance::Parameter::String: 
								{
									FString value;
									int num_chars = 0;
									TCHAR* pbuffer = nullptr;
									if (placement_parameters->GetString( j, 0, nullptr, &num_chars))
									{
										value = FString::ChrN(num_chars, TCHAR(' '));
										pbuffer = value.GetCharArray().GetData();
										placement_parameters->GetString( j, num_chars, pbuffer);
									}
									if(pbuffer)
									{
										if (!ent_params->ModifyString(param_id, num_chars, pbuffer))
										{
											int index = ent_params->AddParameter(param_type, param_id);
											ent_params->SetString(index, num_chars, pbuffer);
										}
									}
									break;
								}
								case Apparance::Parameter::List: 
								{
									const Apparance::IParameterCollection* src_list = placement_parameters->GetList( j );
									if(src_list)
									{
										Apparance::IParameterCollection* dst_list = ent_params->ModifyList( param_id );
										if (!dst_list)
										{
											int index = ent_params->AddParameter(param_type, param_id);
											dst_list = ent_params->SetList( index );
										}

										if(dst_list)
										{
											dst_list->Sync( src_list );
										}
									}
									break;
								}
							}
						}
						placement_parameters->EndAccess();
						pentity->EndEditingParameters();

						//done
						spec_params->EndAccess();
				
					}
				}

				//second stage, because:
				//bp based actor spawn always defers construction script so that we have a chance to inject placement params that will drive generation and script may consume and override			
				//attach has to happen after construction script too
				m_pActor->AddBlueprint_End( pactor, bp_transform, placement_parameters );
			}
		}
		if(pbasemesh)
		{
			//evaluate instancing options
			EApparanceInstancingMode asset_instancing_mode = pstaticmeshresource ? pstaticmeshresource->EnableInstancing : EApparanceInstancingMode::PerEntity;
			EApparanceInstancingMode project_instancing_mode = UApparanceEngineSetup::GetInstancedRenderingMode();
			EApparanceInstancingMode instancing_mode = (asset_instancing_mode == EApparanceInstancingMode::PerEntity) ? project_instancing_mode : asset_instancing_mode;

			//apply creation method
			if (instancing_mode==EApparanceInstancingMode::Always || (instancing_mode==EApparanceInstancingMode::PerEntity && m_pActor->UseMeshInstancing()))
			{
				GENLOG_INC( nGenLogInstances )
				//add via instanced mesh
				FMeshInstanceHandle handle = m_pActor->AddInstancedMesh( pbasemesh, objecttransform );

				//ensure place to store meshes
				FMeshInstanceCacheEntry* pinstancecacheentry = m_pActor->InstanceCache.Find( id );
				TSharedPtr<TArray<FMeshInstanceHandle>> phandlelist;
				if (pinstancecacheentry)
				{
					phandlelist = pinstancecacheentry->Instances;
				}
				else
				{
					phandlelist = MakeShareable( new TArray<FMeshInstanceHandle>() );
					FMeshInstanceCacheEntry mice;
					mice.Instances = phandlelist;
					m_pActor->InstanceCache.Add(id, mice);
				}

				if(phandlelist) //TODO: why would this be null in a cook?
				{
					phandlelist->Add( handle );
				}
			}
			else
			{ 
				//add as mesh component
				GENLOG_INC( nGenLogMeshes )
				UStaticMeshComponent* pmesh = m_pActor->AddMesh( pbasemesh, objecttransform );
				if (pmesh)
				{
					//ensure place to store meshes
					FMeshCacheEntry* pmeshcacheentry = m_pActor->MeshCache.Find(id);
					if (!pmeshcacheentry)
					{
						pmeshcacheentry = &m_pActor->MeshCache.Emplace( id );
					}

					pmeshcacheentry->Meshes.Add(pmesh);
				}
			}
		}
		if (pcomponent_template)
		{
			//add a component of this type
			UActorComponent* pcomp = m_pActor->AddActorComponent( pcomponent_template, objecttransform );
			if (pcomp)
			{
				GENLOG_INC( nGenLogComponents )
				//ensure place to store components
				FActorComponentCacheEntry* pactorcompcacheentry = m_pActor->ActorComponentCache.Find(id);
				if (!pactorcompcacheentry)
				{
					pactorcompcacheentry = &m_pActor->ActorComponentCache.Emplace( id );
				}
				
				pactorcompcacheentry->Components.Add( pcomp );

				//apply placement parameters to component
				pcomponent_template->ApplyParameters( placement_parameters, pcomp );
				USceneComponent* pscene_comp = Cast<USceneComponent>(pcomp);
				
				//needs regenerating
				//TODO: try to do parameter set earlier so don't need to re-gen this
				if (pscene_comp)
				{
					pscene_comp->MarkRenderStateDirty();
				}
			}
		}
	}
	m_pActor->EndGeometryUpdate( tier_index );

	//---- ----
	//notify tooling
	FApparanceUnrealModule::GetModule()->NotifyExternalContentChanged();

	//notify rate limiter so it can measure typical synth rate
	if(m_pRateLimiter.IsValid())
	{
		m_pRateLimiter->End( request_id );
	}

	//done, this is the handle for this added content
	return Apparance::GeometryID( id );
}

// Old geometry
void FEntityRendering::RemoveGeometry_Deferred(Apparance::GeometryID geometry_id)
{
	GENLOG_INC( nGenLogRemove )
	SCOPE_CYCLE_COUNTER( STAT_RemovingContent );

	//UE_LOG( LogApparance, Log, TEXT("RemoveGeometry( %i )"), geometry_id );
	if(geometry_id!=Apparance::InvalidID)
	{
		//remove specific
		RemoveContent( geometry_id );
		
		//notify tooling
		FApparanceUnrealModule::GetModule()->NotifyExternalContentChanged();
	}
}

// content removal: specific
//
void FEntityRendering::RemoveContent( Apparance::GeometryID geometry_id )
{
	//assume that a change of content means this is a dynamic object (needs to be non-static)
#if 1
	if(m_pActor->DisplayRoot->Mobility == EComponentMobility::Static)
	{
		//unless level being torn down of course
		if(IsValid( m_pActor )
			&& IsValid( m_pActor->DisplayRoot )
			&& !m_pActor->DisplayRoot->HasAnyFlags( EObjectFlags::RF_BeginDestroyed )	//double checks
			&& !m_pActor->HasAnyFlags( EObjectFlags::RF_BeginDestroyed ))
		{
			m_pActor->DisplayRoot->SetMobility( EComponentMobility::Stationary );
			UE_LOG( LogApparance, Warning, TEXT( "Entity needs to be dynamic, becoming less static: %s" ), *m_pActor->GetName() );
		}
	}
#endif

	//---- GEOMETRY ----
	//remove proc mesh
	TWeakObjectPtr<class UProceduralMeshComponent>* ppcomp = m_pActor->GeometryCache.Find(geometry_id);
	if (ppcomp)
	{
		class UProceduralMeshComponent* pcomponent = (*ppcomp).Get();
		if (pcomponent) //isn't present after undo of an entity delete
		{
			//UE_LOG( LogApparance, Log, TEXT( "Removing Geometry id %i : [%p]" ), geometry_id, ppcomp );
			m_pActor->RemoveGeometry(pcomponent);
			m_pActor->GeometryCache.Remove(geometry_id);
		}
	}
	//remove collision mesh
	TWeakObjectPtr<class UProceduralMeshComponent>* pcolcomp = m_pActor->CollisionCache.Find( geometry_id );
	if(pcolcomp)
	{
		class UProceduralMeshComponent* pcomponent = (*pcolcomp).Get();
		if(pcomponent) //isn't present after undo of an entity delete
		{
			m_pActor->RemoveGeometry( pcomponent );
			m_pActor->CollisionCache.Remove( geometry_id );
		}
	}

	//TODO: remove need for search?
	for(int i=0 ; i<m_Tiers.Num() ; i++)
	{
		FDetailTier* pt = m_Tiers[i].Get();
		if(pt->Components.Remove( geometry_id )!=0)
		{
			break;
		}
	}
	m_EditingTier.Components.Remove( geometry_id );

	//---- MESHES ----
	//remove instance
	FMeshInstanceCacheEntry* pcache_entry = m_pActor->InstanceCache.Find(geometry_id);
	if (pcache_entry)
	{
		TArray<FMeshInstanceHandle>* pinstances = pcache_entry->Instances.Get();
		if (pinstances)
		{
			for (int i = 0; i < pinstances->Num(); i++)
			{
				m_pActor->RemoveInstancedMesh(pinstances->operator[](i));
			}
		}
		pcache_entry->Instances.Reset();

		m_pActor->InstanceCache.Remove(geometry_id);
	}
	//remove component
	FMeshCacheEntry* pmeshcacheentry = m_pActor->MeshCache.Find(geometry_id);
	if (pmeshcacheentry)
	{
		for (int i = 0; i < pmeshcacheentry->Meshes.Num(); i++)
		{
			UStaticMeshComponent* pmesh = pmeshcacheentry->Meshes[i].Get();
			m_pActor->RemoveMesh(pmesh);
		}
		pmeshcacheentry->Meshes.Reset();

		m_pActor->MeshCache.Remove(geometry_id);
	}
	//remove actor component
	FActorComponentCacheEntry* pactorcompcacheentry = m_pActor->ActorComponentCache.Find(geometry_id);
	if (pactorcompcacheentry)
	{
		for (int i = 0; i < pactorcompcacheentry->Components.Num(); i++)
		{
			UActorComponent* pcomp = pactorcompcacheentry->Components[i].Get();
			m_pActor->RemoveActorComponent( pcomp );
		}
		pactorcompcacheentry->Components.Reset();
		
		m_pActor->ActorComponentCache.Remove(geometry_id);
	}
	
	//---- ACTORS ----
	FActorCacheEntry* pactorcacheentry = m_pActor->ActorCache.Find(geometry_id);
	if (pactorcacheentry)
	{
		for (int i = 0; i < pactorcacheentry->Actors.Num(); i++)
		{
			AActor* pactor = pactorcacheentry->Actors[i].Get();
			m_pActor->RemoveBlueprint(pactor);
		}
		pactorcacheentry->Actors.Reset();

		m_pActor->ActorCache.Remove(geometry_id);
	}
}

// content removal: all
//
void FEntityRendering::RemoveAllContent()
{
	//tier data dead
	m_Tiers.Empty();

	//checks (can be null in dead ER objects arrising from some BP editing cases)
	if (!m_pActor)
	{
		return;
	}

	//---- GEOMETRY ----
	//remove all proc meshes
	for(auto It = m_pActor->GeometryCache.CreateIterator(); It ; ++It)
	{
		TWeakObjectPtr<class UProceduralMeshComponent> pcomp = It.Value();
		if (pcomp.IsValid()) //isn't present after undo of an entity delete
		{
			m_pActor->RemoveGeometry(pcomp.Get());
		}
	}
	m_pActor->GeometryCache.Empty();
	//remove all proc collision
	for(auto It = m_pActor->CollisionCache.CreateIterator(); It; ++It)
	{
		TWeakObjectPtr<class UProceduralMeshComponent> pcomp = It.Value();
		if(pcomp.IsValid()) //isn't present after undo of an entity delete
		{
			m_pActor->RemoveGeometry( pcomp.Get() );
		}
	}
	m_pActor->CollisionCache.Empty();

	//---- MESHES ----
	//remove instances (requires special handling)
	m_pActor->RemoveAllInstancedMeshes();
	//remove components
	for(auto It = m_pActor->MeshCache.CreateIterator();It;++It)
	{
		FMeshCacheEntry& meshcacheentry = It.Value();
		for (int i = 0; i < meshcacheentry.Meshes.Num(); i++)
		{
			UStaticMeshComponent* pmesh = meshcacheentry.Meshes[i].Get();
			m_pActor->RemoveMesh(pmesh);
		}
	}
	m_pActor->MeshCache.Empty();

	//---- ACTOR COMPONENTS ----
	for(auto It = m_pActor->ActorComponentCache.CreateIterator();It;++It)
	{
		FActorComponentCacheEntry& actorcompcacheentry = It.Value();
		for (int i = 0; i < actorcompcacheentry.Components.Num(); i++)
		{
			UActorComponent* pcomp = actorcompcacheentry.Components[i].Get();
			m_pActor->RemoveActorComponent( pcomp );
		}
	}
	m_pActor->ActorComponentCache.Empty();
	
	//---- ACTORS ----
#if WITH_EDITOR
	TArray<AActor*> actors;
	m_pActor->GetAttachedActors( actors );
#endif
	for (auto It = m_pActor->ActorCache.CreateIterator(); It; ++It)
	{
		int id = It.Key();
		const FActorCacheEntry& e = It.Value();
		int count = e.Actors.Num();
		for (int i = 0; i < count; i++)
		{
			AActor* pa = e.Actors[i].Get();
			if (pa)
			{
#if WITH_EDITOR
				//check(actors.Contains(pa));	//check it is currently one of my
#endif
				m_pActor->RemoveBlueprint( pa );
			}
		}
	}
	m_pActor->ActorCache.Empty();

	//clear general tracking
	m_pActor->ProceduralComponents.Empty();

	//clear material cache
	m_pActor->MaterialCache.Empty();
}

Apparance::Host::IDiagnosticsRendering* FEntityRendering::CreateDiagnosticsRendering()
{
	return new FEntityDiagnosticsRendering( this );
}

void FEntityRendering::DestroyDiagnosticsRendering( Apparance::Host::IDiagnosticsRendering* p )
{
	FEntityDiagnosticsRendering* prdr = (FEntityDiagnosticsRendering*)p;	//downcast to known internal type
	delete prdr;
}

void FEntityRendering::NotifyBeginEditing()
{
	auto notify = FApparanceUnrealModule::GetModule();

	//notify
	m_pEditingParameters = notify->BeginEntityParameterEdit( m_pActor, true/*edit*/);
	notify->EndEntityParameterEdit( m_pActor, false/*leave as interactive*/, Apparance::InvalidID );
}

// frames from interactive edit are corner relative, but in unreal parameters they are lower face relative
// so we need to fix any frames up accordingly
//
static void FixupFrameOrigins( Apparance::IParameterCollection* params )
{
	//scan
	int num_params = params->BeginAccess();
	for(int i = 0; i < num_params; i++)
	{
		//needs mapping?
		Apparance::Parameter::Type type = params->GetType( i );
		int origin_value = (int)EApparanceFrameOrigin::Face; //default is face
		bool map_origin = origin_value != (int)EApparanceFrameOrigin::Corner; //no remapping needed for corner

		//apply mapping if needed by type
		Apparance::ValueID id = params->GetID( i );
		switch(type)
		{
			case Apparance::Parameter::Frame:
			{
				if(map_origin)
				{
					Apparance::Frame frame;
					if(params->FindFrame( id, &frame ))
					{
						EApparanceFrameOrigin origin = (EApparanceFrameOrigin)origin_value;
						Apparance::Vector3 pre_origin = frame.Origin;
						ApparanceFrameOriginAdjust( frame, origin, true/*reverse*/);
						Apparance::Vector3 post_origin = frame.Origin;

						UE_LOG( LogApparance, Log, TEXT( "Entity Frame Fixup: origin before %f,%f,%f  origin after %f,%f,%f" ), pre_origin.X, pre_origin.Y, pre_origin.Z, post_origin.X, post_origin.Y, post_origin.Z);


						params->EndAccess();
						params->BeginEdit();
						params->ModifyFrame( id, &frame );
						params->EndEdit();
						params->BeginAccess();
					}
				}
				break;
			}
		}
	}

	//done
	params->EndAccess();
}

void FEntityRendering::NotifyEndEditing( Apparance::IParameterCollection* final_parameter_patch )
{
	if(m_pEditingParameters)
	{
		//fix up frame origin issues
		FixupFrameOrigins( final_parameter_patch );

		//apply final change
		m_pEditingParameters->Merge( final_parameter_patch );
		m_pEditingParameters = nullptr;

		//notify
		auto notify = FApparanceUnrealModule::GetModule();
		notify->BeginEntityParameterEdit( m_pActor, false/*edit of interactive editing*/ );
		notify->EndEntityParameterEdit( m_pActor, true/*edit*/, Apparance::InvalidID);
	}
}

void FEntityRendering::StoreParameters(Apparance::IParameterCollection* parameter_patch)
{	
	AApparanceEntity * p = Cast< AApparanceEntity>( m_pActor );
	if (p)
	{	
		//get instance parameters
		Apparance::IParameterCollection* pparams = p->BeginEditingParameters();
		pparams->EndEdit();

		//fix up frame origin issues
		FixupFrameOrigins( parameter_patch );

		//store changes
		pparams->Merge(parameter_patch);

		//done with instance parameters
		pparams->BeginEdit();
		p->EndEditingParameters();
	}
}


void FEntityRendering::ModifyWorldTransform( Apparance::Vector3 world_origin, Apparance::Vector3 world_unit_right, Apparance::Vector3 world_unit_forward )
{
	FVector location = UNREALSPACE_FROM_APPARANCESPACE( world_origin );
	FVector right = UNREALSPACE_FROM_APPARANCESPACE( world_unit_right );
	FVector forward = UNREALSPACE_FROM_APPARANCESPACE( world_unit_forward );

	FMatrix orientation = FRotationMatrix::MakeFromXY( forward, right ); //(X is forward, Y is right in Unreal)
	FQuat rot = orientation.ToQuat();

	m_pActor->SetActorLocationAndRotation( location, rot, false, nullptr, ETeleportType::TeleportPhysics );
}




// Tier lookup / creation
struct FDetailTier* FEntityRendering::GetTier( int tier_index )
{
	//special editing content tier
	if(tier_index == -1)
	{
		return &m_EditingTier;
	}

	check(tier_index<1000);	//sanity check

	//grow to match
	while(tier_index>=m_Tiers.Num())
	{
		FDetailTier* new_tier = new FDetailTier();
		new_tier->TierIndex = m_Tiers.Num();
		m_Tiers.Add( MakeShareable( new_tier ) );
	}

	return m_Tiers[tier_index].Get();
}

// reset any cached materials (e.g. when material resource entries changed)
//
void FEntityRendering::InvalidateMaterials()
{
	//clear out our record, components hold actual references and they will be gone soon anyway
	for(int i=0 ; i<m_Tiers.Num() ;i++)
	{
		m_Tiers[i]->Materials.Empty();
		m_Tiers[i]->Collision.Empty();
	}

	m_EditingTier.Materials.Empty();
	m_EditingTier.Collision.Empty();
}

// lookup up material, should have been registered by material operator node
//
class UMaterialInterface* FEntityRendering::GetMaterial( Apparance::MaterialID material_id, TSharedPtr<Apparance::IParameterCollection> parameters, TArray<Apparance::TextureID>& textures, int tier_index, bool* pwant_collision_out )
{
	//generate instances of this material for each tier so they can have different blend parameters
	FDetailTier* ptier = GetTier( tier_index );

	//resolve material
	class UMaterialInterface* pmaterial = ptier->GetMaterial( m_pActor, material_id, parameters, textures, pwant_collision_out );
	return pmaterial;
}


//////////////////////////////////////////////////////////////////////////
// FDetailTier


// lookup/cache a material instance
//
class UMaterialInterface* FDetailTier::GetMaterial( AApparanceEntity* pactor, Apparance::MaterialID material_id, TSharedPtr<Apparance::IParameterCollection> parameters, TArray<Apparance::TextureID>& textures, bool* pwant_collision_out )
{
	//extract collision flag
	bool* pcollisionflag = Collision.Find( material_id );
	bool want_collision = pcollisionflag && *pcollisionflag;

	//search for existing
	for(int i = 0; i < Materials.Num(); i++)
	{
		FParameterisedMaterial* pexisting_mat = Materials[i].Get();

		//same material asset?
		if(pexisting_mat->ID != material_id)
		{
			continue;
		}
	
		//parameters?
		Apparance::IParameterCollection* existing_parameters = pexisting_mat->Parameters.Get();
		bool has_params = parameters.IsValid();
		bool existing_params = existing_parameters != nullptr;
		if(has_params != existing_params)
		{
			continue;
		}

		//check paramter match
		if(has_params)
		{
			//same parameters?
			if(((existing_parameters != parameters.Get()) && !existing_parameters->Equal( parameters.Get() )))
			{
				continue;
			}

			//same textures?
			if(pexisting_mat->Textures.Num() != textures.Num())
			{
				continue;
			}
			bool same_textures = true;
			for(int t = 0; t < textures.Num(); t++)
			{
				if(pexisting_mat->Textures[t] != textures[t])
				{
					same_textures = false;
					break;
				}
			}
			if(!same_textures)
			{
				continue;
			}
		}

		//found a match...
		if (!pexisting_mat->MaterialInstance.IsValid())
		{
			//...but it has expired
			Materials.RemoveAt( i ); //purge from tracking
			break;
		}

		if(pwant_collision_out)
		{
			*pwant_collision_out = want_collision;
		}

		//they fully match!
		return pexisting_mat->MaterialInstance.Get();
	}

	//cache a new one

	//query asset database
	UMaterialInterface* pbasematerial = nullptr;
	const UApparanceResourceListEntry_Material* pmaterialentry;
	if(!FApparanceUnrealModule::GetAssetDatabase()->GetMaterial( material_id, pbasematerial, pmaterialentry, &want_collision ))
	{
		//or fallback if not found (no fallback if collision only as should not render)
		if(!want_collision)
		{
			pbasematerial = FApparanceUnrealModule::GetModule()->GetFallbackMaterial();
		}
	}

	//store
	TSharedPtr<FParameterisedMaterial> new_mat_entry = MakeShareable( new FParameterisedMaterial() );
	int new_variant_index = Materials.Num();
	Materials.Add( new_mat_entry );
	Collision.Add( material_id, want_collision );

	//fill in
	new_mat_entry->ID = material_id;
	new_mat_entry->Parameters = parameters;
	new_mat_entry->Textures = textures;

	//wrap with instance
	UMaterialInstanceDynamic* pmaterial = nullptr;
	if(pbasematerial)
	{
		pmaterial = UMaterialInstanceDynamic::Create( pbasematerial, pactor );
	}
	new_mat_entry->MaterialInstance = pmaterial;

	//make actor aware of new material variant in play
	pactor->MaterialCache.Add( (int)material_id | (int)(TierIndex << 18) | (int)(new_variant_index << 25), pmaterial );

	//apply initial parameters
	if(pmaterialentry)
	{
		pmaterialentry->Apply( new_mat_entry->MaterialInstance.Get(), new_mat_entry->Parameters.Get(), new_mat_entry->Textures );

#if WITH_EDITOR
		//need to monitor for internal dyamic resource changes, which can only happen in-editor
		FAssetDatabase* passet_db = FApparanceUnrealModule::GetAssetDatabase();
		passet_db->Editor_TrackMaterialUse( pmaterialentry, new_mat_entry );
#endif
	}

	if(pwant_collision_out)
	{
		*pwant_collision_out = want_collision;
	}

	//result
	return pmaterial;
}



//////////////////////////////////////////////////////////////////////////
// FEntityView

FEntityView::FEntityView( FEntityRendering* per )
	: m_pEntityRendering( per )
{
}

FEntityView::~FEntityView()
{
}

Apparance::Vector3 FEntityView::GetPosition() const
{
	auto world = m_pEntityRendering->GetActor()->GetWorld();
	auto view_locs = world->ViewLocationsRenderedLastFrame;
	if(view_locs.Num() >0)
	{
		FVector view_loc = view_locs[0];		
		Apparance::Vector3 view_position = APPARANCESPACE_FROM_UNREALSPACE( view_loc );
		return view_position;
	}

	return Apparance::Vector3({0,0,0});
}

void FEntityView::SetDetailRange( int tier_index, float near0, float near1, float far1, float far0 )
{
	FDetailTier* ptier = m_pEntityRendering->GetTier( tier_index );

	//transform to unreal space
	near0 = UNREALSCALE_FROM_APPARANCESCALE( near0 );
	near1 = UNREALSCALE_FROM_APPARANCESCALE( near1 );
	far1 = UNREALSCALE_FROM_APPARANCESCALE( far1 );
	far0 = UNREALSCALE_FROM_APPARANCESCALE( far0 );
		
	//calc
	float f0 = far0;
	float f1 = -1.0f/(far0-far1);
	float f2 = near0;
	float f3 = 1.0f/(near1-near0);
	ptier->DetailBlend = FLinearColor( f0, f1, f2, f3 );

	//apply
	for( int i=0 ; i<ptier->Materials.Num() ; i++)		
	{
		FParameterisedMaterial* ppm = ptier->Materials[i].Get();
		UMaterialInstanceDynamic* pmaterial = ppm->MaterialInstance.Get();
		pmaterial->SetVectorParameterValue( "DetailBlend", ptier->DetailBlend );
	}
}

#if APPARANCE_DEBUGGING_HELP_EntityRendering
PRAGMA_ENABLE_OPTIMIZATION_ACTUAL
#endif