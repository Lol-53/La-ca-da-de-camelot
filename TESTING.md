# Pruebas automatizadas de TFG

La suite vive en `Source/TFGTests` y solo se carga en el editor. No forma parte
del ejecutable Shipping.

## Ejecutar toda la suite

Desde PowerShell:

```powershell
& ".\RunTFGTests.ps1"
```

El informe se genera en:

```text
Saved/Automation/TFG/index.html
```

## Ejecutar desde Unreal Editor

1. Reinicia el editor después de añadir o modificar código de pruebas.
2. Abre `Tools > Test Automation` o `Window > Test Automation`, según la
   disposición del editor.
3. Busca el prefijo `TFG`.
4. Ejecuta todo el árbol o una rama concreta.

## Organización

- `TFG.Cpp.Unit`: lógica pura de inventarios, recursos y poderes.
- `TFG.Cpp.Classes`: construcción y metadatos de todas las clases reflejadas
  del módulo C++ `TFG`.
- `TFG.Cpp.SourceFiles`: integridad básica de cada `.cpp` del proyecto.
- `TFG.Blueprints.LoadAndGenerate`: carga, clase padre, clases generadas y CDO
  de cada Blueprint bajo `/Game/MyContent`.
- `TFG.Blueprints.Compile`: recompila individualmente cada Blueprint de
  `/Game/MyContent` y detecta errores.

Los tests de Blueprint descubren assets automáticamente. Un Blueprint nuevo
dentro de `MyContent` queda cubierto sin tener que añadir su nombre al código.
