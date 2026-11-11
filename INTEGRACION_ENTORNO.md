# Proyecto Chignahuapan

Abrir `Proyecto.sln` en Visual Studio 2022 y usar **Release | x86**.
Ejecutable: `bin/Win32-Release/Proyecto_Equipo11.exe`.
La solucion esta ahora en `C:/Users/hugo1/Desktop/Proyecto_Equipo11/Equipo11-ProyectoFinal`,
fuera de OneDrive. `Proyecto/Proyecto.cpp` contiene el render loop.

## Controles y camaras

- **C:** alternar tercera persona y camara libre.
- **G:** hablar con el axolote de la entrada y avanzar sus tres mensajes; otra
  pulsacion cierra el ultimo. Al alejarse, desaparece y reinicia para la siguiente visita.
- **V:** alternar V-Sync. Inicia desactivado (`glfwSwapInterval(0)`).
- **E en tercera persona:** embarcar cerca del casco; desembarcar solo junto a
  suelo firme del muelle, plataformas u orillas. En camara libre E sigue subiendo.
- **Navegando:** W/S avanza/retrocede; A/D gira el timon; Shift acelera.
  La niña y el xolo viajan inmoviles dentro del casco y la camara sigue el bote.
- **F:** el xolo pausa el seguimiento, olfatea, gira y da un paso de carrera;
  al terminar vuelve automaticamente al lado de la niña. Funciona tambien desde
  la camara libre y no mueve ni anima a la niña congelada. Se desactiva a bordo.
- **Tercera persona:** WASD mueve a la niña; Shift corre; raton orbita; rueda acerca/aleja.
- **Camara libre:** WASD vuela en la direccion de la mirada; E/Q sube/baja;
  Shift acelera y la rueda cambia el campo de vision.
- En camara libre la niña queda completamente congelada: posicion, orientacion,
  velocidad, correccion de altura, reloj de animacion y matrices de huesos.
  Los controles del personaje vuelven a activarse al pulsar C.
  Si se esta navegando, C tambien detiene y congela el bote y ambos pasajeros;
  al volver se recupera el control del barco, sin activar la caminata.
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
- Los clips activos proceden de captura humana CMU: Walk 07_01, Run 09_01 e
  Idle 77_02. Sustituyen los ciclos de extremidades sintetizados con senos.
- Retarget por direcciones articulares y marcos anatomicos, correccion de contacto
  de suelas y alineacion de la fase del pie izquierdo entre marcha y carrera.
- Codos de marcha aproximadamente 6 a 45 grados y rodillas hasta 70; en carrera
  conservan flexion variable hasta 86 grados y las rodillas hasta 116.
- Contacto de las suelas horneado en cada keyframe para mantener los pies sobre
  el suelo durante la flexion y el balanceo del cuerpo.
- Versiones `.blend`, `.fbx` y `.glb`, con ocho texturas copiadas a nombres ASCII.

`SkeletalAnimator.h` importa la jerarquia, inverse bind matrices, pesos y canales
de Assimp; interpola traslaciones/escalas y usa slerp para rotaciones. Mezcla
reposo/marcha/carrera y genera la paleta de matrices. El vertex shader aplica
skinning a cuatro influencias y admite hasta 48 huesos. La conversion FBX de
unidades no cambia la cadencia, que se calcula respecto a la altura del personaje.
La geometria original y el archivo Blender original se conservan.

El modelo activo restaura la playera EXACTA del OBJ original: 290 vertices,
272 caras y las mismas UV. No se alarga, afina ni subdivide; su union rigida con
Chest impide que los brazos estiren la tela. El manifiesto guarda dos SHA-256
iguales de posiciones/topologia/UV, antes y despues de generar la pose de enlace.
Solo se modifica la malla de piel de brazos y manos. Se midio la muñeca REAL del
OBJ en X=0.39: el mapa anterior ponia esa zona de palma en el antebrazo. Ahora el
humero mide 0.115 y el antebrazo 0.105, con grosor al 95% del original. Se recupera
longitud de brazo y se reduce TODA la palma/dedos al 40% en XYZ a partir de esa
muñeca, con transicion suave. La mano visible mide aproximadamente
0.079 x 0.023 x 0.087 unidades del modelo. Sus pesos pertenecen al hueso Hand;
el manifiesto conserva estas medidas y la verificacion de tamaño. La longitud
hombro-palma es 0.304, frente a 0.235 en la version demasiado corta.
Los sockets permanecen en X=+/-0.17, dentro de las mangas
originales; las claviculas conservan su posicion relativa al pecho durante el
retarget, evitando que la captura adulta ensanche los hombros. La raiz de piel
se mezcla gradualmente con Chest. Se exportan los cambios a FBX, GLB y BLEND.
`shoulder-clearance-audit.json` comprueba por interseccion de triangulos las
superficies deformadas de brazos y torso en 230 fotogramas de los tres clips;
excluye la union de pesos compartidos y el socket bajo las mangas cortas originales.

