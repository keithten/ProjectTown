//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

// main
#include "GeometryFactory.h"

// unreal

// module
#include "ApparanceUnreal.h"
#include "Geometry.h"
#include "EntityRendering.h"



//////////////////////////////////////////////////////////////////////////
// FGeometryFactory

FGeometryFactory::FGeometryFactory()
{
}

#if TIMESLICE_GEOMETRY_ADD_REMOVE
extern bool Apparance_NotifyGeometryDestruction( struct Apparance::Host::IGeometry* pgeometry );
#endif

struct Apparance::Host::IGeometry* FGeometryFactory::CreateGeometry()
{
	return new FApparanceGeometry();
}
void FGeometryFactory::DestroyGeometry( struct Apparance::Host::IGeometry* pgeometry )
{
#if TIMESLICE_GEOMETRY_ADD_REMOVE
	Apparance_NotifyGeometryDestruction( pgeometry );
#endif
	delete pgeometry;
}
Apparance::MaterialID FGeometryFactory::GetDefaultTriangleMaterial()
{ 
	return Apparance::InvalidID; 
}
Apparance::MaterialID FGeometryFactory::GetDefaultLineMaterial()
{ 
	return Apparance::InvalidID; 
}
