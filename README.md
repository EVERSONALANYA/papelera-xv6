# Papelera de Reciclaje para xv6

**Extensión funcional del sistema de archivos de xv6 (RISC-V)**

Trabajo final del curso **IS-380 Sistemas Operativos**, Escuela Profesional de Ingeniería de Sistemas, Universidad Nacional de San Cristóbal de Huamanga (UNSCH), 2026-II.
Docente: Ing. Lizbeth Jaico Quispe

| Integrante | Usuario de GitHub | Aporte principal |
|---|---|---|
| Everson Alanya Tinco | @EVERSONALANYA | Llamada al sistema `trash()` y funciones base del kernel |
| Luis Lloclla Sauñe | @luislloclla27-del | Llamadas `restore()` y `emptytrash()`, registro de las syscalls |
| Wilder Hinostroza Rivera | @WilderHinostroza | Comandos de usuario `rm`, `trash`, `restore`, `emptytrash` |
| Samuel Barzola Navarro | @samue-cpu | Programa de pruebas `trashtest`, evidencias y documentación |

---

## 1. Problema

En xv6, el comando `rm` llama a `unlink()`, que borra la entrada del directorio. Cuando el contador de enlaces del inodo (`nlink`) llega a 0, el kernel libera el inodo y sus bloques de datos. **Un archivo borrado por error se pierde para siempre**, porque no hay forma de recuperarlo.

## 2. Solución

Se agregó al kernel una **papelera de reciclaje** con tres llamadas al sistema nuevas:

| Llamada al sistema | Qué hace | Devuelve |
|---|---|---|
| `int trash(char *ruta)` | Mueve el archivo a `/.trash` y guarda su ruta original | id del elemento (> 0) o un código de error |
| `int restore(int id)` | Devuelve el archivo a su ubicación original | 0 o un código de error |
| `int emptytrash(void)` | Elimina definitivamente todo el contenido de la papelera y libera el espacio | número de archivos eliminados |

**Idea clave:** el archivo **no se copia**. Se crea un enlace duro `/.trash/<id>` al mismo inodo y luego se quita la entrada original. Como `nlink` nunca llega a 0, los bloques de datos se conservan. Solo `emptytrash()` hace que `nlink` llegue a 0 y libera el espacio. La ruta original se guarda en `/.trash/<id>.p`.

```
 ANTES                      rm nota.txt (trash)          restore 1
 /                          /                            /
 ├── nota.txt ──► inodo 25  ├── .trash/                  ├── nota.txt ──► inodo 25
 └── docs/                  │   ├── 1   ──► inodo 25     └── docs/
                            │   └── 1.p  "/nota.txt"
                            └── docs/
```

Todas las operaciones usan transacciones del log de xv6 (`begin_op`/`end_op`), así que una caída del sistema no deja el disco inconsistente. Como la papelera está en el disco, **sobrevive a los reinicios**.

### Códigos de error (`kernel/trash.h`)

| Código | Valor | Significado |
|---|---|---|
| `TRASH_ENOENT` | -1 | El archivo o el elemento no existe |
| `TRASH_EEXIST` | -2 | Ya existe un archivo con ese nombre en la ruta original |
| `TRASH_ENODIR` | -3 | El directorio original ya no existe |
| `TRASH_EINVAL` | -4 | Operación inválida (directorio, dispositivo o un archivo que ya está en la papelera) |
| `TRASH_EFULL` | -5 | La papelera está llena (9999 elementos) |

## 3. Archivos modificados e incorporados

| Archivo | Tipo | Descripción |
|---|---|---|
| `kernel/trash.c` | **Nuevo** | Implementación de `sys_trash`, `sys_restore` y `sys_emptytrash`, más las funciones auxiliares `dirpath` (reconstruye la ruta absoluta subiendo por `..`), `removeent` y `trashdir` |
| `kernel/trash.h` | **Nuevo** | Constantes y códigos de error compartidos entre el kernel y los programas de usuario |
| `kernel/sysfile.c` | Modificado | `create()` deja de ser `static` para poder usarse desde `trash.c` |
| `kernel/defs.h` | Modificado | Prototipo de `create()` |
| `kernel/syscall.h` | Modificado | `SYS_trash` (23), `SYS_restore` (24), `SYS_emptytrash` (25) |
| `kernel/syscall.c` | Modificado | Registro de las 3 syscalls en la tabla de despacho |
| `user/usys.pl`, `user/user.h` | Modificados | Stubs y prototipos de usuario |
| `user/rm.c` | Modificado | `rm` envía a la papelera; `rm -f` elimina definitivamente |
| `user/trash.c` | **Nuevo** | Comando que lista el contenido de la papelera |
| `user/restore.c` | **Nuevo** | Comando que recupera por id o por nombre |
| `user/emptytrash.c` | **Nuevo** | Comando que vacía la papelera (con confirmación) |
| `user/trashlib.h` | **Nuevo** | Funciones auxiliares de usuario para leer la papelera |
| `user/trashtest.c` | **Nuevo** | 9 pruebas automáticas |
| `Makefile` | Modificado | Agrega `trash.o` y los programas nuevos |

