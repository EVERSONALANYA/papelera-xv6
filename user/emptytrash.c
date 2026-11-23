#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int resultado;

  if(argc != 1){
    fprintf(2, "Uso: emptytrash\n");
    exit(1);
  }

  resultado = emptytrash();

  if(resultado < 0){
    fprintf(2, "emptytrash: error al vaciar la papelera\n");
    exit(1);
  }

  printf("emptytrash: operacion completada\n");
  exit(0);
}
