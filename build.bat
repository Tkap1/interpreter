
@echo off

cls

if NOT defined VSCMD_ARG_TGT_ARCH (
	@REM Replace with your path
	call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" amd64
)

if not exist build\NUL mkdir build

set files=..\src\main.cpp
set comp=-nologo -std:c++20 -Zc:strictStrings- -W4 -wd4505 -FC -I ../../my_libs -Gm- -GR- -EHa-
set linker=user32.lib Shell32.lib "..\libdyncall_s.lib" -INCREMENTAL:NO -STACK:0x400000,0x400000
set comp=%comp% -wd 4244

set debug=2
if %debug%==0 (
	set comp=%comp% -O2 -MT
)
if %debug%==1 (
	set comp=%comp% -O2 -Dm_debug -MTd
)
if %debug%==2 (
	set comp=%comp% -Od -Dm_debug -Zi -MTd
)

pushd build
	cl %files% %comp% -link %linker% > temp_compiler_output.txt
popd
if %errorlevel%==0 goto success
goto fail

:success
copy build\main.exe main.exe > NUL
goto end

:fail

:end
copy build\temp_compiler_output.txt compiler_output.txt > NUL
type build\temp_compiler_output.txt

