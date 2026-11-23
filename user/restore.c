// restore - recupera archivos de la papelera a su ubicación original.
// Uso: restore <id|nombre>...
//   restore 3          recupera el elemento con id 3
//   restore notas.txt  recupera el "notas.txt" eliminado más recientemente

#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/trash.h"
#include "user/user.h"
#include "user/trashlib.h"

// Busca por nombre (o ruta completa) el elemento más reciente. 0 si no hay.
static int
buscarpornombre(char *nombre)
{
  char origen[MAXPATH];
  int id;

  for (id = maxid(); id >= 1; id--) {
    if (leerorigen(id, origen, sizeof(origen)) < 0)
      continue;
    if (strcmp(origen, nombre) == 0 || strcmp(basename(origen), nombre) == 0)
      return id;
  }
  return 0;
}

int
main(int argc, char *argv[])
{
  int i, id, r, error = 0;
  char origen[MAXPATH];

  if (argc < 2) {
    fprintf(2, "Uso: restore <id|nombre>...   (usa 'trash' para ver los ids)\n");
    exit(1);
  }

  for (i = 1; i < argc; i++) {
    id = esnumero(argv[i]) ? atoi(argv[i]) : buscarpornombre(argv[i]);
    if (id == 0) {
      fprintf(2, "restore: '%s' no esta en la papelera\n", argv[i]);
      error = 1;
      continue;
    }
    if (leerorigen(id, origen, sizeof(origen)) < 0)
      strcpy(origen, "?");

    r = restore(id);
    if (r == 0)
      printf("restore: id %d recuperado en %s\n", id, origen);
    else if (r == TRASH_ENOENT)
      fprintf(2, "restore: no existe el id %d en la papelera\n", id);
    else if (r == TRASH_EEXIST)
      fprintf(2, "restore: ya existe %s. Renombra o elimina ese archivo y vuelve a intentar\n", origen);
    else if (r == TRASH_ENODIR)
      fprintf(2, "restore: el directorio original de %s ya no existe. Crealo con mkdir y vuelve a intentar\n", origen);
    else
      fprintf(2, "restore: no se pudo recuperar el id %d\n", id);
    if (r != 0)
      error = 1;
  }
  exit(error);
}