`arm-limits.txt` es el perfil compartido por Blender y C++: balanceo del humero
entre -30 y 45 grados, abduccion 18 a 28 y codo entre 6 y 100 grados. El retarget
comprime suavemente los gestos mas cerrados del adulto para conservar su variacion
sin pegar todo el ciclo al limite. Se mantienen munecas y palmas por debajo del
hombro con margen para el volumen real de las manos. La proteccion de
`SkeletalAnimator.h` se aplica DESPUES de toda mezcla y antes de propagar hijos,
por lo que tambien limita poses intermedias y overlays. `arm-safety-audit.json`
registra la altura maxima de vertices de antebrazos/manos en todos los keyframes.
Los dos ultimos valores del perfil mantienen antebrazos y palmas hacia el lado
exterior del torso; se aplican tanto en Blender como despues de mezclar en C++.

Para regenerar el rig con Blender instalado desde Microsoft Store, ejecutar desde
la carpeta de la solucion:

```powershell
& "$env:LOCALAPPDATA\Microsoft\WindowsApps\blender-launcher.exe" --background --python ".\Proyecto\tools\build_nina_rig.py"
```

El script crea el rig y pesos, y llama a `tools/mocap_retarget.py` para retargetear
los BVH incluidos en `Models/NinaRig/mocap`. Se hornean a 60 FPS, sin desplazamiento
horizontal de raiz, y se corrigen las uniones de los ciclos. El controlador ajusta
la cadencia a la distancia recorrida y mezcla los clips con quaternions, con un
limite de 1.7 ciclos/s andando y 2.8 corriendo para evitar acelerar excesivamente
las articulaciones al navegar mas rapido. WalkSpeed=5.2 y RunSpeed=11.5 unidades
fuente/s (antes 2.6 y 6.5); la integracion usa deltaTime y mantiene la misma
distancia a 30/60/144 FPS. El xolo acelera tambien para conservar el seguimiento.
`mocap-retarget.json` guarda los recortes y fases de las capturas. La atribucion y
los terminos originales se conservan en `mocap/ATTRIBUTION.md` y `READMEFIRST.txt`.
Fuente: https://mocap.cs.cmu.edu/ ; conversion BVH:
https://github.com/una-dinosauria/cmu-mocap .

## NPC sentado y Pan de Muerto

El modelo fuente de `Models/Persona Kiosko/` se conserva. El activo es
`Models/KioskNpcRig/KioskNpcRig.fbx`, generado por `tools/build_kiosk_npc.py`:
armadura de 25 nodos (17 deformantes), pesos suaves y seis clips sentados.
No reutiliza los limites de brazos de la niña. Se doblan muslos, rodillas,
hombros, codos y muñecas, manteniendo las piernas sentadas durante todo el ciclo.
Se copian sus cuatro texturas y se guarda tambien el Blender editable.

El cargador mide el asiento horizontal y el respaldo de cada una de las ocho
bancas. El NPC utiliza la mas cercana al punto inicial. Su orientacion sigue
el frente de la banca; NpcSeatAnchor se coloca sobre el asiento y la escala se
calcula para apoyar los pies en el piso, sin depender de la altura de una pose T.

`KioskNpc.h` implementa cuatro estados que conservan el tiempo sobrante entre
transiciones, independientemente de los FPS:

- Idle (4.2 s): 3 s sentado y 1.2 s alcanzando la canasta mediante ReachBasket.
- Aparicion (0.6 s): se activa el render del pan sobre esa mano.
- Comiendo (4.5 s): 1.2 s elevando el pan y 3.3 s reduciendo su escala de 1 a 0.05.
- Fin (1.4 s): se oculta completamente el pan y vuelve la mano al regazo.

Al terminar vuelve a Idle. El pan real de `Models/Pan de muerto/PanMuerto.obj`
usa su MTL/textura y una matriz padre obtenida de NpcHand.R, con offset de palma
y orientacion de la base del pan. No se mueve con una posicion fija independiente
del esqueleto. Solo se dibuja en los estados Aparicion/Comiendo.

