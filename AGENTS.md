# Regla de trabajo del usuario

Después de aplicar y guardar cambios en código, compilar y ejecutar el proyecto
automáticamente con las herramientas de Visual Studio 2022. No dar por terminada
la tarea sin comprobar el resultado de la compilación y de la ejecución.

- Solución: `Proyecto.sln`. Configuración: `Release | x86`.
- MSBuild: `C:/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe`.
- Compilar: `MSBuild.exe Proyecto.sln /m /t:Build /p:Configuration=Release /p:Platform=x86 /v:minimal /nologo`.
- Ejecutable: `bin/Win32-Release/Proyecto_Equipo11.exe`.
- Ejecutar desde `Proyecto/` con `--validate obj/validation/nombre` para comprobar
  el escenario real, físicas y renderizado; comprobar código de salida cero.
- El usuario quiere los cambios guardados directamente y una respuesta breve,
  sin devolver código por el chat.
