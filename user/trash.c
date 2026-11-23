// trash - muestra el contenido de la papelera de reciclaje.
// Uso: trash

#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/trash.h"
#include "user/user.h"
#include "user/trashlib.h"

// Imprime un número ocupando al menos "ancho" columnas.
static void
pnum(int n, int ancho)
{
  char buf[12];
  int i = 0;

  do {
    buf[i++] = '0' + n % 10;
    n /= 10;
  } while (n > 0);
  for (int k = i; k < ancho; k++)
    printf(" ");
  while (i > 0)
    printf("%c", buf[--i]);
}

int
main(int argc, char *argv[])
{
  int id, max, total = 0, bytes = 0;
  char ruta[32], origen[MAXPATH];
  struct stat st;

  max = maxid();
  if (max == 0) {
    printf("La papelera esta vacia.\n");
    exit(0);
  }

  printf("  ID   TAMANO  RUTA ORIGINAL\n");
  printf("----  -------  -------------------------\n");
  for (id = 1; id <= max; id++) {
    rutaitem(id, 0, ruta);
    if (stat(ruta, &st) < 0)
      continue;
    if (leerorigen(id, origen, sizeof(origen)) < 0)
      strcpy(origen, "(desconocida)");
    pnum(id, 4);
    printf("  ");
    pnum((int)st.size, 7);
    printf("  %s\n", origen);
    total++;
    bytes += st.size;
  }
  printf("\n%d archivo(s) en la papelera, %d bytes en total.\n", total, bytes);
  printf("Recupera con: restore <id|nombre>   Vacia con: emptytrash\n");
  exit(0);
}
