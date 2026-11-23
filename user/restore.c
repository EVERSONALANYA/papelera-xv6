#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/trash.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int id, resultado;

  if(argc != 2){
    fprintf(2, "Uso: restore ID\n");
    exit(1);
  }

  id = atoi(argv[1]);

  if(id <= 0 || id > TRASH_MAXID){
    fprintf(2, "restore: ID invalido\n");
    exit(1);
  }

  resultado = restore(id);

  if(resultado < 0){
    fprintf(2, "restore: error %d al restaurar ID %d\n",
            resultado, id);
    exit(1);
  }

  printf("restore: archivo %d restaurado\n", id);
  exit(0);
}
