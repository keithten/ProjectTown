//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

//local debugging help
#define APPARANCE_DEBUGGING_HELP_Geometry 0
#if APPARANCE_DEBUGGING_HELP_Geometry
PRAGMA_DISABLE_OPTIMIZATION_ACTUAL
#endif

// main
#include "Geometry.h"

// unreal
#include "Math/UnrealMath.h"

// module
#include "ApparanceUnreal.h"


//////////////////////////////////////////////////////////////////////////
// FApparanceGeometry

FApparanceGeometry::FApparanceGeometry()
{
}

FApparanceGeometry::~FApparanceGeometry()
{
	for(auto ppart : m_Parts)
	{
		delete ppart;
	}
	m_Parts.Empty();

	for (int i = 0; i < m_Objects.Num(); i++)
	{
		delete m_Objects[i].Parameters;
	}
	m_Objects.Empty();
}

int FApparanceGeometry::GetVertexLimit() const
{ 
	return MAX_int32; 
}
int FApparanceGeometry::GetIndexLimit() const
{ 
	return MAX_int32;
}

struct Apparance::Host::IGeometryPart* FApparanceGeometry::AddTriangleList( Apparance::MaterialID material, Apparance::IParameterCollection* parameters, const Apparance::TextureID* textures, int texture_count, int vertex_count, int triangle_count )
{
	FApparanceGeometryPart* part = new FApparanceGeometryPart( material, parameters, textures, texture_count, vertex_count, triangle_count );
	m_Parts.Add( part );
	return part;
}

struct Apparance::Host::IGeometryPart* FApparanceGeometry::AddLineList( Apparance::MaterialID material, Apparance::IParameterCollection* parameters, const Apparance::TextureID* textures, int texture_count, int vertex_count, int line_indices )
{
	//TODO: support line rendering
	//check(false);
	return nullptr;
}

int FApparanceGeometry::GetPartCount() const
{
	return m_Parts.Num();
}

void FApparanceGeometry::AddObject(Apparance::ObjectID object_type, Apparance::IParameterCollection* parameters)
{
	FApparancePlacement placement;
	placement.ID = object_type;
	placement.Parameters = parameters;
	m_Objects.Add(placement);
}

/// <summary>
/// in-place upcast of float vector array into double vector array
/// NOTE: These arrays assumed contiguous and are filled in with float vectors by Apparance. To support the double vectors Unreal (5) needs we pretend they are float arrays for Apparance to fill in, then convert them in-place to double vectors
/// </summary>
/// <param name="vector_array"></param>
static void HandleDoublePromotion( TArray<FVector>& vector_array )
{
	//UE5 (Preview 1) onwards is double native
	if(sizeof(FVector::X)==sizeof(double))
	{
		int n = vector_array.Num();
		FVector* dvec = vector_array.GetData();						//a double vector type
		for(int i = 0; i < n; i++)
		{
			//take a copy as we are overwriting the source
			Apparance::Vector3 fvec = *((Apparance::Vector3*)(void*)&dvec[i]);	//view first half as a float vector type
			//explicitly convert the float members to double
			dvec[i].X = (double)fvec.X;
			dvec[i].Y = (double)fvec.Y;
			dvec[i].Z = (double)fvec.Z;
		}
	}
}
static void HandleDoublePromotion( TArray<FVector2D>& vector_array )
{
	//UE5 (Preview 1) onwards is double native
	if(sizeof( FVector2D::X ) == sizeof( double ))
	{
		int n = vector_array.Num();
		FVector2D* dvec = vector_array.GetData();						//a double vector type
		for(int i = 0; i < n; i++)
		{
			//take a copy as we are overwriting the source
			Apparance::Vector2 fvec = *((Apparance::Vector2*)(void*)&dvec[i]);	//view first half as a float vector type
			//explicitly convert the float members to double
			dvec[i].X = (double)fvec.X;
			dvec[i].Y = (double)fvec.Y;
		}
	}
}

// finished adding parts, carry out any finalising possible/needed
void FApparanceGeometry::SealGeometry()
{
	//promote float data to doubles if needed
	for(int i = 0; i < m_Parts.Num(); i++)
	{
		HandleDoublePromotion( m_Parts[i]->Positions );
		HandleDoublePromotion( m_Parts[i]->Normals );
		HandleDoublePromotion( m_Parts[i]->Tangents );
		for(int t = 0; t < ApparanceGeometry_MaxTextureChannels; t++)
		{
			HandleDoublePromotion( m_Parts[i]->UVs[t] );
		}
	}
}


// is geometry actually ready to render and display correctly (i.e. materials ready, etc)
//
bool FApparanceGeometry::IsRenderable() const
{ 
	//TODO: materials
	return true; 
}

void FApparanceGeometry::GetExtents( FVector& out_min, FVector& out_max )
{
	out_min = FVector::ZeroVector;
	out_max = FVector::ZeroVector;

	for(int i=0 ; i<m_Parts.Num() ; i++)
	{
		FVector mn = m_Parts[i]->BoundsMin;
		FVector mx = m_Parts[i]->BoundsMax;
		if(i==0)
		{
			out_min = mn;
			out_max = mx;
		}
		else
		{
			out_min.X = FMath::Min( out_min.X, mn.X );
			out_min.Y = FMath::Min( out_min.Y, mn.Y );
			out_min.Z = FMath::Min( out_min.Z, mn.Z );
			out_max.X = FMath::Max( out_max.X, mx.X );
			out_max.Y = FMath::Max( out_max.Y, mx.Y );
			out_max.Z = FMath::Max( out_max.Z, mx.Z );
		}
	}
}


