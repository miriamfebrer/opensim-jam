set BIN=..\..\bin
set OPENSIM=..\..\opensim


REM Let Windows know where the plugin and opensim libraries are
set PATH=%BIN%;%OPENSIM%;%PATH%

REM Perform Forward Simulation
%BIN%\forsim %BIN%\jam_plugin.dll .\inputs\forsim_settings.xml
move out.log results\forsim_out.log
move err.log results\forsim_err.log

REM Perform Joint Mechanics Analysis
%BIN%\joint-mechanics %BIN%\jam_plugin.dll .\inputs\joint_mechanics_settings.xml 
move out.log results\joint_mechanics_out.log
move err.log results\joint_mechanics_err.log

pause