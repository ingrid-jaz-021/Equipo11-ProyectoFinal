# Festival de la Luz y de la Vida

Proyecto de gráficos 3D en C++17 y OpenGL 3.3 para Visual Studio 2022.

## Compilar y ejecutar

1. Instala **Desarrollo para el escritorio con C++**, MSVC v143 y Windows SDK.
2. Descarga todo el repositorio y abre **Proyecto.sln**.
3. Selecciona **Release | x86** (o **Debug | x86**) y ejecuta con **F5**.

Las bibliotecas necesarias están incluidas en `External Libraries/`. La compilación copia las DLL junto al ejecutable y configura el directorio de trabajo de los recursos.

El escenario original está incluido sin pérdida en `Proyecto/Models/EntornoCompleto/EntornoCompleto.zip`. MSBuild lo extrae automáticamente si falta el OBJ y verifica su SHA-256. No se necesitan Blender, Python, Git LFS ni descargas adicionales.

Desde la terminal de desarrollador de Visual Studio:

```powershell
MSBuild.exe Proyecto.sln /m /t:Build /p:Configuration=Release /p:Platform=x86
Set-Location Proyecto
..\bin\Win32-Release\Proyecto_Equipo11.exe
```

## Controles

| Acción | Controles |
| --- | --- |
| Mover a la niña | Flechas según la orientación de la cámara; Shift izquierdo para correr |
| Cambiar cámara | C |
| Mirar / ajustar vista | Ratón o touchpad con cursor capturado: órbita en tercera persona y vista en libre; rueda: distancia o campo de visión, respectivamente |
| Cámara libre | WASD; E subir, Q bajar; Shift izquierdo para acelerar |
| Acción del xolo | F, fuera del bote |
| Subir o bajar del bote | E cerca del bote, muelle u orilla |
| Navegar | W/S o Arriba/Abajo para avanzar o retroceder; A/D o Izquierda/Derecha para girar; Shift izquierdo para acelerar |
| Diálogo del ajolote | G para iniciar, avanzar y cerrar; a pie, cerca del ajolote |
| Reiniciar | R: niña, mascota y bote en tercera persona; posición de la cámara en libre |
| Liberar o capturar cursor / V-Sync / salir | Tab / V / Esc |

## Recursos y créditos

`Proyecto/Models/`, `images/`, `SkyBox/` y `Shader/` contienen los recursos activos. Los FBX de los personajes y el XANIM del danzante son necesarios para ejecutar el programa.

La atribución y los términos de las capturas de movimiento de CMU se conservan en `Proyecto/Models/NinaRig/mocap/`. Las bibliotecas incluidas conservan sus licencias originales.
