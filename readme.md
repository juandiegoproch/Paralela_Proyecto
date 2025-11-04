# Ecuación del calor en 2D

Descripción: proyecto de implementación y análisis paralelo de la ecuación del calor en dos dimensiones.

## Requerimientos generales
- Implementar una solución paralela en C++.
- Usar un modelo PRAM apropiado y documentarlo.
- Registrar al menos tres versiones (beta) del desarrollo, con evidencia de validación en caso de usar fuentes externas.
- Medir tiempos y analizar escalabilidad y rendimiento.

## Tareas detalladas

1. Diseño y paralelización
    - Elegir el paradigma paralelo adecuado y justificar la elección.
    - Definir y documentar el modelo PRAM usado.
    - Implementar el código en C++ usando MPI para comunicación entre procesos.

2. Descomposición del dominio y comunicación
    - Diseñar descomposición del dominio considerando que n puede no ser divisible entre p (distribución desigual de elementos).
    - Implementar comunicación no bloqueante (`MPI_Isend`/`MPI_Irecv`) y usar tipos derivados MPI cuando corresponda.
    - Garantizar manejo correcto de los bordes y condiciones de contorno entre subdominios.

3. Versionado y validación
    - Registrar el desarrollo en por lo menos 3 pasos (versiones beta), describiendo cambios y mejoras en cada versión.
    - Si se utiliza código o algoritmos externos, mostrar la validación (casos de prueba, comparaciones analíticas o con referencias).

4. Medición y análisis de rendimiento
    - Medir tiempos de ejecución y comparar con la complejidad teórica esperada.
    - Calcular y graficar el speedup frente al número de procesos (p).
    - Medir FLOPs dentro del código y graficar FLOPs vs p. Documentar la metodología de conteo de FLOPs.

5. Escalabilidad
    - Analizar la escalabilidad del algoritmo variando el tamaño del problema y el número de procesos.
    - Presentar conclusiones sobre límites de escalabilidad y cuellos de botella observados.

## Entregables sugeridos
- Código fuente en C++ (con instrucciones de compilación y ejecución).
- Documento con diseño PRAM, decisiones de paralelización y registro de versiones.
- Scripts o instrucciones para reproducir mediciones y generar gráficas (speedup, FLOPs vs p).
- Informe de validación y análisis de resultados.