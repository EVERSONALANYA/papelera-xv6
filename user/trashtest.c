// trashtest - pruebas automáticas de la papelera de reciclaje.
// Uso: trashtest
// Ejecuta cada caso de prueba e imprime el resultado esperado y el obtenido.

#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/trash.h"
#include "user/user.h"
#include "user/trashlib.h"

static int aprobadas = 0, total = 0;

// ---------- utilidades ----------
static int
crear(char *ruta, char *texto)
{
  int fd = open(ruta, O_CREATE | O_TRUNC | O_WRONLY);
  if (fd < 0)
    return -1;
  write(fd, texto, strlen(texto));
  close(fd);
  return 0;
}

static int
leer(char *ruta, char *buf, int max)
{
  int fd = open(ruta, O_RDONLY), n;
  if (fd < 0)
    return -1;
  n = read(fd, buf, max - 1);
  close(fd);
  buf[n < 0 ? 0 : n] = 0;
  return n;
}

static int
existe(char *ruta)
{
  struct stat st;
  return stat(ruta, &st) == 0;
}

static int
contenido(char *ruta, char *esperado)
{
  char buf[256];
  return leer(ruta, buf, sizeof(buf)) >= 0 && strcmp(buf, esperado) == 0;
}

static int
metaes(int id, char *esperado)
{
  char buf[MAXPATH];
  return leerorigen(id, buf, sizeof(buf)) == 0 && strcmp(buf, esperado) == 0;
}

static void
inicio(char *titulo, char *entrada, char *esperado)
{
  total++;
  printf("\n[PRUEBA %d] %s\n", total, titulo);
  printf("  Entrada  : %s\n", entrada);
  printf("  Esperado : %s\n", esperado);
}

static void
resultado(int ok, char *obtenido)
{
  printf("  Obtenido : %s\n", obtenido);
  printf("  Resultado: %s\n", ok ? "APROBADA" : "FALLIDA <<<<<");
  if (ok)
    aprobadas++;
}

static int
papeleravacia(void)
{
  int fd = open(TRASH_DIR, O_RDONLY);
  struct dirent de;
  int n = 0;
  if (fd < 0)
    return 1;
  while (read(fd, &de, sizeof(de)) == sizeof(de))
    if (de.inum != 0)
      n++;
  close(fd);
  return n == 2;     // solo "." y ".."
}

// ---------- casos de prueba ----------
void
prueba_enviar(void)
{
  int id;
  char ruta[32];

  inicio("Enviar un archivo a la papelera",
         "crear /t_a.txt con \"hola papelera\"; trash(\"/t_a.txt\")",
         "id > 0, /t_a.txt ya no existe, /.trash/<id> existe y guarda la ruta /t_a.txt");
  crear("/t_a.txt", "hola papelera");
  id = trash("/t_a.txt");
  rutaitem(id > 0 ? id : 1, 0, ruta);
  int ok = id > 0 && !existe("/t_a.txt") && existe(ruta) && metaes(id, "/t_a.txt");
  printf("  (id devuelto = %d)\n", id);
  resultado(ok, ok ? "id valido, archivo movido y ruta original guardada" : "el archivo no se movio correctamente");

  inicio("Restaurar el archivo con el mismo contenido",
         "restore(id de la prueba 1)",
         "restore devuelve 0 y /t_a.txt vuelve con \"hola papelera\"");
  int r = restore(id);
  ok = r == 0 && contenido("/t_a.txt", "hola papelera") && !existe(ruta);
  printf("  (restore devolvio %d)\n", r);
  resultado(ok, ok ? "archivo recuperado con su contenido original" : "no se recupero correctamente");
}

void
prueba_subdirectorio(void)
{
  int id, r, ok;

  inicio("Ruta relativa dentro de un subdirectorio",
         "mkdir /t_docs; cd /t_docs; crear notas.txt; trash(\"notas.txt\"); cd /",
         "la papelera guarda la ruta absoluta /t_docs/notas.txt y restore la devuelve ahi");
  mkdir("/t_docs");
  chdir("/t_docs");
  crear("notas.txt", "apuntes de SO");
  id = trash("notas.txt");
  chdir("/");
  int meta = metaes(id, "/t_docs/notas.txt");
  r = restore(id);
  ok = id > 0 && meta && r == 0 && contenido("/t_docs/notas.txt", "apuntes de SO");
  printf("  (id = %d, ruta guardada correcta = %s, restore = %d)\n", id, meta ? "si" : "no", r);
  resultado(ok, ok ? "ruta absoluta reconstruida y archivo restaurado en /t_docs" : "ruta o restauracion incorrecta");
}

