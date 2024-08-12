//----
// Apparance Engine API
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

#pragma once

//
// Full API for integrating the Apparance procedural generation system into your engine.
//
namespace Apparance
{
	//declarations of interfaces the host is required to implement
	//(all optional depending on the degree of feature support required)
	namespace Host
	{
		struct IGeometryFactory;
		struct IGeometry;
		struct IGeometryPart;
		struct IEntityRendering;
		struct IEntityView;
		struct IOperatorDefinition;
		struct ILoggingService;
		struct IResultHandler;
		struct IDiagnosticsRendering;
		struct IDiagnosticsGeometryDisplay;
		struct IAssetDatabase;
	}

	//required forward declarations
	struct IEngine;

	//main entry point
	namespace Engine
	{
		//main engine init/shutdown, only call once each per session
		extern IEngine* Start();
		extern void     Stop();
	};

	//ids
	typedef unsigned int ProcedureID;
	typedef unsigned int ValueID;
	typedef unsigned int GeometryID;
	typedef unsigned int MaterialID;
	typedef unsigned int TextureID;
	typedef unsigned int ObjectID;
	typedef unsigned int AssetID;
	const unsigned int InvalidID = 0;	//invalid across all ID types

	//parameter values
	namespace Parameter
	{
		enum Type
		{
			None = 0,
			Integer = 1,
			Float = 2,
			Bool = 3,
			Vector2 = 4,
			Vector3 = 5,
			Vector4 = 6,
			Colour = 7,
			String = 8,
			Frame = 9,
			List = 10,
		};
	}

	//image data
	namespace ImagePrecision
	{
		enum Type
		{
			Byte = 0,	//byte 0-255 per channel
			Float = 1,	//float per channel
		};
	}

	//Coordinate space orientation and scale is arbitrary and you can interpret/use them as you like.
	//However, the third party engine plugin implementations perform some conversion of data to fit each
	//ones conventions so if you want your procedures to be compatible between engines then you should adopt
	//the Apparance conventions when authoring them, as follows:
	// * right-handed coordinate system
	// * directions are according to map/maths/CAD conventions:
	//     positive X is to the right (ground surface)
	//     positive Y is forward (ground surface)
	//     positive Z is up (against gravity)
	// * units are usually considered to be metres in game world space

	//vector types
	struct Vector2
	{
		float X, Y;
		Vector2() : X( 0 ), Y( 0 ) {}
		Vector2( float x, float y ) : X( x ), Y( y ) {}
	};
	struct Vector3
	{
		float X, Y, Z;
		Vector3() : X( 0 ), Y( 0 ), Z( 0 ) {}
		Vector3( float x, float y, float z ) : X( x ), Y( y ), Z( z ) {}
	};
	struct Vector4
	{
		float X, Y, Z, W;
		Vector4() : X( 0 ), Y( 0 ), Z( 0 ), W( 0 ) {}
		Vector4( float x, float y, float z, float w ) : X( x ), Y( y ), Z( z ), W( w ) {}
	};

	//RGBA floating point colour type
	struct Colour
	{
		float R, G, B, A;
		Colour() : R( 0 ), G( 0 ), B( 0 ), A( 0 ) {}
		Colour( float r, float g, float b, float a = 1.0f ) : R( r ), G( g ), B( b ), A( a ) {}
	};
	const Colour NoColour = { 0,0,0,0 };

	//3D oriented bounds
	struct Frame
	{
		struct Axes
		{
			Apparance::Vector3 X;
			Apparance::Vector3 Y;
			Apparance::Vector3 Z;
		}						Orientation;
		Apparance::Vector3		Origin;
		Apparance::Vector3		Size;

		Frame() { for(int i = 0; i < sizeof( Frame ); i++) ((char*)this)[i] = 0; Orientation.X.X = 1; Orientation.Y.Y = 1; Orientation.Z.Z = 1; }
	};

	//library
	struct ILibrary
	{
		//procedure directory
		virtual int GetProcedureCount() const = 0;
		virtual Apparance::ProcedureID GetProcedureID( int procedure_index ) const = 0;
		virtual const struct ProcedureSpec* GetProcedureSpec( int procedure_index ) const = 0;
		virtual const struct ProcedureSpec* FindProcedureSpec( Apparance::ProcedureID id ) const = 0;

		//editor support
		virtual Apparance::ProcedureID CheckEditNotification() const = 0;	//id of procedure with edits (internal change), repeat call until Apparance::InvalidID
		virtual Apparance::ProcedureID CheckSpecNotification() const = 0;	//id of procedure with specification change (external change), repeat call until Apparance::InvalidID
	};

