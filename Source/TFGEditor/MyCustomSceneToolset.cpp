#include "MyCustomSceneToolset.h"

#include "EngineUtils.h"
#include "Engine/World.h"
#include "IPythonScriptPlugin.h"

int32 UMyCustomSceneToolset::CountActorsWithMesh(const FString& MeshName)
{
	int32 Count = 0;
	if (UWorld* World = GWorld)
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (AActor* Actor = *It; Actor && Actor->GetName().Contains(MeshName))
			{
				++Count;
			}
		}
	}
	return Count;
}

FString UMyCustomSceneToolset::RunPython(const FString& Code)
{
	FString ExecutionResult = TEXT("Python Script Plugin is not available.");
	if (IPythonScriptPlugin::Get() && IPythonScriptPlugin::Get()->IsPythonAvailable())
	{
		FPythonCommandEx PythonCommand;
		PythonCommand.Command = Code;
		PythonCommand.ExecutionMode = EPythonCommandExecutionMode::ExecuteFile;
		PythonCommand.FileExecutionScope = EPythonFileExecutionScope::Public;
		ExecutionResult = IPythonScriptPlugin::Get()->ExecPythonCommandEx(PythonCommand)
			? TEXT("Python command executed successfully. Check Output Log for details.")
			: TEXT("Failed to execute Python command. Check Output Log for errors.");
	}
	return ExecutionResult;
}
