#include "MyCustomSceneToolset.h"

#include "IModelContextProtocolModule.h"
#include "Misc/CoreDelegates.h"
#include "ModelContextProtocolSettings.h"
#include "Modules/ModuleManager.h"
#include "ToolsetRegistry/UToolsetRegistry.h"

class FTFGEditorModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FCoreDelegates::GetOnPostEngineInit().AddRaw(this, &FTFGEditorModule::RegisterToolset);
	}

	virtual void ShutdownModule() override
	{
		FCoreDelegates::GetOnPostEngineInit().RemoveAll(this);
		if (UToolsetRegistry::IsAvailable())
		{
			UToolsetRegistry::UnregisterToolsetClass(UMyCustomSceneToolset::StaticClass());
		}
	}

private:
	void RegisterToolset()
	{
		UToolsetRegistry::RegisterToolsetClass(UMyCustomSceneToolset::StaticClass());

		// The MCP server is useful in the interactive editor, but starting it in a
		// cook commandlet conflicts with the already-open editor on port 8000 and
		// turns an otherwise successful cook into an Unknown Cook Failure.
		if (!IsRunningCommandlet())
		{
			if (IModelContextProtocolModule* MCPModule = IModelContextProtocolModule::Get())
			{
				MCPModule->StartServer(
					UE::ModelContextProtocol::GetServerPortNumber(),
					UE::ModelContextProtocol::GetServerUrlPath());
			}
		}
	}
};

IMPLEMENT_MODULE(FTFGEditorModule, TFGEditor)