La canasta `Models/canasta.obj`/`canasta.mtl` es procedural, con base, tiras de
mimbre abiertas y asa arqueada; se regenera con `tools/build_canasta.py`.
BasketMatrix la apoya sobre el asiento, junto al lado derecho del NPC. Su
posicion coincide con el final de ReachBasket, donde aparece el pan.

Se corrigieron los pivotes del codo y la muñeca segun la malla original. La IK
de dos segmentos usa un plano de codo estable hacia fuera; las trayectorias
canasta/boca/regazo son curvas Bezier con aceleracion y frenado suaves. Al tomar
el pan, el torso se inclina 6 grados lateralmente y 3 hacia delante sin mover
las piernas. La flexion de muñeca esta limitada a 30 grados; la pronacion se
distribuye en antebrazo (hasta 45) y muñeca (hasta 15). Los pesos de los vertices
se reasignaron a los nuevos pivotes. El manifiesto registra rangos articulares
y rotacion maxima por keyframe: menos de 4.8 grados a 60 FPS, sin saltos de giro.

### Dialogo de Ivanovich

`NpcDialogue.h` mide la distancia de la niña al NPC, con radio horizontal de
4.5 unidades fuente y diferencia vertical maxima de 3. Al acercarse aparece
automaticamente un panel 2D; al salir del radio se deja de dibujar. La camara
libre no se usa como disparador: cuenta la posicion real de la niña.

El texto es exactamente: **¡Qué onda, soy Ivanovich! Este pan de muerto está
buenísimo... ¡CU, CU, PUMAS!**

`images/IvanovichDialogo.png` contiene la frase Unicode rasterizada en Segoe UI
a 1600x340, con fondo oscuro semitransparente y contraste alto. Se regenera con
`tools/build_npc_dialogue.ps1` (UTF-8 con BOM, compatible con Windows PowerShell).
No requiere instalar FreeType ni cargar fuentes al ejecutar. Los shaders de
dialogo ajustan el panel a la resolucion, mantienen su proporcion y lo dibujan
sin profundidad al final del render. Es una sola draw call cuando esta visible.

## Xoloitzcuintle acompanante

El modelo activo es `Models/XoloRig/XoloRig.fbx`, con 24 huesos deformantes y
cuatro clips: Idle, Walk, Run y Special. Tiene codos, rodillas, corvejones, patas,
cuello, cabeza, orejas y tres segmentos de cola con pesos normalizados (maximo
cuatro influencias). Se conserva el OBJ original y se corrigieron sus rutas MTL
absolutas a rutas relativas; el FBX utiliza dos texturas ASCII dentro de XoloRig.

`CompanionController.h` mantiene un objetivo 1.8 unidades fuente al costado y
0.8 detras de la niña. Usa anticipacion de velocidad, aceleracion amortiguada,
giro por el angulo mas corto, subpasos de 120 Hz y la misma deteccion de suelo.
El perro mide 1.4 unidades fuente y acelera hasta 9 para alcanzar a la niña.
La distancia inicial de tercera persona es 8 unidades fuente para mostrar a ambos.
Cada modelo carga su propia jerarquia y paleta de matrices; DrawObject recibe
explicitamente el actor correspondiente antes de subir sus huesos al shader.

F inicia una sola reproduccion del clip Special (3.8 segundos):

- 0 a 1.6 s: flexion de patas delanteras, cuello y cabeza para olfatear.
- 1.42 a 1.65 s: la cabeza anticipa la direccion; despues comienza el pecho.
- 1.65 a 2.70 s: giro de 360 grados con desfases entre cabeza, cuello, pecho,
  columna y cadera. Las patas alternan apoyos plantados y pasos cortos elevados;
  cada pata tiene su propia fase, orientacion y solucion IK. El cuerpo se agacha
  e inclina ligeramente y la cola acompaña el movimiento.
- 2.70 a 3.8 s: un paso de carrera, con traslacion interpolada de 1.6 unidades.

Durante la accion el objetivo de seguimiento queda pausado. Se mezclan las poses
al entrar/salir; Special se limita a su ultimo keyframe y no entra en bucle.
Despues se recupera el objetivo lateral sin teletransportar al perro.
La nariz queda aproximadamente a 5.7 cm fuente del piso en la fase de olfateo.

Para regenerarlo, ejecutar `tools/build_xolo_rig.py` con blender-launcher.exe y
los mismos argumentos de background usados para la niña. Se producen FBX, GLB,
BLEND y `rig-manifest.json`; los originales del usuario permanecen disponibles.

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
Las mallas estaticas grandes se dividen en sectores de 16 unidades fuente. Cada
sector conserva sus limites geometricos completos; `glMultiDrawElements` envia
solo los sectores visibles, manteniendo un VBO/EBO por material. Asi una hoja o
arbusto visible ya no obliga a enviar toda la vegetacion del Zocalo.

