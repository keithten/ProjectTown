//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

#pragma once

#if false
//unreal

//module


/// <summary>
/// interfacing with the external editor application
/// </summary>
class FApparanceRemoteEditing
{
	FString EditorFolder;
	FString EditorExe;

	FProcHandle EditorProcess;

public:
	//structs
	struct FProcedureInfo
	{
		FString FullName;
		uint32  ID;
	};
	
public:
	bool Init();
	void Shutdown();

	bool IsEditorAvailable() const;
	bool IsEditorRunning() const;
	bool LaunchEditor();

	void GetProcedures( TArray<FProcedureInfo>& procedures_out );

private:
	bool FindEditorApplication();

};


#endif
