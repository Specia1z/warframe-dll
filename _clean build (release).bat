@echo off
where llvm-rc >nul 2>&1 || (echo Missing llvm-rc; release build stopped. & exit /b 1)
where sun >nul 2>&1 || (echo Missing sun; release build stopped. & exit /b 1)

tools\pluto tools\make_version_rc.pluto || exit /b 1
llvm-rc version.rc || exit /b 1
del version.rc

php archive.php || exit /b 1

sun _release || exit /b 1
if exist dwmapi.exp del dwmapi.exp
if exist dwmapi.lib del dwmapi.lib
if exist version.res del version.res

REM tools\pluto tools\embed_checksum.pluto
tools\upx -9 dwmapi.dll || exit /b 1