El cargador separa las ocho bancas fusionadas de LoD_0SG por su ubicacion. Cada
banca usa una matriz local con escala **0.45**, anclada al pie y al nivel del piso;
no se reduce el anillo completo de posiciones alrededor de la plaza. El kiosko
Material.014 conserva escala **1.6** alrededor del centro a nivel del suelo.
La fuente (.004 cuenco, .005 agua y .007 columna) tiene OTRA matriz local,
anclada en su propio centro con escala **0.80** (mitad del tamano anterior).
Su ancho es 8.5368 unidades fuente y cabe holgadamente dentro de los pilares.
Las cotas y los triangulos usados para detectar altura reciben las mismas
transformaciones que el render.

Hay **12 farolas**: las ocho interiores (cuatro esquinas y cuatro alrededor del
quiosco) mas cuatro nuevas en las esquinas exteriores, inmediatamente despues
de las franjas amarillas .016 a .019 y dentro de la calle oscura .020. Se mide el
radio de la base real para que quede entera fuera de la linea, con 0.15 unidades
de margen. Las cuatro del anillo se separan segun el kiosko ya ampliado.
Miden **12 unidades fuente** (antes 8), controladas por LampHeight en
MovementSettings.h. El centro local del cristal se obtiene del material de la
farola. Su offset XYZ se vuelve a escalar y rotar antes de sumar la traslacion:
la luz acompaña tanto la nueva altura como el brazo lateral del foco. La prueba
compara cada luz con el centro del cristal transformado por la matriz completa.
El nuevo offset vertical es 9.40048 unidades fuente. El shader admite las doce
Point Lights; la iluminacion tiene atenuacion y alcance de 38 unidades fuente
dentro de los limites exteriores del Zocalo.

El arco mantiene estructura con Kd blanco puro y flores individuales con el atlas
`Pasillo/tex_florCempa.png`, comprobado contra sus UV.
El skybox conserva filtrado continuo y correccion de diferencias entre caras.

## Agua animada de la Laguna

`WaterSurface.h` crea un plano unitario horizontal debajo de la tarima Material.027
y del muelle de conexion. Su nivel queda 0.4 unidades bajo la tarima y al menos
0.25 bajo los tablones. `WaterPlane.txt`
contiene los limites XZ medidos del material None.001 de `Laguna/Agua/Lago(Lag).obj`.
Se carga solo el plano; ese OBJ auxiliar contiene ademas toda la escena duplicada.
El cargador conserva tambien el pequeno perfil `tarima_madera.001` del muelle.
Se seleccionan sus dos tramos del lado Laguna (X positivo) y se unen sus limites
con los de la cuenca. La superficie alcanza ahora X=4.986202, frente al antiguo
69.67717; cubre el muelle completo de X=4.986202 a 73.178017. El plano mide
434.329 x 369.6384 unidades fuente y su centro se desplaza hacia el Pasillo.
La matriz usa translate al nuevo centro y scale independiente en X/Z. Se verifican
las cuatro esquinas del muelle contra la matriz inversa del plano.

Se usa `Laguna/Agua/River_Water_Texture-427.jpg` con mipmaps, anisotropia y
GL_MIRRORED_REPEAT para que la foto no produzca un salto de color entre repeticiones.
Cada repeticion mide 36 unidades fuente de ancho y conserva la proporcion de los
7360 x 4912 pixeles. Las UV se anclan a posiciones fuente para ampliar sin estirar
ni mover la textura. `Shader/water.frag` recibe el tiempo de glfwGetTime() cada frame
y mezcla dos capas con desplazamiento UV continuo y ondulacion suave. Una mezcla
de cuatro parches con offsets estables elimina la cuadricula visible de la foto;
textureGrad mantiene el LOD estable durante el desplazamiento. El agua
escribe profundidad y queda ocluida por las plataformas y esculturas; se dibuja
antes del skybox y de los planos transparentes.

## Geometria y madera del muelle

`DockSurface.h` alinea las esquinas XZ y niveles Y de los dos bloques positivos
en X de `tarima_madera.001`; elimina la desviacion de las esquinas de la tabla
estrecha. El mismo arreglo se usa en render, raycasting, medicion del agua y
amarre del bote. La tarima de la ofrenda occidental conserva su geometria y foto.

