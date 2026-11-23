// emptytrash - vacía la papelera y libera definitivamente el espacio.
// Uso: emptytrash [-y]    (-y: no pedir confirmación)

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  char resp[8];
  int n;

  if (!(argc >= 2 && strcmp(argv[1], "-y") == 0)) {
    printf("Los archivos de la papelera se eliminaran para siempre. Continuar? (s/n): ");
    gets(resp, sizeof(resp));
    if (resp[0] != 's' && resp[0] != 'S') {
      printf("Cancelado.\n");
      exit(0);
    }
  }
  n = emptytrash();
  printf("Papelera vaciada: %d archivo(s) eliminado(s) definitivamente.\n", n);
  exit(0);
}
