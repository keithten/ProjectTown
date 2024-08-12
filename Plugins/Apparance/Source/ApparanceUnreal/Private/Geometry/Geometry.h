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

// Unreal
#include "Math/Vector.h"

// module
#include "Utility/ApparanceConversion.h"

const int ApparanceGeometry_MaxTextureChannels = 3;


struct FApparancePlacement
{
	Apparance::ObjectID ID;
	Apparance::IParameterCollection* Parameters;
};

// Generated geometry container
//
class FApparanceGeometry : public Apparance::Host::IGeometry
{
	TArray<class FApparanceGeometryPart*> m_Parts;
	TArray<FApparancePlacement>           m_Objects;

public:
	FApparanceGeometry();
	virtual ~FApparanceGeometry();

	//~ Begin Apparance IGeometry Interface
	virtual int GetVertexLimit() const override;
	virtual int GetIndexLimit() const override;

	virtual struct Apparance::Host::IGeometryPart* AddTriangleList( Apparance::MaterialID material, Apparance::IParameterCollection* parameters, const Apparance::TextureID* textures, int texture_count, int vertex_count, int triangle_count ) override;
	virtual struct Apparance::Host::IGeometryPart* AddLineList( Apparance::MaterialID material, Apparance::IParameterCollection* parameters, const Apparance::TextureID* textures, int texture_count, int vertex_count, int line_indices ) override;
	virtual int GetPartCount() const override;

	virtual void AddObject(Apparance::ObjectID object_type, Apparance::IParameterCollection* parameters) override;

	virtual void SealGeometry() override;
	virtual bool IsRenderable() const override;
	//~ End Apparance IGeometry Interface

	//access
	const TArray<class FApparanceGeometryPart*>& GetParts() const { return m_Parts; }
	const TArray<FApparancePlacement>&           GetObjects() const { return m_Objects; }
	void GetExtents( FVector& out_min, FVector& out_max );
};


// Generated geometry part
//
class FApparanceGeometryPart : public Apparance::Host::IGeometryPart
{
	Apparance::IUnknownData* m_pEngineData;

public:
	//appearance
	Apparance::MaterialID    Material;
	TSharedPtr<Apparance::IParameterCollection> Parameters;
	TArray<Apparance::AssetID> Textures;
	FVector                  BoundsMin;
	FVector                  BoundsMax;

	//geometry
	TArray<FVector>          Positions;
	TArray<FVector>          Normals;
	TArray<FVector>          Tangents;
	TArray<FLinearColor>     Colours;
	TArray<FVector2D>        UVs[ApparanceGeometry_MaxTextureChannels];

	TArray<int32>            Triangles;

public:
	FApparanceGeometryPart( Apparance::MaterialID material, Apparance::IParameterCollection* parameters, const Apparance::TextureID* textures, int texture_count, int vertex_count, int triangle_count );
	virtual ~FApparanceGeometryPart();

	virtual Apparance::Parameter::Type GetPositionType() const override;
	virtual Apparance::Parameter::Type GetNormalType() const override;
	virtual Apparance::Parameter::Type GetTangentType() const override;
	virtual Apparance::Parameter::Type GetColourType( int colour_channel_index ) const override;
	virtual Apparance::Parameter::Type GetTextureCoordinateType( int texture_channel_index) const override;

	virtual Apparance::GeometryChannel GetPositions() override;
	virtual Apparance::GeometryChannel GetNormals() override;
	virtual Apparance::GeometryChannel GetTangents() override;
	virtual Apparance::GeometryChannel GetColours( int colour_channel_index ) override;
	virtual Apparance::GeometryChannel GetTextureCoordinates( int texture_channel_index ) override;

	virtual Apparance::GeometryChannel GetIndices() override;
	
	virtual void SetBounds( Apparance::Vector3 min_bounds, Apparance::Vector3 max_bounds ) override;

	virtual void SetEngineData( Apparance::IUnknownData* pdata ) override;
	virtual Apparance::IUnknownData* GetEngineData() const override;

	//access
	void GetExtents( FVector& out_min, FVector& out_max );
};

