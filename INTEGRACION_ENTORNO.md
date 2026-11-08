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
- Codos y rodillas con pesos propios y dos cortes adicionales de soporte en las
  extremidades para repartir la flexion sin doblar brazos desde los hombros.
- Codos de marcha entre 24 y 53 grados; de carrera entre 53 y 96 grados.
  Rodillas de marcha hasta 74 grados y de carrera hasta 108 grados.
- Contacto de las suelas horneado en cada keyframe para mantener los pies sobre
  el suelo durante la flexion y el balanceo del cuerpo.
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

## Tamano, velocidad y altura del terreno

`MovementSettings.h` centraliza los ajustes. La niña mide 2.8 unidades del modelo
fuente, aproximadamente el doble de la escala anterior. Con la matriz global x10,
su altura visible es de 28 unidades del mundo. Camina a 2.6 unidades fuente/segundo
y corre a 6.5; la camara libre usa 18 y 45 con Shift. El desplazamiento conserva
su integracion con deltaTime y la cadencia depende de la distancia recorrida.

`GroundSurface.h` guarda los triangulos transitables y los indexa en una rejilla.
La altura se consulta con nueve rayos verticales: centro y ocho extremos de la
huella. Se registran pisos, banquetas, tarimas, rampas, pasto y plataformas; se
excluyen techos, vegetacion decorativa y esculturas. Los pisos se detectan por
ambas caras, incluso cuando Blender exporta winding invertido.
La altura maxima de paso es el 80% de la altura de la niña: 2.24 unidades fuente,
comun a todos los pisos. Esto permite subir desde la calle (Y=0.843929) a la
banqueta (Y=2.038686) y al tapete de entrada del Pasillo (Y=1.7764). No existe el
antiguo limite de 0.4 que dejaba al personaje bajo esas superficies.
Al subir se ajusta inmediatamente para evitar penetracion; al bajar o perder
apoyo actua la gravedad y el aterrizaje se limita a la superficie real. Las rampas
se evaluan con interpolacion baricentrica de sus triangulos. La camara libre
congela tambien esta correccion de altura.

## Arboles Crossed Quads

Se localizaron los ocho arbustos del Zocalo agrupando su geometria por proximidad.
`Models/EntornoCompleto/Arboles.txt` guarda el centro de cada arbusto y una posicion
adyacente de arbol, separada aproximadamente 3.64 metros de ese centro.

`TreeVegetation.h` registra una sola textura RGBA en `AlphaBillboards.h`. Cada arbol se
dibuja dos veces en la misma posicion con una diferencia de 90 grados en Y:
ocho arboles y dieciseis dibujos de quad. La altura se adapta al suelo.
Se dibujan ambas caras y se usa recorte alfa a 0.1, con profundidad habilitada.

El render usa `images/Arbol_RGBA.png`. `CutoutImage.h` prepara una copia en RAM:
recorta margenes vacios, sustituye el color contaminado del contorno por colores
interiores y extiende esos colores bajo alfa cero antes de generar mipmaps.
Los PNG fuente no se modifican. Los quads usan GL_CLAMP_TO_EDGE.

## Fachadas y transparencia

`ZocaloBackdrop.h` mide la calle (Material.013), las cuatro franjas amarillas
(Material.016 a .019) y el borde oscuro exterior (Material.020). Coloca los planos
0.15 unidades fuente despues del limite externo de esas mallas; el piso interior
Material.006 se usa unicamente para calcular el centro de la plaza.
Las tres casas de `images/` se alternan en las cuatro caras y la iglesia aparece
una sola vez, en el centro de la cara norte. Todos los frentes apuntan hacia dentro.
La altura de la iglesia es 30 unidades fuente (300 unidades mundo); las casas
conservan su proporcion y se ajustan por tramo para cubrir el perimetro, con un
solape de 2.5% solo entre vecinos. El primer y ultimo quad terminan exactamente en
los extremos permitidos. Las bases se ajustan al terreno.

La cara sur reserva la conexion con el Pasillo: se mide el ultimo metro del tapete
`TapeteAserrin1.002` en Z y se deja libre todo su ancho mas 1.5 unidades a cada lado.
El cargador conserva unicamente los pocos vertices de ese material para esta
medicion. El generador construye dos tramos separados y no instancia ningun plano
en el hueco. La validacion comprueba tanto el ancho completo de cada fachada como
una huella de personaje de 0.4 unidades a lo largo del acceso.

