@echo off
echo ******************************************************
echo ** COPY STEP                                        **
echo ******************************************************

:path1
if not exist ..\..\build goto PathError1:
:path2
if not exist ..\..\build\bin goto PathError2:
:path3
if not exist "..\..\build\bin\%3" goto PathError3:
goto pathsok:

:PathError1:
echo Creating: build Folder
mkdir ..\..\build
goto path2:

:PathError2:
echo Creating: build\bin Folder
mkdir ..\..\build\bin
goto path3:

:PathError3:
echo Creating: build\bin\%3 Folder
mkdir "..\..\build\bin\%3"
goto pathsok:

:copypdb
echo.
echo = Copy Debug PDB's =
echo ====================
echo copy "%1\%3\*.pdb" "%2\%3"
copy "%1\%3\*.pdb" "%2\%3"
goto afterpdb



:pathsok
echo.
echo ==   Parameters   ==
echo ====================
echo        Source: %1
echo   Destination: %2
echo Configuration: %3
pause

echo Path 
cd

echo.
echo = Copy Server Exe's / Dll's =
echo =============================
echo From: %1\%3
echo   To: %2\%3 
echo =============================
copy %1\%3\ace.dll %2\%3
copy %1\%3\mangosd.exe %2\%3
copy %1\%3\realmd.exe %2\%3

echo.
echo = Copy Extraction Exe's =
echo =========================
echo From: %1\%3
echo   To: %2\%3 
echo =====================
copy %1\%3\map-extractor.exe %2\%3
copy %1\%3\movemap-generator.exe %2\%3
copy %1\%3\vmap-assembler.exe %2\%3
copy %1\%3\vmap-extractor.exe %2\%3
                         

if %3 == Debug goto copypdb:
:afterpdb

echo .
echo =   mangosd.conf   =
echo ====================
copy "..\..\src\mangosd\mangosd.conf.dist.in" "%2\%3"

echo .
echo =   realmd.conf   =
echo ====================
copy "..\..\src\realmd\realmd.conf.dist.in" "%2\%3"

echo .
echo =    ahbot.conf    =
echo ====================
copy "..\..\src\game\AuctionHouseBot\ahbot.conf.dist.in" "%2\%3"

echo .
echo = Extraction Scripts =
echo ======================
copy "..\..\src\tools\Extractor_Binaries\*.sh" "%2\%3"
copy "..\..\src\tools\Extractor_Binaries\*.txt" "%2\%3"
copy "..\..\src\tools\Extractor_Binaries\*.bat" "%2\%3"
pause

echo .
echo **********************
echo **  OpenSSL DLL's   **
echo **********************
copy "..\..\dep\lib\%4\libeay32.dll" "%2\%3"
copy "..\..\dep\lib\%4\ssleay32.dll" "%2\%3"

echo .
echo **********************
echo **   MySQL DLL's    **
echo **********************
copy "..\..\dep\lib\%4\libmysql.dll" "%2\%3"
pause

echo .
echo **********************
echo * Eluna base Scripts *
echo **********************
XCOPY "..\..\src\modules\Eluna\extensions" "%2\%3\lua_scripts\extensions" /E /I /F /Y

:exit

echo .
echo **********************
echo * Copy Step Complete *
echo **********************

pause