//////////////////////////////////////////////////////////////////////////
// FApparanceGeometryPart

FApparanceGeometryPart::FApparanceGeometryPart( Apparance::MaterialID material, Apparance::IParameterCollection* parameters, const Apparance::TextureID* textures, int texture_count, int vertex_count, int triangle_count )
	: m_pEngineData( nullptr )
{
	Material = material;
	Parameters = nullptr;
	if(parameters)
	{
		//take copy of parameters
		Parameters = MakeShareable( FApparanceUnrealModule::GetEngine()->CreateParameterCollection() );
		Parameters->Sync( parameters );
	}
	for(int i = 0; i < texture_count; i++)
	{
		Textures.Add( textures[i] );
	}

	Positions.SetNumZeroed( vertex_count );
	Normals.SetNumZeroed( vertex_count );
	Tangents.SetNumZeroed( vertex_count );
	Colours.SetNumZeroed( vertex_count );
	for (int i = 0; i < ApparanceGeometry_MaxTextureChannels; i++)
	{
		UVs[i].SetNumZeroed( vertex_count );
	}

	Triangles.SetNumZeroed( triangle_count * 3 );
}

FApparanceGeometryPart::~FApparanceGeometryPart()
{
}

Apparance::Parameter::Type FApparanceGeometryPart::GetPositionType() const
{
	return Apparance::Parameter::Type::Vector3;
}
Apparance::Parameter::Type FApparanceGeometryPart::GetNormalType() const
{
	return Apparance::Parameter::Type::Vector3;
}
Apparance::Parameter::Type FApparanceGeometryPart::GetTangentType() const
{
	return Apparance::Parameter::Type::Vector3;
}
Apparance::Parameter::Type FApparanceGeometryPart::GetColourType( int colour_channel_index ) const
{
	return Apparance::Parameter::Type::Colour;
}
Apparance::Parameter::Type FApparanceGeometryPart::GetTextureCoordinateType( int texture_channel_index) const
{
	return Apparance::Parameter::Type::Vector2;
}

Apparance::GeometryChannel FApparanceGeometryPart::GetPositions()
{
	Apparance::GeometryChannel c;
	c.Data = Positions.GetData();
	c.Count = Positions.Num();
	c.Size = sizeof(Apparance::Vector3);
	c.Span = Positions.GetTypeSize();
	return c;
}
Apparance::GeometryChannel FApparanceGeometryPart::GetNormals()
{
	Apparance::GeometryChannel c;
	c.Data = Normals.GetData();
	c.Count = Normals.Num();
	c.Size = sizeof(Apparance::Vector3);
	c.Span = Normals.GetTypeSize();
	return c;
}
Apparance::GeometryChannel FApparanceGeometryPart::GetTangents()
{
	Apparance::GeometryChannel c;
	c.Data = Tangents.GetData();
	c.Count = Tangents.Num();
	c.Size = sizeof( Apparance::Vector3 );
	c.Span = Tangents.GetTypeSize();
	return c;
}
Apparance::GeometryChannel FApparanceGeometryPart::GetColours( int colour_channel_index )
{
	Apparance::GeometryChannel c;
	c.Data = Colours.GetData();
	c.Count = Colours.Num();
	c.Size = sizeof(FLinearColor);
	c.Span = Colours.GetTypeSize();
	return c;
}
Apparance::GeometryChannel FApparanceGeometryPart::GetTextureCoordinates( int texture_channel_index )
{
	Apparance::GeometryChannel c;
	c.Data = UVs[texture_channel_index].GetData();
	c.Count = UVs[texture_channel_index].Num();
	c.Size = sizeof(FVector2D);
	c.Span = UVs[texture_channel_index].GetTypeSize();
	return c;
}

Apparance::GeometryChannel FApparanceGeometryPart::GetIndices()
{
	Apparance::GeometryChannel c;
	c.Data = Triangles.GetData();
	c.Count = Triangles.Num();
	c.Size = sizeof(int);
	c.Span = Triangles.GetTypeSize();
	return c;
}
	
void FApparanceGeometryPart::SetBounds( Apparance::Vector3 min_bounds, Apparance::Vector3 max_bounds )
{
	//XY flip
	BoundsMin = UNREALSPACE_FROM_APPARANCESPACE( UNREALVECTOR_FROM_APPARANCEVECTOR3( min_bounds ) );
	BoundsMax = UNREALSPACE_FROM_APPARANCESPACE( UNREALVECTOR_FROM_APPARANCEVECTOR3( max_bounds ) );
}

void FApparanceGeometryPart::SetEngineData( Apparance::IUnknownData* pdata )
{
	m_pEngineData = pdata;
}
Apparance::IUnknownData* FApparanceGeometryPart::GetEngineData() const
{
	return m_pEngineData;
}

void FApparanceGeometryPart::GetExtents( FVector& out_min, FVector& out_max )
{
	out_min = BoundsMin;
	out_max = BoundsMax;
}

#if APPARANCE_DEBUGGING_HELP_Geometry
PRAGMA_ENABLE_OPTIMIZATION_ACTUAL
#endif