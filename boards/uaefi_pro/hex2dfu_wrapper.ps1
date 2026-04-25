param(
	[Parameter(ValueFromRemainingArguments = $true)]
	[string[]] $ToolArgs
)

$tool = Join-Path $PSScriptRoot '..\..\ext\rusefi\misc\encedo_hex2dfu\hex2dfu.exe'
$tool = (Resolve-Path $tool).Path

& $tool @ToolArgs
exit $LASTEXITCODE
