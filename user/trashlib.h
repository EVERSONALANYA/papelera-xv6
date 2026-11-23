// trashlib.h - funciones auxiliares de usuario para leer la papelera.
// Lo incluyen trash.c y restore.c.

#include "kernel/param.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"

// ¿La cadena es un número entero positivo?
static __attribute__((unused)) int
esnumero(char *s)
{
  if (*s == 0)
    return 0;
  for (; *s; s++)
    if (*s < '0' || *s > '9')
      return 0;
  return 1;
}

// Arma "/.trash/<id>" o "/.trash/<id>.p" en buf.
static __attribute__((unused)) void
rutaitem(int id, int meta, char *buf)
{
  char num[12];
  int i = 0, j, n = id;

  do {
    num[i++] = '0' + n % 10;
    n /= 10;
  } while (n > 0);
  strcpy(buf, TRASH_DIR "/");
  j = strlen(buf);
  while (i > 0)
    buf[j++] = num[--i];
  if (meta) {
    buf[j++] = '.';
    buf[j++] = 'p';
  }
  buf[j] = 0;
}

// Lee la ruta original de un elemento. Devuelve 0 si existe.
static __attribute__((unused)) int
leerorigen(int id, char *buf, int max)
{
  char ruta[32];
  int fd, n;

  rutaitem(id, 1, ruta);
  if ((fd = open(ruta, O_RDONLY)) < 0)
    return -1;
  n = read(fd, buf, max - 1);
  close(fd);
  if (n < 0)
    n = 0;
  buf[n] = 0;
  return 0;
}

// Mayor id presente en la papelera (0 si está vacía o no existe).
static __attribute__((unused)) int
maxid(void)
{
  int fd, id, max = 0;
  struct dirent de;
  char nombre[DIRSIZ + 1];

  if ((fd = open(TRASH_DIR, O_RDONLY)) < 0)
    return 0;
  while (read(fd, &de, sizeof(de)) == sizeof(de)) {
    if (de.inum == 0)
      continue;
    memmove(nombre, de.name, DIRSIZ);
    nombre[DIRSIZ] = 0;
    if (esnumero(nombre) && (id = atoi(nombre)) > max)
      max = id;
  }
  close(fd);
  return max;
}

// Devuelve el nombre final de una ruta ("/docs/a.txt" -> "a.txt").
static __attribute__((unused)) char *
basename(char *ruta)
{
  char *p = ruta + strlen(ruta);
  while (p > ruta && *(p - 1) != '/')
    p--;
  return p;
}