	//data to track and be able to clean up that we don't need to know anything about
	struct IUnknownData
	{
		virtual ~IUnknownData() {}
	};

	//closure
	struct IClosure : public IUnknownData
	{
		virtual Apparance::ProcedureID       GetID() = 0;
		virtual struct IParameterCollection* GetParameters() = 0;
	};

	//procedure specifications
	struct ProcedureSpec
	{
		Apparance::ProcedureID       ID;
		const char*                  Name;			//name[.subname...]
		const char*                  Category;		//category
		const char*                  Description;	//[description]
		const char*                  Metadata;		//[name=value][,name2=value2...]
		const IParameterCollection*  Inputs;		//ID/name/type/default value
		const IParameterCollection*  Outputs;		//ID/name/type
	};

	//parameter collection
	struct IParameterCollection : public IUnknownData
	{
		//access
		virtual int BeginAccess() const = 0; //returns number of parameters
		virtual Apparance::Parameter::Type GetType( int index ) const = 0;
		virtual Apparance::ValueID GetID( int index ) const = 0;
		virtual const char* GetName( int index ) const = 0;
		virtual Apparance::Parameter::Type FindType( Apparance::ValueID id ) const = 0;
		virtual const char* FindName( Apparance::ValueID id ) const = 0;
		//access by index
		virtual bool GetInteger( int index, int* integer_value ) const = 0;
		virtual bool GetFloat( int index, float* float_value ) const = 0;
		virtual bool GetBool( int index, bool* bool_value ) const = 0;
		virtual bool GetVector3( int index, Apparance::Vector3* vector_value ) const = 0;
		virtual bool GetColour( int index, Apparance::Colour* colour_value ) const = 0;
		virtual bool GetString( int index, int buffer_size, wchar_t* string_buffer_to_fill, int* string_len_out = nullptr ) const = 0;
		virtual bool GetFrame( int index, Apparance::Frame* value ) const = 0;
		virtual const IParameterCollection* GetList( int index ) const = 0;
		//access by id
		virtual bool FindInteger( Apparance::ValueID id, int* integer_value ) const = 0;
		virtual bool FindFloat( Apparance::ValueID id, float* float_value ) const = 0;
		virtual bool FindBool( Apparance::ValueID id, bool* bool_value ) const = 0;
		virtual bool FindVector3( Apparance::ValueID id, Apparance::Vector3* vector_value ) const = 0;
		virtual bool FindColour( Apparance::ValueID id, Apparance::Colour* colour_value ) const = 0;
		virtual bool FindString( Apparance::ValueID id, int buffer_size, wchar_t* string_buffer_to_fill, int* string_len_out = nullptr ) const = 0;
		virtual bool FindFrame( Apparance::ValueID id, Apparance::Frame* value ) const = 0;
		virtual const IParameterCollection* FindList( Apparance::ValueID id ) const = 0;
		virtual void EndAccess() const = 0;

		//editing
		virtual void BeginEdit() = 0;
		virtual int AddParameter( Apparance::Parameter::Type type, Apparance::ValueID id, const char* pszname = nullptr ) = 0;
		virtual int InsertParameter( int index, Apparance::Parameter::Type type, Apparance::ValueID id, const char* pszname = nullptr ) = 0;
		virtual bool DeleteParameter( int index ) = 0;
		virtual bool RemoveParameter( Apparance::ValueID id ) = 0;
		//modify by index
		virtual bool SetInteger( int index, int integer_value ) = 0;
		virtual bool SetFloat( int index, float float_value ) = 0;
		virtual bool SetBool( int index, bool bool_value ) = 0;
		virtual bool SetVector3( int index, const Apparance::Vector3* vector_value ) = 0;
		virtual bool SetColour( int index, const Apparance::Colour* colour_value ) = 0;
		virtual bool SetString( int index, int string_char_count, const wchar_t* string_value ) = 0;
		virtual bool SetFrame( int index, const Apparance::Frame* value ) = 0;
		virtual IParameterCollection* SetList( int index ) = 0; //to set up list
		//modify by id
		virtual bool ModifyInteger( Apparance::ValueID id, int integer_value ) = 0;
		virtual bool ModifyFloat( Apparance::ValueID id, float float_value ) = 0;
		virtual bool ModifyBool( Apparance::ValueID id, bool bool_value ) = 0;
		virtual bool ModifyVector3( Apparance::ValueID id, const Apparance::Vector3* vector_value ) = 0;
		virtual bool ModifyColour( Apparance::ValueID id, const Apparance::Colour* colour_value ) = 0;
		virtual bool ModifyString( Apparance::ValueID id, int string_char_count, const wchar_t* string_value ) = 0;
		virtual bool ModifyFrame( Apparance::ValueID id, const Apparance::Frame* value ) = 0;
		virtual IParameterCollection* ModifyList( Apparance::ValueID id ) = 0;
		virtual void EndEdit() = 0;