Los 20 triangulos del muelle tienen normales por cara y UV nuevas, separadas
en los cantos para evitar proyecciones estiradas. Una repeticion cubre 1.6 x 3.2
unidades fuente: aproximadamente 42.6 x 21.25 repeticiones sobre el conjunto.
`images/MuelleMadera.png` es una textura procedural de 1024x2048 con veta,
tablones y juntas, regenerable con `tools/build_dock_texture.py` (Python estandar).
Usa GL_REPEAT, mipmaps trilineales y anisotropia mediante Texture2D.h.

## Cerca perimetral de la Laguna

`LakeFence.h` utiliza `images/Cerca_lago.png` en el pase compartido de quads RGBA.
Rodea las cuatro caras del plano real del agua, 0.15 unidades hacia fuera, con
146 paneles de altura visible 3.5 y juntas interiores solapadas 0.01 unidades.
Se conserva el canal alfa, se recortan solo margenes transparentes en RAM y se
usa descarte <0.1, GL_BLEND y SRC_ALPHA/ONE_MINUS_SRC_ALPHA. No se duplica textura
por panel. Cada tramo se divide segun la proporcion visible de la imagen.

La cara oeste se divide en DOS tramos: se mide TODO el muelle positivo en X de
tarima_madera.001 y se reserva su intervalo Z mas 1.5 unidades a cada lado.
La abertura queda Z=-64.391235..6.598602. Ningun quad entra en ese intervalo;
se verifican los extremos reales de sus matrices, la continuidad en las otras
caras y el ancho de paso con la huella del personaje. Las capturas 305/315
revisan el acceso en ambos sentidos, 325 la baranda y 335 la fuente reducida.

## Bote y navegacion

`Models/barquito.obj` contiene 412 vertices y 108 caras (196 triangulos despues
de Assimp), generados directamente: casco con quilla, fondo interior, borda,
tablones y dos bancos. `barquito.mtl` define cinco tonos cafe madera. No necesita
texturas externas. Mide 4.6 de ancho por 7.7 de largo; su linea de flotacion local
es Y=0 y la quilla llega a Y=-0.45. Se importa una sola vez antes del render loop.

`BoatNavigation.h` administra Caminando/Navegando. El amarre inicial se mide
desde el borde este del muelle: centro fuente aproximadamente
(77.828, 0.514682, -28.896), con proa hacia +X. E usa proximidad a la AABB del
casco ampliada 2.5 unidades y comprueba tambien altura. A bordo se cancelan
caminata/accion del perro, se congela la pose Idle de los dos esqueletos y se
colocan ambos sobre el fondo del casco usando su transformacion completa.

W/S controla velocidad (8 unidades/s, 12 con Shift, marcha atras 4); A/D controla
velocidad angular amortiguada. Se integran aceleracion y giro con deltaTime y
subpasos. La traslacion Y se asigna EXACTAMENTE al nivel del agua cada frame.
Se comprueba todo el casco contra limites, tarimas y orillas antes de aceptar
movimiento/rotacion; el bote no atraviesa las plataformas ni sale del lago.

Se agregan franjas de desembarque de madera de 4 unidades de ancho junto a la
cerca, dentro de las cuatro orillas, sin cerrar la entrada del muelle. Su suelo
esta 0.65 unidades por encima del agua y sus triangulos se registran en el mismo
GroundSurface usado por los personajes. Al pulsar E se buscan dos posiciones
con soporte completo para niña y perro, incluida su huella y el offset de
seguimiento. Si no existe suelo cercano, se permanece navegando. Al bajar,
se restauran controles y seguimiento, y el bote queda inmovil en su ultima
posicion. El movimiento a pie se detiene al intentar entrar en agua sin suelo.

El bloqueo del acceso se debia a reutilizar FullSupport, que exige que todos los
rayos de apoyo tengan menos de 0.45 unidades de diferencia de altura. En la union
Pasillo/muelle el escalon mide aproximadamente 0.716 y los rayos tocaban ambos
pisos. GuardWalking usa ahora apoyo parcial y la altura de paso del controlador,
seguida por la misma correccion vertical de GroundSurface. FullSupport solo se
usa para colocar ambos personajes de forma segura al desembarcar. La prueba
camina desde 12 unidades antes de la tabla hasta el trigger, sin teletransportar
al personaje al borde, y exige cero bloqueos a 30/60/144 FPS.

La flecha del bote es una piramide invertida de seis triangulos, creada en
BoatNavigation.h. Su material amarillo emisivo permanece brillante de noche.
La matriz sigue el centro del casco y agrega 4.9 + 0.55*sin(1.8*glfwGetTime())
unidades de altura, sin alterar la Y fisica del barco. Se descarta por frustum
independientemente del casco y no necesita cargar otro modelo o textura.

