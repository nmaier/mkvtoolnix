@echo off
if not exist %1* goto notfound
cd %1*
if exist %1_* goto subdir
cd ..
if exist %1_* goto rename

goto done
:rename
if exist %1 goto done
move %1_* %1 >NUL
goto done
:subdir
move %1_* ..\%1 >NUL
cd ..
goto done
:notfound
echo %1 not found!
:done