		//byte access
		virtual const unsigned char* GetBytes( int& byte_count_out ) const = 0;
		virtual void SetBytes( int byte_count, const unsigned char* psource ) = 0;

		//special utility
		virtual int Sync( const Apparance::IParameterCollection* psource, bool want_names = false ) = 0;	//sync parameters to exactly match another set, but keeping existing values, can use to clone by calling on a new collection. returns resulting parameter count
		virtual int Merge( const Apparance::IParameterCollection* psource, bool want_names = false ) = 0;	//apply parameters to another set, overriding existing values and adding ones not present. returns resulting parameter count
		virtual int Sanitise( const Apparance::IParameterCollection* psource ) = 0; //ensure only parameters matching type/id of source parameters are present. returns resulting parameter count
		virtual bool Equal( const Apparance::IParameterCollection* pother, float tolerance=0 ) const = 0; //compare two collections for exact equality (quantity, types, and values, where possible toleranced compare is used)
	};

	//editor
	struct IEditor
	{
		//setup
		virtual bool SetInstallDirectory( const wchar_t* install_dir ) = 0;	//supply place it's safe to install editor to, can be per-project or global but must have write permissions, returns true if IsInstalled
		virtual bool IsInstalled() const = 0;
		virtual bool RequestInstallation() = 0;
		virtual float GetInstallationProgress() const = 0;
		virtual int GetInstallationMessage( int message_buffer_char_count, wchar_t* message_buffer ) const = 0;

		//operation
		virtual bool Open( Apparance::ProcedureID procedure=Apparance::InvalidID ) = 0;	//open/focus, optionally open/navigate to specific procedure

		//save/load
		virtual bool IsUnsaved() const = 0;
		virtual bool Save() = 0;
	};

	//engine configuration settings
	//(memset to zeros for defaults)
	namespace Engine
	{
		//one-time setup of engine
		//pass once on call to Setup
		struct SetupParameters
		{
			//required
			const char*                     ProcedureStoragePath; //location to load/save procedure definitions

			//optional
			Host::IGeometryFactory*			GeometryFactory;  //0 for non-geometry synthesis
			Host::IOperatorDefinition*		MaterialNodeDefinition; //0 for no materials support
			Host::ILoggingService*			LoggingService;   //0 for standard log output
			Host::IAssetDatabase*			AssetDatabase;    //0 for no asset placement support
		};

		//re-configuration of engine
		//can be changed dynamically if needed
		struct ConfigParameters
		{
			int								SynthesiserCount; //0 for automatic
			int								BufferSizeMB;     //0 for automatic
			int								LiveEditing;      //0 for off, 1 for server for editor to connect to
		};
	}

	//engine
	struct IEngine
	{
		//setup/shutdown
		virtual bool Setup( const Engine::SetupParameters& params ) = 0; //call to set up how Apparance is used
		virtual bool Configure( const Engine::ConfigParameters& params ) = 0; //call to set/change how Apparance is used dynamically
		virtual void Update( float dt ) = 0;                            //call regularly (e.g. every frame)

		//systems
		virtual struct ILibrary* GetLibrary() = 0;	//procedure library API
		virtual struct IEditor*  GetEditor() = 0;	//editor application API

		//objects
		virtual struct IClosure*             CreateClosure( Apparance::ProcedureID id ) const = 0;
		virtual struct IParameterCollection* CreateParameterCollection() const = 0;
		virtual struct IEntity*              CreateEntity( struct Host::IEntityRendering* entity_rendering, bool in_play_mode/*play-in-editor or standalone*/ ) = 0;
		virtual void                         DestroyEntity( struct Apparance::IEntity* entity ) = 0;

		//misc
		virtual struct IParameterCollection* GetVersionInformation() const = 0;
		virtual void                         RunProcedure( struct Apparance::IClosure* procedure_input, struct Apparance::Host::IResultHandler* result_recipient ) = 0;

		//editing handle interaction
		virtual void                         UpdateInteractiveViewport( Apparance::Vector3 view_point, Apparance::Vector3 view_direction, float view_angle_degrees, float pixel_angle_degrees ) = 0;
		virtual bool                         UpdateInteractiveEditing( Apparance::Vector3 cursor_direction_or_offset, bool interact_button_active, int modifier_key_flags ) = 0;  //returns true if input used
	};

