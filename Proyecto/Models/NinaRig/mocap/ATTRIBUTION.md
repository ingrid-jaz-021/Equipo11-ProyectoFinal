# Captura de movimiento de la nina

Los clips activos Idle, Walk y Run se retargetearon desde capturas humanas reales:

- Walk: CMU 07_01, https://raw.githubusercontent.com/una-dinosauria/cmu-mocap/master/data/007/07_01.bvh
- Run: CMU 09_01, https://raw.githubusercontent.com/una-dinosauria/cmu-mocap/master/data/009/09_01.bvh
- Idle: CMU 77_02, https://raw.githubusercontent.com/una-dinosauria/cmu-mocap/master/data/077/77_02.bvh

Fuente original: https://mocap.cs.cmu.edu/
Conversion BVH: Bruce Hahne / cgspeed, distribuida por https://github.com/una-dinosauria/cmu-mocap
Se conserva READMEFIRST.txt con los terminos y detalles de la conversion.
Terminos originales: https://mocap.cs.cmu.edu/faqs.php

The data used in this project was obtained from mocap.cs.cmu.edu.
The database was created with funding from NSF EIA-0196217.

`mocap-retarget.json` registra los intervalos y fases utilizados. Los BVH son
fuente para Blender; el programa carga el FBX ya retargeteado y con pesos.
