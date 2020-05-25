$ErrorActionPreference = 'Stop'

function Invoke-Utility {
<#
.SYNOPSIS
Invokes an external utility, ensuring successful execution.

.DESCRIPTION
Invokes an external utility (program) and, if the utility indicates failure by 
way of a nonzero exit code, throws a script-terminating error.

* Pass the command the way you would execute the command directly.
* Do NOT use & as the first argument if the executable name is not a literal.

.EXAMPLE
Invoke-Utility git push

Executes `git push` and throws a script-terminating error if the exit code
is nonzero.
#>
  $exe, $argsForExe = $Args
  $ErrorActionPreference = 'Continue' # to prevent 2> redirections from triggering a terminating error.
  try { & $exe $argsForExe } catch { Throw } # catch is triggered ONLY if $exe can't be found, never for errors reported by $exe itself
  if ($LASTEXITCODE) { Throw "$exe indicated failure (exit code $LASTEXITCODE; full command: $Args)." }
}

$build_types = "Release"
$lib_types = "static"

foreach ($build_type in $build_types) {
	foreach ($lib_type in $lib_types) {
		$directory = "build_$($lib_type.toLower())_$($build_type.toLower())"
		if (-not $(test-path $directory)) {
			mkdir $directory 
		}
		cd $directory
		$ci_name = "synthizer"
		if ($build_type -eq "Debug") { $ci_name += "d" }
		if ($lib_type -eq "static") { $ci_name += "_static" }
		if ($lib_type -eq "dynamic" ) { $lib_type = "shared" }
		invoke-utility cmake -G Ninja ".." `
			"-DCMAKE_C_COMPILER=clang-cl.exe" `
			"-DCMAKE_CXX_COMPILER=clang-cl.exe" `
			"-DCMAKE_BUILD_TYPE=$build_type" `
			"-DCI_SYNTHIZER_NAME=$ci_name" `
			"-DSYNTHIZER_LIB_TYPE=$($lib_type.toUpper())"
		invoke-utility ninja
		cd ..
	}
}