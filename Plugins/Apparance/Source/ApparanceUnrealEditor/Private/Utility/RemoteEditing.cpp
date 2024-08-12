//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

//main
#include "RemoteEditing.h"

#if false

//unreal
#include "Misc/Parse.h"

//module
#include "ApparanceUnreal.h"


// Helper class to find files.
struct FApparanceFileFinder : public IPlatformFile::FDirectoryVisitor
{
	//search pattern
	FString Search;

	//results
	FString FoundFirst;
	TArray<FString> FoundAll;

	FApparanceFileFinder(FString filename_filter)
	:Search(filename_filter)
	{
	}

	bool IsFound()
	{
		return !FoundFirst.IsEmpty();
	}

	virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory)
	{
		if (bIsDirectory == false)
		{
			FString Path( FilenameOrDirectory );
			FString Filename = FPaths::GetCleanFilename(Path);
			if (Filename.MatchesWildcard( Search ))
			{
				//first one
				if(!IsFound())
				{
					FoundFirst = Path;
				}

				//all
				FoundAll.Add( Path );
			}
		}
		return true;
	}
};


#if 1

/// <summary>
/// set up
/// </summary>
bool FApparanceRemoteEditing::Init()
{
	if(!FindEditorApplication())
	{
		return false;
	}


	return true;
}	
	
// done, close editor if open
//
void FApparanceRemoteEditing::Shutdown()
{
	if(IsEditorRunning())
	{
		
		FPlatformProcess::TerminateProc( EditorProcess );
	}
}


// search and resolve location of editor application
// should be somewhere relative to the project we are loaded into
//
bool FApparanceRemoteEditing::FindEditorApplication()
{
	//search for editor exe
	FString plugins_dir = FPaths::ProjectPluginsDir() / "ApparanceUnreal";
	FApparanceFileFinder search("Apparance.Editor.exe");
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	PlatformFile.IterateDirectoryRecursively( *plugins_dir, search );
	if(!search.IsFound())
	{
		UE_LOG(LogApparance,Display,TEXT("Failed to locate Apparance editor application within plugin folder: %s"), *plugins_dir);
		return false;
	}	

	//launch dir
	EditorExe = search.FoundFirst;
	EditorFolder = FPaths::GetPath( search.FoundFirst );

	return true;
}

// can we launch the editor:
// * have we found the exe location
// * do config settings prohibit it (as no server running in apparance engine)
//
bool FApparanceRemoteEditing::IsEditorAvailable() const
{
	return FApparanceUnrealModule::GetModule()->IsLiveEditingEnabled()
		&& !EditorExe.IsEmpty();
}

// determine if the editor application is already running on the system?
//
bool FApparanceRemoteEditing::IsEditorRunning() const
{
	if(!IsEditorAvailable())
	{
		return false;
	}

	//determine if process is running
	FProcHandle proc = EditorProcess;
	return EditorProcess.IsValid() && FPlatformProcess::IsProcRunning( proc );
}

// launch a new instance of the editor application
//
bool FApparanceRemoteEditing::LaunchEditor()
{
	if(!IsEditorAvailable())
	{
		return false;
	}
	if(IsEditorRunning())
	{
		return true;
	}

	//launch process
	bool success = false;

	uint32 process_id;
	EditorProcess = FPlatformProcess::CreateProc( *EditorExe, nullptr, true, false, false, &process_id, 0, *EditorFolder, nullptr, nullptr );
				
	return EditorProcess.IsValid();
}
#endif

// gather list of procedures
//
void FApparanceRemoteEditing::GetProcedures( TArray<FProcedureInfo>& procedures_out )
{
	procedures_out.Empty();

	//for now build from directory scan
	FApparanceFileFinder search("*.proc");
	FString path = FApparanceUnrealModule::GetModule()->GetProceduresPath();
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	PlatformFile.IterateDirectoryRecursively( *path, search );

	//gather info
	for(int i=0 ; i<search.FoundAll.Num() ; i++)
	{
		FString fullpath = search.FoundAll[i];
		FString filename = FPaths::GetBaseFilename(fullpath);

		//read start of file to extract ID
		IFileHandle* handle = PlatformFile.OpenRead( *fullpath );
		const uint32 header_size = 128;
		ANSICHAR header[header_size+1];
		if(handle->Read( (uint8*)header, header_size ))
		{
			uint32 file_size = (uint32)handle->Size();
			uint32 end = FMath::Min( file_size, header_size );
			header[end] = '\0';
			delete handle;

			//analyse
			const ANSICHAR* ptypesearch = FCStringAnsi::Strifind( header, "type=\"" );
			if(ptypesearch)
			{
				ANSICHAR* ptype = header + (ptypesearch-header);
				ANSICHAR* pid = ptype+6;
				//terminate id string
				pid[8] = '\0';
			
				//parse hex id
				FUTF8ToTCHAR converter( pid );			
				uint32 id = FParse::HexNumber( converter.Get() );
					
				//record this proc
				FProcedureInfo info;
				info.FullName = filename;
				info.ID = id;
				procedures_out.Add( info );
			}
		}		
	}

}
#endif
