# INVENTARIO OBSOLETO: no ejecutar esta lista

El proyecto incorpora ahora arboles RGBA, un rig FBX, archivos Blender, clips y nuevas dependencias. El inventario historico de abajo no representa las dependencias actuales; Arbol.png es ahora una entrada de autor y debe conservarse. La limpieza anterior no se ejecuto.

# Limpieza pendiente por bloqueo de herramientas

La eliminacion no se ejecuto: la herramienta rechazo el borrado con "blocked by policy".
Se intentaron rutas verificadas dentro de la solucion y rutas individuales; no se elimino ningun archivo de esta lista.

El inventario sigue las referencias desde Proyecto.cpp, sus includes locales, los tres OBJ activos, sus MTL, todas sus texturas y los cinco shaders activos. Las bibliotecas externas de GLEW/GLFW/Assimp/GLM se conservan como dependencias transitivas.

- Dependencias propias conservadas: 76.
- Archivos sin referencias: 153.
- Tamaño candidato: 433.67 MiB.

## Archivos candidatos

| Archivo | Bytes |
| --- | ---: |
| Proyecto/images/Arbol.png | 18228 |
| Proyecto/Main.cpp | 5331 |
| Proyecto/Modelado_Jerarquico.cpp | 15967 |
| Proyecto/Models/EntornoCompleto/Laguna/Agua/River_Water_Texture-427.jpg | 7926860 |
| Proyecto/Models/EntornoCompleto/Laguna/Almas/alma.mtl | 235 |
| Proyecto/Models/EntornoCompleto/Laguna/Almas/alma.obj | 89237 |
| Proyecto/Models/EntornoCompleto/Laguna/Almas/almaCap.mtl | 243 |
| Proyecto/Models/EntornoCompleto/Laguna/Almas/almaCap.obj | 93423 |
| Proyecto/Models/EntornoCompleto/Laguna/Almas/Almas.blend | 1446224 |
| Proyecto/Models/EntornoCompleto/Laguna/Almas/Almas.blend1 | 1446224 |
| Proyecto/Models/EntornoCompleto/Laguna/Almas/AlmasTela.png | 540235 |
| Proyecto/Models/EntornoCompleto/Laguna/Almas/AlmasTelaGris.png | 1208064 |
| Proyecto/Models/EntornoCompleto/Laguna/Axolote/Axolote.blend | 167850 |
| Proyecto/Models/EntornoCompleto/Laguna/Axolote/Axolote.blend1 | 167717 |
| Proyecto/Models/EntornoCompleto/Laguna/Axolote/axolotl.mtl | 242 |
| Proyecto/Models/EntornoCompleto/Laguna/Axolote/axolotl.obj | 153631 |
| Proyecto/Models/EntornoCompleto/Laguna/Axolote/Untitled.blend | 140446 |
| Proyecto/Models/EntornoCompleto/Laguna/Axolote/Untitled.blend1 | 129201 |
| Proyecto/Models/EntornoCompleto/Laguna/BaseEscultura/BaseEscultura.mtl | 250 |
| Proyecto/Models/EntornoCompleto/Laguna/BaseEscultura/BaseEscultura.obj | 4579 |
| Proyecto/Models/EntornoCompleto/Laguna/CalacaEsc/CalacaEsc.blend | 123265 |
| Proyecto/Models/EntornoCompleto/Laguna/CalacaEsc/CalacaEsc.blend1 | 123539 |
| Proyecto/Models/EntornoCompleto/Laguna/CalacaEsc/calacaEsc.mtl | 245 |
| Proyecto/Models/EntornoCompleto/Laguna/CalacaEsc/calacaEsc.obj | 43120 |
| Proyecto/Models/EntornoCompleto/Laguna/Danzantes/DanzanteHombre.mtl | 614 |
| Proyecto/Models/EntornoCompleto/Laguna/Danzantes/DanzanteHombre.obj | 1805392 |
| Proyecto/Models/EntornoCompleto/Laguna/Danzantes/DanzanteHombre2.mtl | 615 |
| Proyecto/Models/EntornoCompleto/Laguna/Danzantes/DanzanteHombre2.obj | 599166 |
| Proyecto/Models/EntornoCompleto/Laguna/Danzantes/danzanteMujer.mtl | 621 |
| Proyecto/Models/EntornoCompleto/Laguna/Danzantes/danzanteMujer.obj | 154102 |
| Proyecto/Models/EntornoCompleto/Laguna/Danzantes/Danzantes.blend | 4065169 |
| Proyecto/Models/EntornoCompleto/Laguna/Danzantes/Danzantes.blend1 | 4083416 |
| Proyecto/Models/EntornoCompleto/Laguna/Danzantes/Feather26.OBJ | 1102585 |
| Proyecto/Models/EntornoCompleto/Laguna/Danzantes/FemaleColor.png | 1511 |
| Proyecto/Models/EntornoCompleto/Laguna/Danzantes/indianHeaddress.mtl | 254 |
| Proyecto/Models/EntornoCompleto/Laguna/Danzantes/indianHeaddress.obj | 66564 |
| Proyecto/Models/EntornoCompleto/Laguna/Danzantes/MaleColor.png | 3836 |
| Proyecto/Models/EntornoCompleto/Laguna/Danzantes/mask.mtl | 439 |
| Proyecto/Models/EntornoCompleto/Laguna/Danzantes/mask.obj | 1598398 |
| Proyecto/Models/EntornoCompleto/Laguna/Danzantes/PlumasDanz.png | 34223 |
| Proyecto/Models/EntornoCompleto/Laguna/Danzantes/PlumasDanz2.png | 52827 |
| Proyecto/Models/EntornoCompleto/Laguna/Danzantes/RopaHombreColor.png | 45892 |
| Proyecto/Models/EntornoCompleto/Laguna/Danzantes/telaVestido.png | 800054 |
| Proyecto/Models/EntornoCompleto/Laguna/HumanoEsc/HumanoEsc.blend | 124982 |
| Proyecto/Models/EntornoCompleto/Laguna/HumanoEsc/HumanoEsc.blend1 | 122385 |
| Proyecto/Models/EntornoCompleto/Laguna/HumanoEsc/humanoEsc.mtl | 248 |
| Proyecto/Models/EntornoCompleto/Laguna/HumanoEsc/humanoEsc.obj | 51050 |
| Proyecto/Models/EntornoCompleto/Laguna/LuzEsc/LuzEsc.blend | 107706 |
| Proyecto/Models/EntornoCompleto/Laguna/LuzEsc/LuzEsc.blend1 | 107672 |
| Proyecto/Models/EntornoCompleto/Laguna/LuzEsc/LuzEsc.mtl | 246 |
| Proyecto/Models/EntornoCompleto/Laguna/LuzEsc/LuzEsc.obj | 9174 |
| Proyecto/Models/EntornoCompleto/Laguna/LuzEsc/metallic-background-with-scratched-metal-design.jpg | 10343386 |
| Proyecto/Models/EntornoCompleto/Laguna/Piramide2/Piramide2.blend | 143866 |
| Proyecto/Models/EntornoCompleto/Laguna/Piramide2/Piramide2.blend1 | 441539 |
| Proyecto/Models/EntornoCompleto/Laguna/Piramide2/Piramide2.mtl | 261 |
| Proyecto/Models/EntornoCompleto/Laguna/Piramide2/Piramide2.obj | 45295 |
| Proyecto/Models/EntornoCompleto/Laguna/Piramide2/rock_main.jpg | 290697 |
| Proyecto/Models/EntornoCompleto/Laguna/Piramide2/stone_tex_1.jpg | 563598 |
| Proyecto/Models/EntornoCompleto/Laguna/SerpienteEsc/SerpienteEsc.blend | 114209 |
| Proyecto/Models/EntornoCompleto/Laguna/SerpienteEsc/SerpienteEsc.blend1 | 113396 |
| Proyecto/Models/EntornoCompleto/Laguna/SerpienteEsc/SerpienteEsc.mtl | 254 |
| Proyecto/Models/EntornoCompleto/Laguna/SerpienteEsc/SerpienteEsc.obj | 30648 |
| Proyecto/Models/EntornoCompleto/Laguna/Tarima/Tarima.blend | 138224 |
| Proyecto/Models/EntornoCompleto/Laguna/Tarima/Tarima.blend1 | 138242 |
| Proyecto/Models/EntornoCompleto/Laguna/Tarima/Tarima.mtl | 256 |
| Proyecto/Models/EntornoCompleto/Laguna/Tarima/Tarima.obj | 9239 |
| Proyecto/Models/EntornoCompleto/Laguna/Xolotl/AccesoriosXolColor.png | 95554 |
| Proyecto/Models/EntornoCompleto/Laguna/Xolotl/Drago.fbx | 420924 |
| Proyecto/Models/EntornoCompleto/Laguna/Xolotl/MaleColorXolotl.png | 6926 |
| Proyecto/Models/EntornoCompleto/Laguna/Xolotl/PlumasColorXolotl.png | 54547 |
| Proyecto/Models/EntornoCompleto/Laguna/Xolotl/RopaXolotlColor.png | 73472 |
| Proyecto/Models/EntornoCompleto/Laguna/Xolotl/Xolotl.blend | 260882 |
| Proyecto/Models/EntornoCompleto/Laguna/Xolotl/Xolotl.blend1 | 256281 |
| Proyecto/Models/EntornoCompleto/Laguna/Xolotl/Xolotl.mtl | 983 |
| Proyecto/Models/EntornoCompleto/Laguna/Xolotl/Xolotl.obj | 390453 |
| Proyecto/Models/EntornoCompleto/Laguna/Xolotl/XolotlMaskColor.png | 21090 |
| Proyecto/Models/EntornoCompleto/LagunaCompleto.mtl | 1671 |
| Proyecto/Models/EntornoCompleto/LagunaCompleto.obj | 1086365 |
| Proyecto/Models/EntornoCompleto/Pasillo/CuboBaseColor(1)(1).png | 1502 |
| Proyecto/Models/EntornoCompleto/Pasillo/CuboBaseColor(1)(2).png | 1502 |
| Proyecto/Models/EntornoCompleto/Pasillo/CuboBaseColor(1).png | 1502 |
| Proyecto/Models/EntornoCompleto/Pasillo/FuegoBaseColor(1)(1).png | 1502 |
| Proyecto/Models/EntornoCompleto/Pasillo/FuegoBaseColor(1).png | 1502 |
| Proyecto/Models/EntornoCompleto/Pasillo/RampaBaseColor(1).png | 1503 |
| Proyecto/Models/EntornoCompleto/PasilloCompleto.mtl | 4496 |
| Proyecto/Models/EntornoCompleto/PasilloCompleto.obj | 57281090 |
| Proyecto/Models/EntornoCompleto/Zocalo/Agua.jpg | 245097 |
| Proyecto/Models/EntornoCompleto/Zocalo/Banqueta(1).jpg | 5371 |
| Proyecto/Models/EntornoCompleto/Zocalo/Banqueta.jpg | 5371 |
| Proyecto/Models/EntornoCompleto/Zocalo/Calle.jpg | 373406 |
| Proyecto/Models/EntornoCompleto/Zocalo/Gris(1).jpg | 302767 |
| Proyecto/Models/EntornoCompleto/Zocalo/Gris.jpg | 302767 |
| Proyecto/Models/EntornoCompleto/Zocalo/Pasto.jpg | 461617 |
| Proyecto/Models/EntornoCompleto/Zocalo/Piedra.jpg | 271042 |
| Proyecto/Models/EntornoCompleto/Zocalo/Piso.jpg | 90591 |
| Proyecto/Models/EntornoCompleto/Zocalo/Piso_banqueta.jpg | 80068 |
| Proyecto/Models/EntornoCompleto/Zocalo/Zocalo(Completo).mtl | 4291 |
| Proyecto/Models/EntornoCompleto/Zocalo(Completo).mtl | 4248 |
| Proyecto/Models/EntornoCompleto/Zocalo(Completo).obj | 343195244 |
| Proyecto/Models/Farola/Textura_focoAzul.jpg | 32256 |
| Proyecto/Models/Niña Zocalo/Niña.blend | 657080 |
| Proyecto/Models/Niña Zocalo/Niña.mtl | 2378 |
| Proyecto/Models/Niña Zocalo/Niña.obj | 1407528 |
| Proyecto/Shader/core.frag | 108 |
| Proyecto/Shader/core.vs | 303 |
| Proyecto/Shader/images/checker_Tex.png | 1079056 |
| Proyecto/Shader/images/window.png | 728977 |
| Proyecto/Shader/lamp.frag | 95 |
| Proyecto/Shader/lamp.vs | 227 |
| Proyecto/Shader/lighting.frag | 5107 |
| Proyecto/Shader/lighting.vs | 494 |
| Proyecto/Shader/Shader/lamp.frag | 193 |
| Proyecto/Shader/Shader/lamp.vs | 386 |
| Proyecto/Shader/Texturizado.cpp | 8281 |
| Proyecto/SkyBox/back.jpg | 28439 |
| Proyecto/SkyBox/bottom.jpg | 5075 |
| Proyecto/SkyBox/front.jpg | 29387 |
| Proyecto/SkyBox/left.jpg | 29533 |
| Proyecto/SkyBox/right.jpg | 26363 |
| Proyecto/SkyBox/space_bk.png | 305924 |
| Proyecto/SkyBox/space_dn.png | 651162 |
| Proyecto/SkyBox/space_ft.png | 557776 |
| Proyecto/SkyBox/space_lf.png | 520314 |
| Proyecto/SkyBox/space_rt.png | 333862 |
| Proyecto/SkyBox/space_up.png | 519300 |
| Proyecto/SkyBox/top.jpg | 47306 |
| Proyecto/SOIL2/etc1_utils.c | 22399 |
| Proyecto/SOIL2/etc1_utils.h | 3632 |
| Proyecto/SOIL2/Iluminacion.cpp | 10507 |
| Proyecto/SOIL2/image_DXT.c | 17181 |
| Proyecto/SOIL2/image_DXT.h | 3212 |
| Proyecto/SOIL2/image_helper.c | 10526 |
| Proyecto/SOIL2/image_helper.h | 2287 |
| Proyecto/SOIL2/jo_jpeg.h | 17476 |
| Proyecto/SOIL2/pkm_helper.h | 385 |
| Proyecto/SOIL2/pvr_helper.h | 5967 |
| Proyecto/SOIL2/Shader/lamp.frag | 95 |
| Proyecto/SOIL2/Shader/lamp.vs | 227 |
| Proyecto/SOIL2/Shader/lighting.frag | 1050 |
| Proyecto/SOIL2/Shader/lighting.vs | 494 |
| Proyecto/SOIL2/SOIL2.c | 88550 |
| Proyecto/SOIL2/SOIL2.h | 18145 |
| Proyecto/SOIL2/stbi_DDS.h | 1644 |
| Proyecto/SOIL2/stbi_DDS_c.h | 16847 |
| Proyecto/SOIL2/stbi_ext.h | 609 |
| Proyecto/SOIL2/stbi_ext_c.h | 1691 |
| Proyecto/SOIL2/stbi_pkm.h | 1534 |
| Proyecto/SOIL2/stbi_pkm_c.h | 5073 |
| Proyecto/SOIL2/stbi_pvr.h | 1610 |
| Proyecto/SOIL2/stbi_pvr_c.h | 23387 |
| Proyecto/SOIL2/stb_image.h | 251308 |
| Proyecto/SOIL2/stb_image_write.h | 40569 |
| External Libraries/SOIL2/lib/soil2-debug.lib | 755196 |

