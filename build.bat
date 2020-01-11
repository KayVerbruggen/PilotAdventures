@echo off
IF NOT EXIST bin MKDIR bin
pushd bin

set cl_flags=-Oi -Z7 -nologo -Od -GR- -Gm- -EHa- -W3 -wd4505 -wd4100 -wd4189 -WX
set link_flags=-SUBSYSTEM:WINDOWS -opt:ref
set libs=user32.lib gdi32.lib xaudio2.lib xinput.lib xaudio2_9redist.lib xinput9_1_0.lib

cl  ..\src\mario.cpp %cl_flags% -Fe:mario.exe -link %linker_flags% %libs%

popd