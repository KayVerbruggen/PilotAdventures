@echo off
IF NOT EXIST bin MKDIR bin

set cl_flags=-Oi -Z7 -nologo -O2 -GR- -Gm- -EHa- -W3 -wd4505 -wd4100 -wd4189 -WX -D_CRT_SECURE_NO_WARNINGS
set link_flags=-SUBSYSTEM:WINDOWS -opt:ref -KEYFILE:"cert.pfx"
set libs=user32.lib gdi32.lib xaudio2.lib xinput.lib Icons.res

cl src\pilot.cpp %cl_flags% -Fe:pilot.exe -link %linker_flags% %libs%