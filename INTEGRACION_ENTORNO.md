# Proyecto Chignahuapan

Abrir `Proyecto.sln` en Visual Studio 2022 y usar **Release | x86**.
Ejecutable: `bin/Win32-Release/Proyecto_Equipo11.exe`.
La solucion esta ahora en `C:/Users/hugo1/Desktop/Proyecto_Equipo11/Equipo11-ProyectoFinal`,
fuera de OneDrive. `Proyecto/Proyecto.cpp` contiene el render loop.

## Controles y camaras

- **C:** alternar tercera persona y camara libre.
- **Tercera persona:** WASD mueve a la niña; Shift corre; raton orbita; rueda acerca/aleja.
- **Camara libre:** WASD vuela en la direccion de la mirada; E/Q sube/baja;
  Shift acelera y la rueda cambia el campo de vision.
- En camara libre la niña queda completamente congelada: posicion, orientacion,
  velocidad, correccion de altura, reloj de animacion y matrices de huesos.
  Los controles del personaje vuelven a activarse al pulsar C.
- **R:** en tercera persona reinicia la escena; en camara libre solo reinicia la camara.
- **Tab:** liberar/capturar cursor. **Esc:** salir.

## Esqueleto real de la niña

El render ya no utiliza ProceduralRig.h ni calcula pesos anatomicos durante la carga.
El modelo activo es **`Models/NinaRig/NinaRig.fbx`**. Blender genero:

- Una armadura jerarquica de 22 huesos; 17 tienen influencias deformantes.
- Pesos normalizados de hasta cuatro huesos por vertice, guardados en el modelo.
- Pose de enlace con brazos relajados, horquillas de hombro/clavicula y codos propios.
- Clips **Idle**, **Walk** y **Run** con keyframes de posicion, quaternion y escala.
- Versiones `.blend`, `.fbx` y `.glb`, con ocho texturas copiadas a nombres ASCII.

`SkeletalAnimator.h` importa la jerarquia, inverse bind matrices, pesos y canales
de Assimp; interpola traslaciones/escalas y usa slerp para rotaciones. Mezcla
reposo/marcha/carrera y genera la paleta de matrices. El vertex shader aplica
skinning a cuatro influencias y admite hasta 48 huesos. La conversion FBX de
unidades no cambia la cadencia, que se calcula respecto a la altura del personaje.
La geometria original y el archivo Blender original se conservan.

Para regenerar el rig con Blender instalado desde Microsoft Store, ejecutar desde
la carpeta de la solucion:

```powershell
& "$env:LOCALAPPDATA\Microsoft\WindowsApps\blender-launcher.exe" --background --python ".\Proyecto\tools\build_nina_rig.py"
```

El script crea el rig, aplica la pose de enlace relajada, asigna los grupos de
vertices, hornea los tres clips y exporta. Los pivotes estan ajustados a la niña
suministrada; un modelo de anatomia diferente requiere retocar este script.

## Arboles Crossed Quads

Se localizaron los ocho arbustos del Zocalo agrupando su geometria por proximidad.
`Models/EntornoCompleto/Arboles.txt` guarda el centro de cada arbusto y una posicion
adyacente de arbol, separada aproximadamente 3.64 metros de ese centro.

`TreeVegetation.h` carga una sola textura RGBA y un solo quad. Cada arbol se
dibuja dos veces en la misma posicion con una diferencia de 90 grados en Y:
ocho arboles y dieciseis dibujos de quad. La altura se adapta al suelo.
Se dibujan ambas caras y se usa recorte alfa a 0.1, con profundidad habilitada.

El archivo original `images/Arbol.png` contenia realmente un JPEG sin alpha y
con cuadricula pintada. Se conserva intacto. La copia `Arbol_RGBA.png` es un PNG
real con fondo transparente, bordes descontaminados y margen recortado.
Los quads usan GL_CLAMP_TO_EDGE para evitar repeticion en los bordes.

## Piso y filtrado

`Texture2D.h` carga RGBA, genera mipmaps, utiliza GL_LINEAR_MIPMAP_LINEAR y
habilita anisotropia hasta min(16, limite del equipo). El equipo de validacion
admite **16x**. Las texturas del entorno utilizan GL_REPEAT.

`FloorUV.h` reconstruye UV continuas a partir de posiciones del OBJ, en metros,
para los materiales del piso, calle, banquetas y jardineras del Zocalo.
La escala no depende de la matriz global x10 ni de los -s del MTL:

- Piso principal: una imagen de 600x400 cubre 2.4 x 1.6 metros.
- Calle: 4.0 x 2.667 metros.
- Banquetas: 1.5 x 2.251 metros.
- Piso de banqueta: 2.0 x 3.002 metros.
- Pasto: 1.5 x 2.0 metros.

La proyeccion elige XZ en caras superiores y planos verticales en los laterales.
Se mantienen las texturas fuente, evitando el estiramiento que producia superficies
borrosas y sin modificar las UV de personajes, flores u otros escenarios.

## Escenario, farolas y arco

El escenario unificado `Models/EntornoCompleto/EntornoCompleto.obj` se carga una vez.
Assimp importa en segundo plano; los recursos OpenGL se crean en el hilo principal.
Se omiten mallas fuera de vista y no se conservan copias de su geometria en RAM.

Las ocho farolas y sus luces se conservan: cuatro esquinas y cuatro alrededor
del quiosco. El centro del foco se calcula con el material de cristal y se
transforma con la matriz completa de cada farola. La iluminacion tiene
atenuacion y alcance finito en el Zocalo.

El arco mantiene estructura con Kd blanco puro y flores individuales con el atlas
`Pasillo/tex_florCempa.png`, comprobado contra sus UV.
El skybox conserva filtrado continuo y correccion de diferencias entre caras.

## Validacion

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Proyecto.sln /m /t:Build /p:Configuration=Release /p:Platform=x86 /v:minimal /nologo
& .\bin\Win32-Release\Proyecto_Equipo11.exe --validate
```

`--validate` prueba movimiento a distintos FPS, diagonales, escala UV y bloqueo
de controles. Con el FBX real cargado, simula C y WASD y comprueba que las matrices
de huesos y la posicion permanezcan exactamente iguales mientras la camara se mueve.
Dibuja 160 frames con modelos, arboles, luces y ambas camaras; verifica profundidad
y errores de OpenGL. `--validate-character` omite el entorno masivo y dibuja
130 frames. Un segundo argumento opcional define el prefijo de capturas TGA.

La carga del OBJ de 325 MB puede tardar varios minutos. La navegacion ajusta
la altura al terreno, pero no incluye colisiones solidas contra paredes.
