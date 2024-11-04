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


// Source of new geometry
//
struct FGeometryFactory : public Apparance::Host::IGeometryFactory
{
	// apparance

public:
	FGeometryFactory();


	//~ Begin Apparance IGeometryFactory Interface
	virtual struct Apparance::Host::IGeometry* CreateGeometry(int debug_request_version);
	virtual void DestroyGeometry( struct Apparance::Host::IGeometry* pgeometry );

	virtual Apparance::MaterialID GetDefaultTriangleMaterial();
	virtual Apparance::MaterialID GetDefaultLineMaterial();
	//~ End Apparance IGeometryFactory Interface

	

};
