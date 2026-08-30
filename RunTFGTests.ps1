param(
	[string]$UnrealRoot = "C:\Program Files\Epic Games\UE_5.8",
	[string]$Project = "C:\Users\javie\Documents\Unreal Projects\TFG_V2 5.8\TFG.uproject"
)

$editor = Join-Path $UnrealRoot "Engine\Binaries\Win64\UnrealEditor-Win64-DebugGame-Cmd.exe"
$report = Join-Path (Split-Path $Project) "Saved\Automation\TFG"
$reportArgument = "-ReportExportPath=$report"

& $editor $Project `
	-Unattended `
	-NoSplash `
	-NoSound `
	-NullRHI `
	-NoP4 `
	-ExecCmds="Automation RunTests TFG; Quit" `
	-TestExit="Automation Test Queue Empty" `
	$reportArgument

exit $LASTEXITCODE