`AlphaBillboards.h` comparte una sola geometria de quad entre arboles y fachadas.
`Shader/billboard.frag` descarta `texColor.a < 0.1` y conserva el alfa restante.
Los opacos y el skybox se dibujan primero. El pase de planos ordena todas las
instancias de lejos a cerca, habilita GL_BLEND con SRC_ALPHA/ONE_MINUS_SRC_ALPHA
y mantiene GL_DEPTH_TEST sin escribir profundidad. Al finalizar restaura la
escritura de profundidad y deshabilita la mezcla para el siguiente frame.

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
del quiosco. Miden **8 unidades fuente** (antes 5), controladas por LampHeight en
MovementSettings.h. El centro local del cristal se obtiene del material de la
farola. Su offset XYZ se vuelve a escalar y rotar antes de sumar la traslacion:
la luz acompaña tanto la nueva altura como el brazo lateral del foco. La prueba
compara cada luz con el centro del cristal transformado por la matriz completa.
La iluminacion tiene atenuacion y alcance de 28 unidades fuente en el Zocalo.

El arco mantiene estructura con Kd blanco puro y flores individuales con el atlas
`Pasillo/tex_florCempa.png`, comprobado contra sus UV.
El skybox conserva filtrado continuo y correccion de diferencias entre caras.

## Agua animada de la Laguna

`WaterSurface.h` crea un plano horizontal debajo de la tarima Material.027:
su nivel es el minimo Y de esa malla menos 0.4 unidades fuente. `WaterPlane.txt`
contiene los limites XZ medidos del material None.001 de `Laguna/Agua/Lago(Lag).obj`.
Se carga solo el plano; ese OBJ auxiliar contiene ademas toda la escena duplicada.

Se usa `Laguna/Agua/River_Water_Texture-427.jpg` con mipmaps, anisotropia y
GL_MIRRORED_REPEAT para que la foto no produzca un salto de color entre repeticiones.
Cada repeticion mide 36 unidades fuente de ancho y conserva la proporcion de los
7360 x 4912 pixeles. `Shader/water.frag` recibe el tiempo de glfwGetTime() cada frame
y mezcla dos capas con desplazamiento UV continuo y ondulacion suave. Una mezcla
de cuatro parches con offsets estables elimina la cuadricula visible de la foto;
textureGrad mantiene el LOD estable durante el desplazamiento. El agua
escribe profundidad y queda ocluida por las plataformas y esculturas; se dibuja
antes del skybox y de los planos transparentes.

## Validacion

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Proyecto.sln /m /t:Build /p:Configuration=Release /p:Platform=x86 /v:minimal /nologo
& .\bin\Win32-Release\Proyecto_Equipo11.exe --validate
```

`--validate` prueba movimiento a distintos FPS, diagonales, escala UV y bloqueo
de controles. Con el FBX real cargado, simula C y WASD y comprueba que las matrices
de huesos y la posicion permanezcan exactamente iguales mientras la camara se mueve.
Dibuja 330 frames con modelos, agua, arboles, fachadas, luces y ambas camaras; verifica profundidad
y errores de OpenGL. Mide la flexion de codos y rodillas en los clips importados,
prueba entrada/salida de pasto a 30/60/144 FPS y comprueba las tapas elevadas del
escenario real. Comprueba exactamente una iglesia, repeticion de las tres casas,
orientacion interior, cobertura exterior y ausencia de edificios en el acceso al
Pasillo. Recorre calle/banqueta/Pasillo en ambos sentidos a 30/60/144 FPS y prueba
rampas y pisos invertidos. Comprueba la altura y alineacion de las ocho luces.
Guarda vistas del paso, del personaje sobre la entrada y de la laguna. Una prueba
en GPU comprueba que el agua produce imagenes distintas en dos tiempos.
Una prueba de framebuffer
verifica en GPU el descarte alfa <0.1 y la mezcla de un pixel al 50% sobre el cielo.
`--validate-character` omite el entorno masivo y dibuja 140 frames.
Un segundo argumento opcional define el prefijo de capturas TGA.

La carga del OBJ de 325 MB puede tardar varios minutos. La navegacion ajusta
la altura al terreno, pero no incluye colisiones solidas contra paredes.
