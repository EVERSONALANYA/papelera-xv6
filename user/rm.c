#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/trash.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int force = 0;
  int inicio = 1;
  int errores = 0;

  if(argc > 1 && strcmp(argv[1], "-f") == 0){
    force = 1;
    inicio = 2;
  }

  if(argc <= inicio){
    fprintf(2, "Uso: rm [-f] archivo...\n");
    exit(1);
  }

  for(int i = inicio; i < argc; i++){
    if(force){
      if(unlink(argv[i]) < 0){
        fprintf(2, "rm: no se pudo eliminar %s\n", argv[i]);
        errores = 1;
      }
    } else {
      int id = trash(argv[i]);
      if(id > 0)
        printf("rm: %s enviado a la papelera (ID %d)\n",
               argv[i], id);
      else {
        fprintf(2, "rm: error %d al eliminar %s\n",
                id, argv[i]);
        errores = 1;
      }
    }
  }

  exit(errores);
}
