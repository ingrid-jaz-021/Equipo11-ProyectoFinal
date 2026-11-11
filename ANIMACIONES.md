# Inventario de animaciones activas

**Total: 18 tipos = 14 clips esqueléticos + 4 efectos animados independientes.**

Criterio: cada clip cargado y utilizado por C++ cuenta como un tipo, incluidos
los reposos. Las fases internas de un mismo clip no se cuentan de nuevo.

| Elemento | Cantidad | Animaciones |
|---|---:|---|
| Niña | 3 | Idle (reposo), Walk (caminar), Run (correr). |
| Xoloitzcuintle | 4 | Idle, Walk, Run, Special (olfatear, girar y avanzar en una sola secuencia). |
| NPC Ivanovich | 6 | Idle, ReachBasket, Appear, RaiseBread, Eat, ReturnHand. |
| Ajolote | 1 | AxoGreeting: levantar el brazo, saludo lateral de mano y regreso al reposo. |
| Pan de muerto | 1 | Reducción progresiva de escala mientras el NPC come. |
| Agua de la laguna | 1 | Desplazamiento UV y ondulación del shader, como un único efecto de agua. |
| Flecha del bote | 1 | Oscilación vertical senoidal. |
| Guías antorcha | 1 | Marcha procedural sin esqueleto: piernas alternas, rebote y recorrido de ida y vuelta por los tapetes. |
| **Total** | **18** | **14 clips + 4 efectos.** |

La Luna permanece fija; su esfera y su iluminación no agregan una animación.
Las cámaras, el seguimiento de la mascota y la navegación del bote son sistemas
de control/movimiento y no clips adicionales. La cola del xolo y el movimiento
de cabeza del NPC ya forman parte de sus clips. Los diálogos cambian de página
sin animación de transición. El skybox, árboles y farolas son estáticos.

El programa suma los clips de los cuatro FBX realmente cargados e imprime el
total en consola. El modo de validación comprueba que existan 14 clips activos.
El antiguo ademán de llamar al jugador del ajolote ya no está en el archivo
exportado ni en su máquina de estados.