	//entity
	struct IEntity
	{
		virtual int Build( struct Apparance::IClosure* procedure_closure, bool dynamic_detail ) = 0;	//returns an ID associated with this request
		virtual void Update( struct Host::IEntityView** views, int view_count, float delta_time ) = 0;

		//editing state interaction
		virtual bool SupportsSmartEditing() const = 0;	//does entity currently support smart editing? (i.e. uses Edit Mode switching operator)
		virtual void EnableSmartEditing() = 0;  //enable play mode editing (NOTE: only needed if editing features used at run-time, they are always smart-editable in editor)
		virtual void SetSelected(bool is_selected) = 0;   //select/deselect the entity in editor (for editing handles/etc), or if using editing features at run-time
		virtual void NotifyWorldTransform( Apparance::Vector3 world_origin, Apparance::Vector3 world_unit_right, Apparance::Vector3 world_unit_forward ) = 0; //inform of movement in editor, or if using editing features, at run-time
	};

	//vertex channel information
	struct GeometryChannel
	{
		void* Data;		//address of first element
		int   Size;		//element size (bytes)
		int   Span;		//how far apart each element
		int   Count;	//how many elements
	};

	//interfaces the host may be required to implement
	namespace Host
	{
		//custom operators (experimental)
		struct IOperatorDefinition
		{
			virtual const char* GetName() const = 0;
			virtual const char* GetDescription() const = 0;

			virtual int GetInputCount() const = 0;

			virtual Apparance::Parameter::Type GetInputType( int input_index ) const = 0;
			virtual Apparance::ValueID GetInputID( int input_index ) const = 0;
			virtual const char* GetInputName( int input_index ) const = 0;
			virtual const char* GetInputDescription( int input_index ) const = 0;
			virtual void* GetInputDefaultValue( int input_index ) const = 0;
		};

		//multiple parts allowed in a single geometry
		struct IGeometryPart
		{
			virtual Apparance::Parameter::Type GetPositionType() const = 0;
			virtual Apparance::Parameter::Type GetNormalType() const = 0;
			virtual Apparance::Parameter::Type GetTangentType() const = 0;
			virtual Apparance::Parameter::Type GetColourType( int colour_channel_index ) const = 0;
			virtual Apparance::Parameter::Type GetTextureCoordinateType( int texture_channel_index ) const = 0;

			virtual Apparance::GeometryChannel GetPositions() = 0;
			virtual Apparance::GeometryChannel GetNormals() = 0;
			virtual Apparance::GeometryChannel GetTangents() = 0;
			virtual Apparance::GeometryChannel GetColours( int colour_channel_index ) = 0;
			virtual Apparance::GeometryChannel GetTextureCoordinates( int texture_channel_index ) = 0;

			virtual Apparance::GeometryChannel GetIndices() = 0;

			virtual void SetBounds( Apparance::Vector3 min_bounds, Apparance::Vector3 max_bounds ) = 0;

			virtual void SetEngineData( Apparance::IUnknownData* pdata ) = 0;
			virtual Apparance::IUnknownData* GetEngineData() const = 0;
		};

		//geometry factory
		struct IGeometryFactory
		{
			virtual struct Apparance::Host::IGeometry* CreateGeometry() = 0;
			virtual void DestroyGeometry( struct Apparance::Host::IGeometry* pgeometry ) = 0;

			virtual Apparance::MaterialID GetDefaultTriangleMaterial() = 0;
			virtual Apparance::MaterialID GetDefaultLineMaterial() = 0;
		};

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4100) //unreferenced formal parameter in empty default implementations below
#endif

		//geometry
		struct IGeometry
		{
			virtual ~IGeometry() {}	//enforce self cleanup ability

			virtual int GetVertexLimit() const { return 0; }	 //
			virtual int GetIndexLimit() const { return 0; }		//override if supported
			virtual int GetObjectLimit() const { return 0; }	 //

			virtual struct Apparance::Host::IGeometryPart* AddTriangleList( Apparance::MaterialID material, Apparance::IParameterCollection* parameters, const Apparance::TextureID* textures, int texture_count, int vertex_count, int triangle_count ) { return nullptr; }
			virtual struct Apparance::Host::IGeometryPart* AddLineList( Apparance::MaterialID material, Apparance::IParameterCollection* parameters, const Apparance::TextureID* textures, int texture_count, int vertex_count, int line_indices ) { return nullptr; }
			virtual void AddObject( Apparance::ObjectID object_type, IParameterCollection* parameters = nullptr ) {}
			virtual void AddGroup( int name_char_count, const wchar_t* name_value, int child_count ) {}
			virtual void AddParent( Apparance::ObjectID object_type, IParameterCollection* parameters, int name_char_count, const wchar_t* name_value, int child_count ) {}
			virtual int GetPartCount() const = 0;

