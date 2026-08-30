#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "MyCustomSceneToolset.generated.h"

UCLASS(BlueprintType)
class TFGEDITOR_API UMyCustomSceneToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	UFUNCTION(meta=(AICallable), Category="Scene Tools")
	static int32 CountActorsWithMesh(const FString& MeshName);

	UFUNCTION(meta=(AICallable), Category="Scene Tools")
	static FString RunPython(const FString& Code);
};
