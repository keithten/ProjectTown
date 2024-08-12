//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

using UnrealBuildTool;

public class ApparanceUnrealScripting : ModuleRules
{

	public ApparanceUnrealScripting(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		PrecompileForTargets = PrecompileTargetsType.Any;
		bPrecompile = true;

		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				//private plugin header folders
//				PluginPrivate( "Utility" ),
//				PluginPrivate( "Resources" ),
//				PluginPrivate( "Editing" ),
				}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"Engine",
				"UnrealEd",
				"AssetTools",
				"Kismet",	//for SContentReference
				
				// ... add other public dependencies that you statically link with here ...
				"ApparanceUnreal"
			}
			);
	
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"BlueprintGraph",	//UK2Node
				"KismetCompiler",	//FKismetCompilerContext
				}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);

	}

	/// <summary>
	/// helper to add path info needed for explicit private include subdirs
	/// </summary>
	/// <param name="name"></param>
	/// <returns></returns>
	static string PluginPrivate( string name )
	{
		return System.IO.Path.Combine( "ApparanceUnrealScripting", "Private", name );
	}

}