## Dependencias activas

| Archivo | Motivo |
| --- | --- |
| Proyecto/Proyecto.cpp | incluido desde el ejecutable |
| Proyecto/ZocaloLighting.h | incluido desde el ejecutable |
| Proyecto/Model.h | incluido desde el ejecutable |
| Proyecto/GroundSurface.h | incluido desde el ejecutable |
| Proyecto/UnicodeIO.h | incluido desde el ejecutable |
| Proyecto/stb_image.h | incluido desde el ejecutable |
| Proyecto/Mesh.h | incluido desde el ejecutable |
| Proyecto/ProceduralRig.h | incluido desde el ejecutable |
| Proyecto/Shader.h | incluido desde el ejecutable |
| Proyecto/CharacterController.h | incluido desde el ejecutable |
| Proyecto/Texture.h | incluido desde el ejecutable |
| Proyecto/Camera.h | incluido desde el ejecutable |
| Proyecto/Models/EntornoCompleto/EntornoCompleto.obj | modelo instanciado |
| Proyecto/Models/EntornoCompleto/EntornoCompleto.mtl | MTL del modelo activo |
| Proyecto/Models/EntornoCompleto/Pasillo/tex_cempasuchil.png | textura de EntornoCompleto.mtl |
| Proyecto/Models/EntornoCompleto/Pasillo/tex_frutas.png | textura de EntornoCompleto.mtl |
| Proyecto/Models/EntornoCompleto/Pasillo/ladrilloOfrenda.png | textura de EntornoCompleto.mtl |
| Proyecto/Models/EntornoCompleto/Pasillo/letrero_Ofrenda.png | textura de EntornoCompleto.mtl |
| Proyecto/Models/EntornoCompleto/Zocalo/Textura_MetalB.jpg | textura de EntornoCompleto.mtl |
| Proyecto/Models/EntornoCompleto/Pasillo/facade.png | textura de EntornoCompleto.mtl |
| Proyecto/Models/EntornoCompleto/Pasillo/roof.png | textura de EntornoCompleto.mtl |
| Proyecto/Models/EntornoCompleto/Zocalo/Piedra.png | textura de EntornoCompleto.mtl |
| Proyecto/Models/EntornoCompleto/Zocalo/Agua.png | textura de EntornoCompleto.mtl |
| Proyecto/Models/EntornoCompleto/Zocalo/Piso.png | textura de EntornoCompleto.mtl |
| Proyecto/Models/EntornoCompleto/Zocalo/Pasto.png | textura de EntornoCompleto.mtl |
| Proyecto/Models/EntornoCompleto/Zocalo/Calle.png | textura de EntornoCompleto.mtl |
| Proyecto/Models/EntornoCompleto/Zocalo/Textura_Kiosko.jpg | textura de EntornoCompleto.mtl |
| Proyecto/Models/EntornoCompleto/Zocalo/Banqueta.png | textura de EntornoCompleto.mtl |
| Proyecto/Models/EntornoCompleto/Zocalo/Piso_banqueta.png | textura de EntornoCompleto.mtl |
| Proyecto/Models/EntornoCompleto/Laguna/BaseEscultura/BaseEsculturaColor.png | textura de EntornoCompleto.mtl |
| Proyecto/Models/EntornoCompleto/Laguna/Axolote/cementoTex.png | textura de EntornoCompleto.mtl |
| Proyecto/Models/EntornoCompleto/Laguna/Piramide2/rock-stone-texture-seamless.jpg | textura de EntornoCompleto.mtl |
| Proyecto/Models/EntornoCompleto/Laguna/LuzEsc/luzEscBaseColor.png | textura de EntornoCompleto.mtl |
| Proyecto/Models/EntornoCompleto/Laguna/Tarima/natural-wooden-background.jpg | textura de EntornoCompleto.mtl |
| Proyecto/Models/EntornoCompleto/Laguna/HumanoEsc/HumanoEscColor.png | textura de EntornoCompleto.mtl |
| Proyecto/Models/EntornoCompleto/Laguna/CalacaEsc/CalacaColor.png | textura de EntornoCompleto.mtl |
| Proyecto/Models/EntornoCompleto/Laguna/SerpienteEsc/SerpienteEscColor.png | textura de EntornoCompleto.mtl |
| Proyecto/Models/EntornoCompleto/Zocalo/Gris.png | textura de EntornoCompleto.mtl |
| Proyecto/Models/EntornoCompleto/Pasillo/Piso_Aserrin_1.png | textura de EntornoCompleto.mtl |
| Proyecto/Models/EntornoCompleto/Pasillo/Piso_Aserrin_3.png | textura de EntornoCompleto.mtl |
| Proyecto/Models/EntornoCompleto/Pasillo/Piso_Aserrin_2.png | textura de EntornoCompleto.mtl |
| Proyecto/Models/EntornoCompleto/Pasillo/tex_azucar.png | textura de EntornoCompleto.mtl |
| Proyecto/Models/EntornoCompleto/Pasillo/tex_chocolate.001.png | textura de EntornoCompleto.mtl |
| Proyecto/Models/EntornoCompleto/Pasillo/tex_florCempa.png | textura de EntornoCompleto.mtl |
| Proyecto/Models/EntornoCompleto/Pasillo/FuegoBaseColor.png | textura de EntornoCompleto.mtl |
| Proyecto/Models/EntornoCompleto/Pasillo/CuboBaseColor.png | textura de EntornoCompleto.mtl |
| Proyecto/Models/EntornoCompleto/Pasillo/Letrero_bienvenida.png | textura de EntornoCompleto.mtl |
| Proyecto/Models/EntornoCompleto/Pasillo/piso_ofrendaCempa.png | textura de EntornoCompleto.mtl |
| Proyecto/Models/EntornoCompleto/Pasillo/RampaBaseColor.png | textura de EntornoCompleto.mtl |
| Proyecto/Models/EntornoCompleto/Pasillo/TexturaTarima2.jpg | textura de EntornoCompleto.mtl |
| Proyecto/Models/EntornoCompleto/Pasillo/VELABaseColor.png | textura de EntornoCompleto.mtl |
| Proyecto/Models/niña zocalo.obj | modelo instanciado |
| Proyecto/Models/niña zocalo.mtl | MTL del modelo activo |
| Proyecto/Models/Niña Zocalo/Ojos_Boca_Niña.png | textura de niña zocalo.mtl |
| Proyecto/Models/Niña Zocalo/Textura_pantalon_niña.png | textura de niña zocalo.mtl |
| Proyecto/Models/Niña Zocalo/Textura_PiesNiña.png | textura de niña zocalo.mtl |
| Proyecto/Models/Niña Zocalo/Textura_Pelo_Niña.png | textura de niña zocalo.mtl |
| Proyecto/Models/Niña Zocalo/Textura_Cuerpo_Niña.png | textura de niña zocalo.mtl |
| Proyecto/Models/Niña Zocalo/Textura_Cabeza_Niña.png | textura de niña zocalo.mtl |
| Proyecto/Models/Niña Zocalo/Textura_Brazos_niña.png | textura de niña zocalo.mtl |
| Proyecto/Models/Niña Zocalo/Calcetas_Niña.png | textura de niña zocalo.mtl |
| Proyecto/Models/Farola/Farola.obj | modelo instanciado |
| Proyecto/Models/Farola/Farola.mtl | MTL del modelo activo |
| Proyecto/Models/Farola/Textura_MetalB.jpg | textura de Farola.mtl |
| Proyecto/Models/Farola/Textura_focoAzul.png | textura de Farola.mtl |
| Proyecto/Shader/character.vs | shader del render loop |
| Proyecto/Shader/modelLoading.vs | shader del render loop |
| Proyecto/Shader/modelLoading.frag | shader del render loop |
| Proyecto/Shader/SkyBox.vs | shader del render loop |
| Proyecto/Shader/SkyBox.frag | shader del render loop |
| Proyecto/SkyBox/galaxy+X.tga | cara del cubemap activo |
| Proyecto/SkyBox/galaxy-X.tga | cara del cubemap activo |
| Proyecto/SkyBox/galaxy+Y.tga | cara del cubemap activo |
| Proyecto/SkyBox/galaxy-Y.tga | cara del cubemap activo |
| Proyecto/SkyBox/galaxy+Z.tga | cara del cubemap activo |
| Proyecto/SkyBox/galaxy-Z.tga | cara del cubemap activo |
