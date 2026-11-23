#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int fd, mf, n;
  struct dirent de;
  struct stat st;
  char nombre[DIRSIZ + 1];
  char ruta[128];
  char meta[128];
  char original[256];

  if(argc != 1){
    fprintf(2, "Uso: trash\n");
    exit(1);
  }

  fd = open("/.trash", O_RDONLY);
  if(fd < 0){
    fprintf(2, "trash: no existe la papelera\n");
    exit(1);
  }

  printf("ID\tTAMANO\tRUTA ORIGINAL\n");

  while(read(fd, &de, sizeof(de)) == sizeof(de)){
    if(de.inum == 0)
      continue;

    memset(nombre, 0, sizeof(nombre));
    memmove(nombre, de.name, DIRSIZ);

    if(strcmp(nombre, ".") == 0 ||
       strcmp(nombre, "..") == 0)
      continue;

    int valido = 1;
    for(int i = 0; nombre[i]; i++){
      if(nombre[i] < '0' || nombre[i] > '9'){
        valido = 0;
        break;
      }
    }

    if(!valido)
      continue;

    strcpy(ruta, "/.trash/");
    strcpy(ruta + strlen(ruta), nombre);

    if(stat(ruta, &st) < 0)
      continue;

    strcpy(meta, ruta);
    strcpy(meta + strlen(meta), ".p");

    mf = open(meta, O_RDONLY);
    if(mf < 0)
      continue;

    memset(original, 0, sizeof(original));
    n = read(mf, original, sizeof(original) - 1);
    close(mf);

    if(n < 0)
      continue;

    original[n] = 0;

    printf("%s\t%d\t%s\n",
           nombre, (int)st.size, original);
  }

  close(fd);
  exit(0);
}
