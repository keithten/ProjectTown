//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

// enable for Apparance development library use
//#define ENABLE_DEVELOPMENT


using UnrealBuildTool;
using System.IO;

public class ApparanceUnreal : ModuleRules
{
	public ApparanceUnreal(ReadOnlyTargetRules Target) : base(Target)
	{
		// Apparance libraries
		AddApparanceLibraries( Target );

		//normal
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		PrecompileForTargets = PrecompileTargetsType.Any;
		bPrecompile = true;

		PrivateIncludePaths.AddRange(
			new string[] 
			{
				//private plugin header folders
				PluginPrivate( "Geometry" ),
				PluginPrivate( "Support" ),
			}
			);			

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				//higher level engine modules
				"Core",
				"Projects",
				"ProceduralMeshComponent",
			}
			);		
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{			
				//fundamental unreal modules
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
			}
			);		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
			}
			);
	}

	/// <summary>
	/// helper to add path info needed for explicit private include subdirs
	/// </summary>
	/// <param name="name"></param>
	/// <returns></returns>
	string PluginPrivate( string name )
	{
		return System.IO.Path.Combine( ModuleDirectory, "Private", name );
	}
	
	/// <summary>
	/// work out where they are and add lib refs and include path needed
	/// </summary>
	/// <param name="Target"></param>
	void AddApparanceLibraries( ReadOnlyTargetRules Target )
	{
		string module_path = ModuleDirectory;
		string intermediate_path = Path.GetFullPath( Path.Combine( module_path, "..", "..", "Source", "ThirdParty", "Apparance" ) );
		string include_dir = Path.Combine( intermediate_path, "inc" );
		string lib_dir = Path.Combine( intermediate_path, "lib" );

		//config
		bool is_lib_supported = false;
		if((Target.Platform==UnrealTargetPlatform.Win64))// || (Target.Platform==UnrealTargetPlatform.Win32))
		{
			//platform variants
			string platform_string = "Win64";

			//configuration
			string config_string; //shipping/test has none
			switch(Target.Configuration)
			{
				case UnrealTargetConfiguration.Debug:
				case UnrealTargetConfiguration.DebugGame:
					//debug can't currently be supported (Unreal still links the release CRT)
					//Development is unoptimised anyway so should be fine for debugging
				case UnrealTargetConfiguration.Development:
#if ENABLE_DEVELOPMENT
					config_string = "Development";
					break;
#endif//fallthrough..
				case UnrealTargetConfiguration.Shipping:
					config_string = "Shipping";
					break;
				default:
					throw new System.ApplicationException("Unsupported Apparance build configuration "+Target.Configuration );
			}

			//narrow down location
			lib_dir = Path.Combine( lib_dir, platform_string, config_string );
			is_lib_supported = true;
		}

		if(is_lib_supported)
		{
			PublicAdditionalLibraries.Add( Path.Combine( lib_dir, "ApparanceEngineLib.lib" ) );
			PublicAdditionalLibraries.Add( Path.Combine( lib_dir, "ApparanceCoreLib.lib" ) );
			PublicIncludePaths.Add( include_dir );

			//other libs used
			//PublicAdditionalLibraries.Add( Path.Combine( lib_dir, "libpng16.lib" ) );
			//PublicAdditionalLibraries.Add( Path.Combine( lib_dir, "zlib.lib" ) );
		}
	}
}
