// rm - elimina archivos.
// Con la papelera de reciclaje: por defecto los archivos se envían a /.trash
// y se pueden recuperar con "restore". Con -f se eliminan definitivamente.

#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/trash.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int i, inicio = 1, forzar = 0, id, error = 0;
  struct stat st;

  if (argc >= 2 && strcmp(argv[1], "-f") == 0) {
    forzar = 1;
    inicio = 2;
  }
  if (argc <= inicio) {
    fprintf(2, "Uso: rm [-f] archivos...\n");
    fprintf(2, "  sin -f : envia los archivos a la papelera (recuperables con restore)\n");
    fprintf(2, "  -f     : elimina definitivamente\n");
    exit(1);
  }

  for (i = inicio; i < argc; i++) {
    if (forzar) {
      if (unlink(argv[i]) < 0) {
        fprintf(2, "rm: no se pudo eliminar %s\n", argv[i]);
        error = 1;
      }
      continue;
    }

    id = trash(argv[i]);
    if (id > 0) {
      printf("rm: '%s' enviado a la papelera (id %d). Recuperalo con: restore %d\n", argv[i], id, id);
    } else if (id == TRASH_ENOENT) {
      fprintf(2, "rm: %s no existe\n", argv[i]);
      error = 1;
    } else if (id == TRASH_EINVAL && stat(argv[i], &st) == 0 && st.type == T_DIR) {
      // Los directorios no pasan por la papelera: se eliminan como antes (si estan vacios).
      if (unlink(argv[i]) < 0) {
        fprintf(2, "rm: no se pudo eliminar el directorio %s (debe estar vacio)\n", argv[i]);
        error = 1;
      }
    } else if (id == TRASH_EINVAL) {
      fprintf(2, "rm: %s no se puede enviar a la papelera (dispositivo o ya esta en la papelera). Usa rm -f o emptytrash\n", argv[i]);
      error = 1;
    } else {
      fprintf(2, "rm: la papelera esta llena; vaciala con emptytrash\n");
      error = 1;
    }
  }
  exit(error);
}