void
prueba_conflicto(void)
{
  int id, r1, r2, ok;
  char ruta[32];

  inicio("Conflicto: ya existe un archivo con el mismo nombre",
         "trash(\"/t_a.txt\"); crear otro /t_a.txt \"nuevo\"; restore(id)",
         "restore devuelve TRASH_EEXIST (-2) y NO sobrescribe; tras borrar el nuevo, restore funciona");
  id = trash("/t_a.txt");
  crear("/t_a.txt", "nuevo");
  r1 = restore(id);
  rutaitem(id, 0, ruta);
  int protegido = contenido("/t_a.txt", "nuevo") && existe(ruta);
  unlink("/t_a.txt");
  r2 = restore(id);
  ok = r1 == TRASH_EEXIST && protegido && r2 == 0 && contenido("/t_a.txt", "hola papelera");
  printf("  (1er restore = %d, archivo nuevo intacto = %s, 2do restore = %d)\n", r1, protegido ? "si" : "no", r2);
  resultado(ok, ok ? "se detecto el conflicto sin perder datos" : "el conflicto no se manejo bien");
}

void
prueba_dir_eliminado(void)
{
  int id, r, ok;

  inicio("El directorio original ya no existe",
         "mkdir /t_tmp; crear /t_tmp/x.txt; trash; unlink(\"/t_tmp\"); restore(id)",
         "restore devuelve TRASH_ENODIR (-3); al recrear /t_tmp, restore funciona");
  mkdir("/t_tmp");
  crear("/t_tmp/x.txt", "dato");
  id = trash("/t_tmp/x.txt");
  unlink("/t_tmp");
  r = restore(id);
  mkdir("/t_tmp");
  int r2 = restore(id);
  ok = id > 0 && r == TRASH_ENODIR && r2 == 0 && contenido("/t_tmp/x.txt", "dato");
  printf("  (restore sin directorio = %d, despues de mkdir = %d)\n", r, r2);
  resultado(ok, ok ? "error detectado y recuperacion posterior correcta" : "comportamiento incorrecto");
}

void
prueba_invalidas(void)
{
  int a, b, c, d, e, f, ok, id;
  char ruta[32];

  inicio("Operaciones invalidas",
         "trash de archivo inexistente, de un directorio, del dispositivo /console y de un elemento de /.trash; restore(9999) y restore(0)",
         "-1, -4, -4, -4, -1, -1 (ninguna modifica el sistema de archivos)");
  a = trash("/no_existe.txt");
  b = trash("/t_docs");
  c = trash("/console");
  crear("/t_c.txt", "c");
  id = trash("/t_c.txt");
  rutaitem(id, 0, ruta);
  d = trash(ruta);
  e = restore(9999);
  f = restore(0);
  ok = a == TRASH_ENOENT && b == TRASH_EINVAL && c == TRASH_EINVAL && d == TRASH_EINVAL &&
       e == TRASH_ENOENT && f == TRASH_ENOENT && existe("/t_docs") && existe("/console");
  printf("  (obtenidos: %d, %d, %d, %d, %d, %d)\n", a, b, c, d, e, f);
  resultado(ok, ok ? "todas las operaciones invalidas fueron rechazadas" : "alguna operacion invalida fue aceptada");
}

void
prueba_mismo_nombre(void)
{
  int id1, id2, ok;

  inicio("Eliminar dos veces un archivo con el mismo nombre",
         "crear /t_b.txt \"version 1\"; trash; crear /t_b.txt \"version 2\"; trash",
         "ids distintos; se puede recuperar cualquiera de las dos versiones");
  crear("/t_b.txt", "version 1");
  id1 = trash("/t_b.txt");
  crear("/t_b.txt", "version 2");
  id2 = trash("/t_b.txt");
  int r2 = restore(id2);
  int v2 = contenido("/t_b.txt", "version 2");
  unlink("/t_b.txt");
  int r1 = restore(id1);
  int v1 = contenido("/t_b.txt", "version 1");
  ok = id1 > 0 && id2 > 0 && id1 != id2 && r1 == 0 && r2 == 0 && v1 && v2;
  printf("  (id1 = %d, id2 = %d, version 2 ok = %s, version 1 ok = %s)\n", id1, id2, v2 ? "si" : "no", v1 ? "si" : "no");
  resultado(ok, ok ? "ambas versiones se conservaron por separado" : "se perdio alguna version");
}