## Rendimiento del render loop

`Frustum.h` comparte planos normalizados entre entorno, farolas y billboards.
Se descartan sectores estaticos, cada farola y cada plano de casas/arboles/cerca;
tambien se omiten pasajeros, bote, orillas y agua fuera del campo de vision.
Las cajas son conservadoras e incluyen el ancho completo de los quads y las
transformaciones locales, evitando cortes o desapariciones en los bordes.

Las farolas visibles usan glDrawElementsInstanced: dos lotes (metal/cristal).
Los planos se compactan en un VBO de instancias y se dibujan por grupos contiguos
de textura. Se conserva el orden GLOBAL de lejos a cerca, el alfa original y la
mezcla SRC_ALPHA, incluyendo las dos caras cruzadas de cada arbol. La prueba en
GPU compara los pixeles transparentes/semitransparentes de los dos caminos.

`Shader.h` conserva las ubicaciones de uniforms por programa. `RenderState.h`
evita repetir glUseProgram, bind de VAO/textura y material, y se invalida al
empezar cada frame. Las matrices normales locales se calculan durante la carga;
el render reutiliza la matriz de modelo entre materiales que comparten espacio.
Los buffers/vectores de visibilidad se reutilizan entre cuadros.

La simulacion subdivide los cuadros largos en pasos de hasta 1/60 s (1/120 para
el seguimiento del xolo y la navegacion), conservando el deltaTime acumulado
hasta 0.25 s. Movimiento, gravedad y amortiguacion usan tiempo; cada esqueleto se
evalua una sola vez por cuadro, despues de todos los subpasos. La camara sigue
suavemente el resultado al final del cuadro. V-Sync inicia en 0 para evitar que
una caida ocasional fuerce saltos de 60 a 30 FPS; V permite activarlo en ejecucion.

Medicion comparativa del mismo recorrido de 800 cuadros, 1280x720, Intel UHD
Graphics y V-Sync desactivado en ambas ejecuciones (se excluyen los 30 primeros
cuadros del resumen). No se reduce detalle ni se cambian los modelos:

| Medida | Antes | Despues |
|---|---:|---:|
| Llamadas de dibujo medias/cuadro | 233.94 | 45.41 |
| Triangulos enviados medios/cuadro | 2,782,327 | 1,123,457 |
| CPU render mediana / p95 (ms) | 1.599 / 3.905 | 0.341 / 1.303 |
| GPU mediana / p95 (ms) | 15.014 / 20.331 | 6.405 / 15.383 |

Resultados crudos: `Proyecto/obj/validation/perf-before-performance.json` y
`perf-after-performance.json`. Las capturas comparativas de plaza, arbol y
muelle conservan la geometria visible; las diferencias del agua corresponden a
su animacion temporal. La reduccion media es 80.6% en llamadas de dibujo y 59.6%
en triangulos enviados, con 57.3% menos tiempo mediano de GPU en esta medicion.

