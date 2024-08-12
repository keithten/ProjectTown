//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

using UnrealBuildTool;

public class ApparanceUnrealEditor : ModuleRules
{

	public ApparanceUnrealEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				//private plugin header folders
				PluginPrivate( "Utility" ),
				PluginPrivate( "Resources" ),
				PluginPrivate( "Editing" ),
				}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"Projects",
				//"UnrealEd",
				"Engine",
				"AssetTools",
				"EditorStyle",
				"Kismet",	//for SContentReference
				"ProceduralMeshComponent", //for stats gathering
				
				// ... add other public dependencies that you statically link with here ...
				"ApparanceUnrealScripting",
				"ApparanceUnreal"
			}
			);
		if(Target.Version.MajorVersion>=5)
		{
			PublicDependencyModuleNames.Add( "EditorFramework" ); //for toolkits
		}

		
		if(Target.Type==TargetRules.TargetType.Editor)
		{
			PublicDependencyModuleNames.Add( "UnrealEd" );
		}
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"ApplicationCore",
				"CoreUObject",
				"ApplicationCore",
				"Slate",
				"SlateCore",
				"PropertyEditor",
				"EditorWidgets",
				"InputCore",
				"AppFramework",		//SColorPicker
				"BlueprintGraph",	//UK2Node
				"KismetCompiler",	//FKismetCompilerContext
				"PlacementMode",	//FPlaceableItem & IPlacementModeModule
				"MainFrame",        //for opening windows (IMainFrameModule)
				"RenderCore",		//for linking in GWhiteTexture
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
		return System.IO.Path.Combine( "ApparanceUnrealEditor", "Private", name );
	}

}