void
prueba_archivo_grande(void)
{
  int fd, i, id, r, ok = 1;
  char bloque[512];

  inicio("Archivo grande (usa bloques directos e indirectos)",
         "crear /t_big.dat de 20 KB (40 bloques de 512 bytes con patron); trash; restore",
         "los 20 KB se recuperan identicos (no se copian datos: es el mismo inodo)");
  fd = open("/t_big.dat", O_CREATE | O_TRUNC | O_WRONLY);
  for (i = 0; i < 40; i++) {
    memset(bloque, 'A' + (i % 26), sizeof(bloque));
    write(fd, bloque, sizeof(bloque));
  }
  close(fd);
  struct stat antes, despues;
  stat("/t_big.dat", &antes);
  id = trash("/t_big.dat");
  r = restore(id);
  stat("/t_big.dat", &despues);
  fd = open("/t_big.dat", O_RDONLY);
  for (i = 0; i < 40 && ok; i++) {
    if (read(fd, bloque, sizeof(bloque)) != sizeof(bloque))
      ok = 0;
    for (int k = 0; k < sizeof(bloque) && ok; k++)
      if (bloque[k] != 'A' + (i % 26))
        ok = 0;
  }
  close(fd);
  ok = ok && id > 0 && r == 0 && despues.size == 20480 && antes.ino == despues.ino;
  printf("  (tamano = %d bytes, inodo antes = %d, inodo despues = %d)\n", (int)despues.size, antes.ino, despues.ino);
  resultado(ok, ok ? "contenido identico y mismo numero de inodo" : "datos corruptos o inodo distinto");
  unlink("/t_big.dat");
}

void
prueba_vaciar(void)
{
  int i1, i2, i3, n, ok;

  inicio("Vaciar la papelera",
         "trash de 3 archivos; emptytrash()",
         "emptytrash devuelve >= 3, /.trash queda vacia y esos ids ya no se pueden restaurar");
  crear("/t_1.txt", "1");
  crear("/t_2.txt", "2");
  crear("/t_3.txt", "3");
  i1 = trash("/t_1.txt");
  i2 = trash("/t_2.txt");
  i3 = trash("/t_3.txt");
  n = emptytrash();
  ok = n >= 3 && papeleravacia() && restore(i1) == TRASH_ENOENT &&
       restore(i2) == TRASH_ENOENT && restore(i3) == TRASH_ENOENT && !existe("/t_1.txt");
  printf("  (archivos eliminados = %d, papelera vacia = %s)\n", n, papeleravacia() ? "si" : "no");
  resultado(ok, ok ? "papelera vaciada y espacio liberado" : "la papelera no se vacio correctamente");
}

int
main(void)
{
  printf("=============================================\n");
  printf("  PRUEBAS DE LA PAPELERA DE RECICLAJE - xv6\n");
  printf("=============================================\n");

  // Estado inicial limpio
  emptytrash();
  unlink("/t_a.txt"); unlink("/t_b.txt");
  unlink("/t_docs/notas.txt"); unlink("/t_docs");
  unlink("/t_tmp/x.txt"); unlink("/t_tmp");

  prueba_enviar();          // pruebas 1 y 2
  prueba_subdirectorio();   // prueba 3
  prueba_conflicto();       // prueba 4
  prueba_dir_eliminado();   // prueba 5
  prueba_invalidas();       // prueba 6
  prueba_mismo_nombre();    // prueba 7
  prueba_archivo_grande();  // prueba 8
  prueba_vaciar();          // prueba 9

  // Limpieza
  unlink("/t_a.txt"); unlink("/t_b.txt");
  unlink("/t_docs/notas.txt"); unlink("/t_docs");
  unlink("/t_tmp/x.txt"); unlink("/t_tmp");

  printf("\n=============================================\n");
  printf("  RESULTADO: %d de %d pruebas aprobadas\n", aprobadas, total);
  printf("=============================================\n");
  exit(aprobadas == total ? 0 : 1);
}
