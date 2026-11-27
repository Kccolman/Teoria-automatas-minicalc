# Mini-Calc en C++

Trabajo práctico final de Teoría de Autómatas. Mini-Calc lee un archivo
`.calc`, muestra los tokens con su línea y columna, y verifica la sintaxis
de las asignaciones y las instrucciones `print`. El código está escrito en
C++98 y usa la biblioteca estándar.

## Cómo ejecutar en Dev-C++ 5.11

1. En GitHub, elegí **Código → Descargar ZIP** y extraé el contenido.
2. Abrí `minicalc.cpp` en Dev-C++ y elegí **Execute → Compile & Run**.
3. Cuando la consola pida la ruta del archivo, escribí
   `ejemplos/suma_valida.calc` y presioná **Enter**.
4. El programa vuelve a pedir otra ruta al terminar cada análisis, así que
   podés probar todos los ejemplos en una sola ejecución. Para salir,
   presioná **Enter** sin escribir nada. También acepta la ruta completa
   con comillas o espacios de más, tal como la copia Windows con *Copiar
   como ruta de acceso*.

## Pruebas de ejecución

| Archivo de entrada | Resultado esperado |
| --- | --- |
| `ejemplos/suma_valida.calc` | Acepta `print 4 + 4;` y confirma que la sintaxis es válida. |
| `ejemplos/error_operador.calc` | En `print 4 4;`, espera un operador y señala el segundo `4` en la línea 1, columna 9. |
| `ejemplos/error_lexico.calc` | Detecta el carácter `@` en la línea 2, columna 9. |
| `ejemplos/error_punto_y_coma.calc` | Espera un punto y coma al encontrar `print` en la línea 2, columna 1. |
| `ejemplos/error_parentesis.calc` | Espera un paréntesis de cierre en la línea 1, columna 13. |
| `ejemplos/error_identificador.calc` | Rechaza `print1` como palabra no reconocida en la línea 1, columna 1. |
| `ejemplos/error_palabra_invalida.calc` | Rechaza `printx` como palabra no reconocida en la línea 1, columna 1. |
| `ejemplos/vacio.calc` | Archivo vacío: informa que no contiene instrucciones para analizar. |
| `ejemplos/error_mayuscula.calc` | Rechaza la `P` de `Print x;` en la línea 1, columna 1, porque solo se permiten minúsculas. |
| `ejemplos/error_parentesis_sobrante.calc` | En `print 2 + 3);`, espera un punto y coma y encuentra `)` en la línea 1, columna 12. |
| `ejemplos/error_expresion_incompleta.calc` | En `print 2 +;`, espera una variable, un número o `(` y encuentra `;` en la línea 1, columna 10. |
| `ejemplos/error_multiple.calc` | Tiene un error sintáctico en la línea 1 y uno léxico en la línea 2. Como el análisis léxico se hace completo antes del sintáctico, informa el carácter `@` en la línea 2, columna 9. |

También se incluye `ejemplos/ejemplo.calc`, con dos asignaciones y una
instrucción `print` válidas. Al finalizar, el programa informa cuántas
sentencias analizó. Un archivo vacío o con solo espacios se informa como
un archivo sin instrucciones para analizar.

## Cómo funciona

El analizador léxico usa la tabla de transiciones de un autómata finito
determinista para reconocer los tokens. Luego el analizador sintáctico usa
descenso recursivo para comprobar que los tokens siguen la gramática de Mini-Calc.
La salida confirma si la estructura es válida o indica el primer error detectado.
Cuando hay un error, se muestra además la línea del archivo con un `^` debajo de
la columna donde se detectó:

```
Error [Linea 1, Columna 9]: Error sintactico. Se esperaba un operador (+, -, * o /) y se encontro '4'.
  print 4 4;
          ^
```
