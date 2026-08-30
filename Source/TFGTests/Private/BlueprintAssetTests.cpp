#if WITH_DEV_AUTOMATION_TESTS

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Blueprint.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "Modules/ModuleManager.h"

namespace TFGBlueprintTestHelpers
{
	void FindMyContentBlueprints(TArray<FAssetData>& OutAssets)
	{
		FAssetRegistryModule& AssetRegistryModule =
			FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		IAssetRegistry& Registry = AssetRegistryModule.Get();
		Registry.SearchAllAssets(true);

		FARFilter Filter;
		Filter.PackagePaths.Add(FName(TEXT("/Game/MyContent")));
		Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
		Filter.bRecursivePaths = true;
		Filter.bRecursiveClasses = true;
		Registry.GetAssets(Filter, OutAssets);
		OutAssets.Sort([](const FAssetData& A, const FAssetData& B)
		{
			return A.PackageName.LexicalLess(B.PackageName);
		});
	}
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(
	FTFGBlueprintLoadTest,
	"TFG.Blueprints.LoadAndGenerate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FTFGBlueprintLoadTest::GetTests(
	TArray<FString>& OutBeautifiedNames,
	TArray<FString>& OutTestCommands) const
{
	TArray<FAssetData> Assets;
	TFGBlueprintTestHelpers::FindMyContentBlueprints(Assets);
	for (const FAssetData& Asset : Assets)
	{
		OutBeautifiedNames.Add(Asset.AssetName.ToString());
		OutTestCommands.Add(Asset.GetObjectPathString());
	}
}

bool FTFGBlueprintLoadTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *Parameters);
	if (!TestNotNull(*FString::Printf(TEXT("Se puede cargar %s"), *Parameters), Blueprint))
	{
		return false;
	}

	TestNotNull(TEXT("Tiene una clase padre valida"), Blueprint->ParentClass.Get());
	TestNotNull(TEXT("Tiene SkeletonGeneratedClass"), Blueprint->SkeletonGeneratedClass.Get());
	TestNotNull(TEXT("Tiene GeneratedClass"), Blueprint->GeneratedClass.Get());
	TestTrue(TEXT("No estaba guardado con errores de compilacion"), Blueprint->Status != BS_Error);
	if (Blueprint->GeneratedClass)
	{
		TestNotNull(TEXT("La clase generada construye su CDO"), Blueprint->GeneratedClass->GetDefaultObject());
	}
	return true;
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(
	FTFGBlueprintCompileTest,
	"TFG.Blueprints.Compile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FTFGBlueprintCompileTest::GetTests(
	TArray<FString>& OutBeautifiedNames,
	TArray<FString>& OutTestCommands) const
{
	TArray<FAssetData> Assets;
	TFGBlueprintTestHelpers::FindMyContentBlueprints(Assets);
	for (const FAssetData& Asset : Assets)
	{
		OutBeautifiedNames.Add(Asset.AssetName.ToString());
		OutTestCommands.Add(Asset.GetObjectPathString());
	}
}

bool FTFGBlueprintCompileTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *Parameters);
	if (!TestNotNull(*FString::Printf(TEXT("Se puede cargar %s"), *Parameters), Blueprint))
	{
		return false;
	}

	FKismetEditorUtilities::CompileBlueprint(
		Blueprint,
		EBlueprintCompileOptions::SkipGarbageCollection);
	TestTrue(*FString::Printf(TEXT("%s compila sin errores"), *Parameters), Blueprint->Status != BS_Error);
	TestNotNull(TEXT("La compilacion produce GeneratedClass"), Blueprint->GeneratedClass.Get());
	return Blueprint->Status != BS_Error && Blueprint->GeneratedClass != nullptr;
}

#endif