			virtual void SealGeometry() = 0;		//finished adding parts, carry out any finalising possible/needed
			virtual bool IsRenderable() const = 0;	//is geometry actually ready to render and display correctly (i.e. materials ready, etc)

			//optional diags rendering control of geometry
			virtual struct Apparance::Host::IDiagnosticsGeometryDisplay* GetDiagnosticsDisplay() { return nullptr; }
		};

		//views
		struct IEntityView
		{
			virtual Apparance::Vector3 GetPosition() const = 0;
			virtual void               SetDetailRange( int tier_index, float near0, float near1, float far1, float far0 ) = 0;
		};

		//rendering of entity
		struct IEntityRendering
		{
			virtual Apparance::GeometryID AddGeometry( struct Apparance::Host::IGeometry* geometry, int tier_index, Apparance::Vector3 offset, int request_id ) = 0;
			virtual void                  RemoveGeometry( Apparance::GeometryID geometry_id ) = 0;

			//optionally support rendering of diagnostics primitives
			virtual Apparance::Host::IDiagnosticsRendering* CreateDiagnosticsRendering() { return nullptr; }
			virtual void DestroyDiagnosticsRendering( Apparance::Host::IDiagnosticsRendering* p ) { }

			//interactive editing support
			virtual void NotifyBeginEditing() = 0;
			virtual void ModifyWorldTransform( Apparance::Vector3 world_origin, Apparance::Vector3 world_unit_right, Apparance::Vector3 world_unit_forward ) = 0;
			virtual void NotifyEndEditing( Apparance::IParameterCollection* final_parameter_patch ) = 0;
			virtual void StoreParameters( Apparance::IParameterCollection* parameter_patch ) = 0; //update parameters to persist final changes, but don't trigger a rebuild
		};

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

		//diagnostics rendering
		struct IDiagnosticsRendering
		{
			virtual void Begin() = 0;
			virtual void DrawLine( Apparance::Vector3 start, Apparance::Vector3 end, Apparance::Colour colour ) = 0;
			virtual void DrawBox( Apparance::Frame frame, Apparance::Colour colour ) = 0;
			virtual void DrawSphere( Apparance::Vector3 centre, float radius, Apparance::Colour colour ) = 0;
			virtual void End() = 0;
		};

		//diagnostics control of the way geometry is displayed
		struct IDiagnosticsGeometryDisplay
		{
			virtual void EnableDiagnosticsRendering( bool enable, int part_index = -1/*set all*/ ) = 0;
			virtual void SetDiagnosticsVisibility( bool ignore_culling, bool force_hidden, int part_index = -1/*set all*/ ) = 0;
			virtual void SetDiagnosticsColour( Apparance::Colour colour = Apparance::NoColour, int part_index = -1/*set all*/ ) = 0;
		};

		//logging service
		struct ILoggingService
		{
			virtual void LogMessage( const char* pszmessage ) = 0;
			virtual void LogWarning( const char* pszwarning ) = 0;
			virtual void LogError( const char* pszerror ) = 0;
		};

		//procedure run output recipient
		struct IResultHandler
		{
			virtual void HandleResult( struct Apparance::IClosure* procedure_output ) = 0;
		};

		//asset placement support
		struct IAssetDatabase
		{
			//find out how to instance the asset
			virtual bool CheckAssetValid(const char* pszassetdescriptor, int entity_context) = 0;	//is it actually mapped to an asset?
			virtual Apparance::AssetID GetAssetID( const char* pszassetdescriptor, int entity_context ) = 0; //get an ID, should still get one even if not mapped, i.e. to an asset that signifies 'missing asset'
			virtual bool GetAssetBounds( const char* pszassetdescriptor, int entity_context, Apparance::Frame& out_bounds ) = 0; //where applicable (e.g. mesh)
			virtual int GetAssetVariants( const char* pszassetdescriptor, int entity_context ) = 0;

			//dynamic procedural resource support
			virtual Apparance::AssetID AddDynamicAsset() { return Apparance::InvalidID; }
			virtual void SetDynamicAssetTexture( Apparance::AssetID /*id*/, const void* /*pdata*/, int /*width*/, int /*height*/, int /*channels*/, Apparance::ImagePrecision::Type /*precision*/ ) {}
			//virtual void SetGeometryAsset( Apparance::AssetID id, struct Apparance::Host::IGeometry* geometry ) {}
			virtual void RemoveDynamicAsset( Apparance::AssetID /*id*/ ) {}
		};
	}
}