## Validacion

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Proyecto.sln /m /t:Build /p:Configuration=Release /p:Platform=x86 /v:minimal /nologo
& .\bin\Win32-Release\Proyecto_Equipo11.exe --validate
```

`--validate` prueba movimiento a distintos FPS, diagonales, escala UV y bloqueo
de controles. Con el FBX real cargado, simula C y WASD y comprueba que las matrices
de huesos y la posicion permanezcan exactamente iguales mientras la camara se mueve.
Dibuja 840 frames con niña, xolo, bote, agua, arboles, fachadas, luces y ambas camaras; verifica profundidad
y errores de OpenGL. Mide la flexion de codos y rodillas en los clips importados,
prueba entrada/salida de pasto a 30/60/144 FPS y comprueba las tapas elevadas del
escenario real. Comprueba exactamente una iglesia, repeticion de las tres casas,
orientacion interior, cobertura exterior y ausencia de edificios en el acceso al
Pasillo. Recorre calle/banqueta/Pasillo en ambos sentidos a 30/60/144 FPS y prueba
rampas y pisos invertidos. Comprueba las proporciones del kiosko y las ocho bancas,
las cuatro esquinas exteriores y la altura/alineacion de las doce luces.
Guarda vistas del paso, del personaje sobre la entrada y de la laguna. Una prueba
en GPU comprueba que el agua produce imagenes distintas en dos tiempos.
Una prueba de framebuffer
verifica en GPU el descarte alfa <0.1 y la mezcla de un pixel al 50% sobre el cielo.
Tambien verifica el esqueleto del xolo, bajada de cabeza, rotacion de raiz,
anticipacion de cabeza, desfase de torso y flexion de patas durante el giro.
Comprueba 4500 poses/mezclas de brazos, la cobertura completa del muelle,
la reproduccion sin bucle y el seguimiento/accion/regreso a 30/60/144 FPS. Simula la
tecla F y captura las tres fases. `--validate-character` omite el entorno masivo
y dibuja 460 frames con los personajes y el axolote.
Ambos modos prueban embarque, poses de pasajeros, distancia independiente de FPS,
timon, altura exacta, colision de casco, desembarque/reembarque en las cuatro
orillas y regreso al muelle a 30/60/144 FPS. La prueba completa usa el suelo real,
simula E/F/C, verifica la pausa de bote/huesos en camara libre y guarda las
capturas 610/640/670/690/720/750 de embarque, navegacion y desembarque.
El NPC prueba los cuatro estados y dos ciclos completos a 30/60/144 FPS,
incluido el anclaje a la mano y la reduccion monotona del pan. Las capturas
400/405/410/415/417 (modo rapido) y 780/785/790/795/797 (escena completa)
revisan reposo, aparicion, comida, retorno y reinicio sobre la banca.
Las tomas de aparicion/comida colocan a la niña dentro del radio y comprueban el
panel; las otras tomas la dejan fuera y exigen que desaparezca. Tambien se
comprueba que la canasta apoye en la banca, que la mano alcance su abertura y
que codos y muñecas permanezcan dentro de los limites a 30/60/144 FPS.
Se comprueban tambien las UV repetidas del muelle y la oscilacion de la flecha
sin modificar la altura del bote; se repite el recorrido real de acceso al muelle.
Ambos modos caminan hasta el borde del muelle y embarcan a 30/60/144 FPS; el
modo completo usa el piso y el escalon originales del OBJ.
Un segundo argumento opcional define el prefijo de capturas TGA.

`--profile obj/validation/nombre` ejecuta el mismo recorrido y guarda
`nombre-performance.json` con tiempos CPU/GPU, draw calls, triangulos enviados y
objetos/sectores descartados por cuadro. Las consultas GPU son asincronas: solo
se lee el resultado cuando esta disponible, sin detener el render. Los tiempos
de render excluyen la carga de modelos y la lectura/guardado de capturas.

La carga del OBJ de 325 MB puede tardar varios minutos. La navegacion ajusta
la altura al terreno y bloquea las cuatro puertas blancas con colisiones solidas.

## Axolote de la entrada y dialogo del festival

Se copio completa la carpeta del Escritorio a `Proyecto/Models/AxoloteEntrada/`,
conservando los originales `AjoloteEntrada.blend` y su respaldo.
El archivo cargado una sola vez por Assimp es `AxoloteEntrada.fbx`, con dos PNG incrustados; tambien se
generaron `AxoloteEntrada.glb` y `AxoloteSaludo.blend` para edicion/exportacion.
El Assimp incluido en la solucion requiere FBX para este personaje.

`tools/build_axolote.py` crea seis huesos deformantes y el unico clip `AxoGreeting`
de ocho segundos. La maquina pasa por reposo, elevacion, saludo lateral de mano
(+/-16 grados), retorno y reposo. No contiene ademanes de llamar al jugador.
Descarta antes de asignar pesos los vertices sueltos sin caras para evitar el
fallo del importador FBX antiguo. Los ojos, reflejos y sonrisa quedan ligados a la cabeza.

El Blender entregado no tiene material asignado a piel/ojos y su material de
plumas apunta a un archivo ausente. Se conservan las UV originales y los slots
existentes; las texturas de plumas y ropa se recuperan de los assets Xolotl
del mismo escenario y se empaquetan en FBX/GLB. La piel tiene dos materiales
para distinguir cuerpo y branquias. El original no se sobrescribe.

`Model.h` reconoce las referencias incrustadas *0/*1 de Assimp: PNG/JPEG se
decodifican con stb_image desde memoria; aiTexel sin comprimir se convierte de
BGRA a RGBA. `Texture2D.h` aplica mipmaps, repeticion y anisotropia al mismo
camino de subida GPU. Los materiales conservan el factor baseColor glTF cuando
existe y usan las UV del canal 0. Cada imagen se carga/cachea una sola vez.

El NPC mide 4.4 unidades fuente incluyendo el penacho. Su posicion se obtiene
del AABB real de `letrero_bienvenida.001`: borde derecho del letrero azul,
mas medio ancho del modelo y 0.65 unidades de separacion. Se busca suelo a
menos de 1.5 unidades de su plano; no se coloca en la calle de la plaza.

`AxoloteNpc.h` usa un trigger horizontal de radio 5, con tolerancia vertical 3.
Al entrar muestra el aviso G. G avanza exactamente los mensajes 1, 2 y 3;
una pulsacion adicional los cierra. Salir del trigger, navegar o reiniciar
restablece la secuencia; G se desactiva en camara libre y a bordo.
Los mensajes Unicode solicitados estan en `tools/build_axolote_dialogue.ps1`
y se rasterizan con Segoe UI a cuatro PNG RGBA en `images/AxoloteDialogo[0-3].png`.
La interfaz reutiliza una geometria, selecciona la textura de la pagina y hace
un solo draw call; no carga fuentes ni texturas dentro del loop. Tiene prioridad
sobre el panel de Ivanovich para evitar superposiciones.

## Cuatro puertas blancas solidas

`Model::AddDoors` separa las cuatro portadas cardinales del material
`Stuco_Wall`, preservando la pieza central del kiosko. Cada arco recibe una
traslacion de 3 unidades fuente hacia el centro del parque y su base se ajusta
al piso interior. La validacion exige que los ocho pilares queden estrictamente
dentro del AABB del piso del parque y a su misma altura.

Se calculan DOS AABB por arco, una por cada pilar, a partir del tramo vertical
de la malla. El dintel/boveda no participa en las cajas y el centro queda abierto.
`SolidObstacles.h` mantiene el barrido continuo, recuperacion de penetracion y
deslizamiento. Prueba los ocho pilares a 4000 unidades/s por ambos ejes/sentidos
y recorre los cuatro huecos caminando/corriendo en ambos sentidos a 30/60/144 FPS,
con ajuste de altura sobre el piso real; falla si aparece una pared invisible.

## Vista limpia en Visual Studio

Los archivos `.vcxproj` y `.vcxproj.filters` ya no enumeran scripts de
exportacion, archivos Blender, GLB alternativos ni OBJ originales reemplazados
por rigs. Todos permanecen en disco. Se conservan los FBX activos de niña, xolo,
NPC y axolote, el perfil anatomico y los recursos de ejecucion.
Los FBX activos no son residuos: C++ los carga al iniciar. Ambos XML tienen
los mismos elementos, sin referencias inexistentes ni duplicadas.

La validacion rapida ahora ejecuta 460 cuadros y la completa 840. Añaden
dos ciclos de saludo a 30/60/144 FPS, continuidad de mano, limite de muñeca,
G secuencial y capturas de aviso/mensajes/cierre/salida del trigger.

## Compilacion y ejecucion automáticas

Despues de cada cambio de codigo, compilar con MSBuild de Visual Studio 2022
y ejecutar el proyecto automaticamente; comprobar que ambas operaciones terminen
correctamente. `--validate` verifica la escena completa y termina con codigo cero
tras 840 cuadros sin errores OpenGL. `--validate-character` sirve como comprobacion
previa de 460 cuadros para modelos, materiales, animaciones y dialogos.

## Luna e iluminacion nocturna

`MoonSky.h` crea una esfera de 2145 vertices y 3968 triangulos, una sola vez
al cargar. Usa `Shader/moon.vs` y `moon.frag` para crateres procedurales
y emision propia, sin textura externa ni animacion orbital. Posicion de mundo:
(3100, 10000, -2600), radio 280. La Luna escribe profundidad, queda detras
de los objetos y delante del skybox; el plano lejano siempre incluye su esfera.
Una prueba GPU verifica su silueta y su brillo. Las capturas 420 (rapida)
y 800 (completa) miran directamente hacia ella.

La direccion de luz se calcula desde el punto medio entre plaza y lago hacia
la Luna. Los shaders de entorno, personajes y farolas reciben esa misma luz
direccional global: ambiente (0.17, 0.20, 0.28), difusa (0.32, 0.36, 0.47).
Se conserva la iluminacion calida local de las doce farolas. El agua utiliza
la misma direccion/color y agrega un reflejo especular que responde a su
normal animada y a la posicion de la camara. No hay caida por distancia para
la luz lunar: alcanza tanto el Zocalo como la laguna.

## Recuento de animaciones

`ANIMACIONES.md` documenta **18 tipos activos**: 14 clips esqueleticos
(3 niña, 4 xolo, 6 NPC, 1 ajolote) y 4 efectos independientes (pan, agua,
flecha y marcha procedural de los dos Guias Antorcha).
Se cuentan los reposos y cada clip completo una sola vez; la Luna es estatica.
La consola muestra el recuento obtenido de los FBX cargados.
