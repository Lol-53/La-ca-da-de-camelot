#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

IMPLEMENT_COMPLEX_AUTOMATION_TEST(
	FTFGCppSourceIntegrityTest,
	"TFG.Cpp.SourceFiles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FTFGCppSourceIntegrityTest::GetTests(
	TArray<FString>& OutBeautifiedNames,
	TArray<FString>& OutTestCommands) const
{
	TArray<FString> SourceFiles;
	IFileManager::Get().FindFilesRecursive(
		SourceFiles,
		*FPaths::Combine(FPaths::ProjectDir(), TEXT("Source")),
		TEXT("*.cpp"),
		true,
		false);

	SourceFiles.Sort();
	const FString SourceRoot = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"));
	for (const FString& File : SourceFiles)
	{
		if (!File.Contains(TEXT("/TFGTests/")) && !File.Contains(TEXT("\\TFGTests\\")))
		{
			FString RelativeFile = File;
			FPaths::MakePathRelativeTo(RelativeFile, *SourceRoot);
			RelativeFile.ReplaceInline(TEXT("\\"), TEXT("/"));
			OutBeautifiedNames.Add(RelativeFile);
			OutTestCommands.Add(File);
		}
	}
}

bool FTFGCppSourceIntegrityTest::RunTest(const FString& Parameters)
{
	FString Contents;
	const bool bFileLoaded = TestTrue(
		TEXT("El archivo C++ puede leerse"),
		FFileHelper::LoadFileToString(Contents, *Parameters));
	if (bFileLoaded)
	{
		TestFalse(TEXT("El archivo no esta vacio"), Contents.TrimStartAndEnd().IsEmpty());
		TestFalse(TEXT("No contiene un conflicto Git sin resolver"),
			Contents.Contains(TEXT("<<<<<<<")) || Contents.Contains(TEXT(">>>>>>>")));
	}
	return bFileLoaded;
}

#endif
