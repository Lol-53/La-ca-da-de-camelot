#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UObject/UObjectIterator.h"

namespace TFGCppTestHelpers
{
	bool IsTFGRuntimeClass(const UClass* Class)
	{
		return Class &&
			Class->GetOutermost() &&
			Class->GetOutermost()->GetName() == TEXT("/Script/TFG") &&
			!Class->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists);
	}
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(
	FTFGCppClassSmokeTest,
	"TFG.Cpp.Classes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FTFGCppClassSmokeTest::GetTests(
	TArray<FString>& OutBeautifiedNames,
	TArray<FString>& OutTestCommands) const
{
	for (TObjectIterator<UClass> It; It; ++It)
	{
		if (TFGCppTestHelpers::IsTFGRuntimeClass(*It))
		{
			OutBeautifiedNames.Add(It->GetName());
			OutTestCommands.Add(It->GetPathName());
		}
	}
}

bool FTFGCppClassSmokeTest::RunTest(const FString& Parameters)
{
	UClass* Class = FindObject<UClass>(nullptr, *Parameters);
	const bool bClassExists = TestNotNull(TEXT("La clase C++ reflejada existe"), Class);
	if (bClassExists)
	{
		TestTrue(TEXT("Pertenece al modulo TFG"), TFGCppTestHelpers::IsTFGRuntimeClass(Class));
		TestNotNull(TEXT("La clase tiene clase padre"), Class->GetSuperClass());
		TestNotNull(TEXT("El objeto por defecto se puede construir"), Class->GetDefaultObject());
		TestFalse(TEXT("La clase no esta marcada como obsoleta"), Class->HasAnyClassFlags(CLASS_Deprecated));
	}
	return bClassExists;
}

#endif