## 4. Requisitos

- Linux (Ubuntu o Debian, o WSL en Windows)
- Toolchain RISC-V y QEMU:

```bash
sudo apt update
sudo apt install git build-essential gdb-multiarch qemu-system-misc gcc-riscv64-linux-gnu binutils-riscv64-linux-gnu
```

## 5. Instalación, compilación y ejecución

```bash
git clone https://github.com/<usuario>/<repositorio>.git
cd <repositorio>
make qemu          # compila el kernel, crea fs.img y arranca xv6
```

Para salir de QEMU, pulsa `Ctrl+A` y luego `X`. Para recompilar desde cero, usa `make clean && make qemu`.

> Si tu xv6 es otra versión, puedes aplicar solo la extensión con `git apply papelera-xv6.patch`.

## 6. Uso

```
$ echo hola mundo > nota.txt
$ rm nota.txt
rm: 'nota.txt' enviado a la papelera (id 1). Recuperalo con: restore 1
$ trash
  ID   TAMANO  RUTA ORIGINAL
----  -------  -------------------------
   1       11  /nota.txt
$ restore nota.txt            # o: restore 1
restore: id 1 recuperado en /nota.txt
$ cat nota.txt
hola mundo
$ rm -f nota.txt              # eliminación definitiva (sin papelera)
$ emptytrash                  # vacía la papelera (pide confirmación; -y para omitirla)
```

## 7. Pruebas

Dentro de xv6, ejecuta:

```
$ trashtest
```

| # | Caso de prueba | Resultado esperado | Obtenido |
|---|---|---|---|
| 1 | Enviar un archivo a la papelera | id > 0, el original desaparece y se guarda la ruta | ✅ |
| 2 | Restaurar el archivo | Vuelve con el mismo contenido | ✅ |
| 3 | Ruta relativa en un subdirectorio | Se guarda la ruta absoluta `/t_docs/notas.txt` | ✅ |
| 4 | Conflicto de nombre al restaurar | `TRASH_EEXIST`, sin sobrescribir | ✅ |
| 5 | El directorio original fue eliminado | `TRASH_ENODIR`; funciona al recrearlo | ✅ |
| 6 | Operaciones inválidas (no existe, directorio, dispositivo, archivo ya en la papelera, id inválido) | Todas rechazadas | ✅ |
| 7 | El mismo nombre eliminado dos veces | Dos ids distintos, ambas versiones recuperables | ✅ |
| 8 | Archivo de 20 KB (bloques indirectos) | Contenido idéntico y **mismo número de inodo** | ✅ |
| 9 | Vaciar la papelera | Papelera vacía y los ids ya no se pueden restaurar | ✅ |

**Resultado: 9 de 9 pruebas aprobadas.** Además, los `usertests -q` originales de xv6 siguen pasando (`ALL TESTS PASSED`), lo que confirma que la extensión no rompe el sistema de archivos.

Las evidencias están en la carpeta [`pruebas/`](pruebas/):
- `salida_trashtest.txt`: salida completa de las 9 pruebas
- `salida_demo_comandos.txt`: uso de los comandos y persistencia tras reiniciar
- `salida_usertests.txt`: pruebas de regresión de xv6
- `capturas/`: capturas de compilación y ejecución

## 8. Limitaciones

- Solo se envían a la papelera **archivos comunes**. Los directorios se siguen eliminando con `rm` como antes (si están vacíos).
- Los archivos en la papelera **siguen ocupando espacio** hasta ejecutar `emptytrash`.
- xv6 no tiene reloj de fecha y hora, así que no se registra cuándo se eliminó cada archivo.
- La ruta original se guarda en una transacción separada del movimiento. Si el sistema cae justo entre ambas, el archivo queda en la papelera con ruta "(desconocida)", pero no se pierde.
- Máximo 9999 elementos en la papelera.

## 9. Referencias

- Cox, R., Kaashoek, F., & Morris, R. *xv6: a simple, Unix-like teaching operating system* (libro de xv6 para RISC-V). MIT.
- Silberschatz, A., Galvin, P. B., & Gagne, G. *Operating System Concepts*. Wiley.
- Código base: https://github.com/mit-pdos/xv6-riscv
