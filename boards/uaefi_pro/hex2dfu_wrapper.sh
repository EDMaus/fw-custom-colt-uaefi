#!/usr/bin/env bash

set -euo pipefail

tool_dir="$(cd "$(dirname "$0")/../../ext/rusefi/misc/encedo_hex2dfu" && pwd)"
tool_path="$(cygpath -w "$tool_dir/hex2dfu.exe")"

/c/WINDOWS/System32/WindowsPowerShell/v1.0/powershell.exe \
	-NoProfile \
	-Command '
		$exe = [string]$args[0]
		$toolArgs = @()
		if ($args.Length -gt 1) {
			$toolArgs = $args[1..($args.Length - 1)]
		}
		& $exe @toolArgs
		exit $LASTEXITCODE
	' \
	"$tool_path" \
	"$@"
