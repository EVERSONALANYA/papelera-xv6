//
// trash.c - Papelera de reciclaje para xv6
//
// Proyecto final - IS-380 Sistemas Operativos (UNSCH)
//
// En xv6 original, unlink() (comando rm) elimina un archivo para siempre:
// borra la entrada del directorio y, cuando nlink llega a 0, el inodo y sus
// bloques de datos se liberan. Esta extensión agrega tres llamadas al sistema:
//
//   int trash(char *ruta)  Mueve el archivo a /.trash y devuelve su id (>0).
//   int restore(int id)    Devuelve el archivo id a su ubicación original.
//   int emptytrash(void)   Elimina definitivamente todo el contenido de /.trash.
//
// Cómo se guarda un archivo en la papelera:
//   /.trash/<id>     enlace duro al MISMO inodo del archivo (no se copian datos)
//   /.trash/<id>.p   archivo de metadatos con la ruta absoluta original
//
// Mover a la papelera es "enlazar en /.trash y desenlazar del directorio
// original": el inodo nunca llega a nlink = 0, así que sus bloques NO se
// liberan. Solo emptytrash() hace que nlink llegue a 0 y libera el espacio.
//
// Todas las modificaciones al disco se hacen dentro de transacciones del log
// (begin_op / end_op), igual que sys_link y sys_unlink, para que una caída
// del sistema no deje el sistema de archivos inconsistente.
//

#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "spinlock.h"
#include "proc.h"
#include "fs.h"
#include "sleeplock.h"
#include "file.h"
#include "fcntl.h"
#include "trash.h"

#define TRASH_MAXDEPTH 32   // profundidad máxima de directorios al armar la ruta

// Convierte un entero positivo a texto (el kernel no tiene sprintf).
static void
itoa(int n, char *buf)
{
  char tmp[12];
  int i = 0, j = 0;

  if (n == 0)
    tmp[i++] = '0';
  while (n > 0) {
    tmp[i++] = '0' + (n % 10);
    n /= 10;
  }
  while (i > 0)
    buf[j++] = tmp[--i];
  buf[j] = 0;
}

// Nombres de las dos entradas de un elemento: "<id>" y "<id>.p".
static void
idnames(int id, char *nombre, char *meta)
{
  int n;

  itoa(id, nombre);
  n = strlen(nombre);
  memmove(meta, nombre, n);
  meta[n] = '.';
  meta[n + 1] = 'p';
  meta[n + 2] = 0;
}

// Devuelve el inodo de /.trash (referenciado y SIN bloquear).
// Si no existe y crear = 1, lo crea. Debe llamarse dentro de begin_op().
static struct inode *
trashdir(int crear)
{
  char ruta[] = TRASH_DIR;
  struct inode *td;

  if ((td = namei(ruta)) == 0) {
    if (!crear)
      return 0;
    if ((td = create(ruta, T_DIR, 0, 0)) == 0)
      return 0;
    iunlock(td);           // create() lo devuelve bloqueado
    return td;
  }
  ilock(td);
  if (td->type != T_DIR) { // existe /.trash pero no es directorio
    iunlockput(td);
    return 0;
  }
  iunlock(td);
  return td;
}

// Construye la ruta absoluta del directorio dir (referenciado, sin bloquear).
// Sube por ".." hasta la raíz y en cada padre busca el nombre del hijo.
// Bloquea un solo inodo a la vez para evitar interbloqueos con namei().
static int
dirpath(struct inode *dir, char *buf, int max)
{
  char tmp[MAXPATH];
  int pos = MAXPATH - 1, len, profundidad = 0, hallado;
  struct inode *cur, *padre;
  struct dirent de;
  uint off;

  tmp[pos] = 0;
  cur = idup(dir);
  while (cur->inum != ROOTINO) {
    if (++profundidad > TRASH_MAXDEPTH) {
      iput(cur);
      return -1;
    }
    ilock(cur);
    padre = dirlookup(cur, "..", 0);
    iunlock(cur);
    if (padre == 0) {
      iput(cur);
      return -1;
    }

    // Buscar en el padre la entrada que apunta a cur.
    ilock(padre);
    hallado = 0;
    for (off = 2 * sizeof(de); off < padre->size; off += sizeof(de)) {
      if (readi(padre, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
        break;
      if (de.inum == cur->inum) {
        hallado = 1;
        break;
      }
    }
    iunlock(padre);

    if (!hallado) {
      iput(cur);
      iput(padre);
      return -1;
    }
    for (len = 0; len < DIRSIZ && de.name[len]; len++)
      ;
    if (pos - len - 1 < 0) {
      iput(cur);
      iput(padre);
      return -1;
    }
    pos -= len;
    memmove(tmp + pos, de.name, len);
    tmp[--pos] = '/';
    iput(cur);
    cur = padre;
  }
  iput(cur);

  if (tmp[pos] == 0)       // el directorio era la raíz
    tmp[--pos] = '/';
  if (MAXPATH - pos > max)
    return -1;
  safestrcpy(buf, tmp + pos, max);
  return 0;
}

// Elimina la entrada "nombre" del directorio dp (que debe estar BLOQUEADO)
// y decrementa nlink del inodo. Si inum != 0, solo borra si la entrada
// apunta a ese inodo. Es el núcleo de sys_unlink, para archivos comunes.
static int
removeent(struct inode *dp, char *nombre, uint inum)
{
  struct inode *ip;
  struct dirent de;
  uint off;

  if ((ip = dirlookup(dp, nombre, &off)) == 0)
    return -1;
  if (inum != 0 && ip->inum != inum) {
    iput(ip);
    return -1;
  }
  ilock(ip);
  if (ip->type == T_DIR) { // la papelera solo maneja archivos
    iunlockput(ip);
    return -1;
  }
  memset(&de, 0, sizeof(de));
  if (writei(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
    panic("trash: removeent");
  ip->nlink--;
  iupdate(ip);
  iunlockput(ip);          // si nlink == 0, iput libera inodo y bloques
  return 0;
}

// ---------------------------------------------------------------------------
// int trash(char *ruta)
// Devuelve el id (> 0) con el que el archivo quedó en la papelera, o:
//   TRASH_ENOENT  el archivo no existe
//   TRASH_EINVAL  no es un archivo común (directorio, dispositivo),
//                 ya está en la papelera o la ruta es inválida
//   TRASH_EFULL   la papelera está llena
// ---------------------------------------------------------------------------
uint64
sys_trash(void)
{
  char ruta[MAXPATH], nombre[DIRSIZ + 1], orig[MAXPATH], completa[MAXPATH];
  char idn[DIRSIZ], idp[DIRSIZ], meta[MAXPATH];
  struct inode *td, *dp, *ip, *x, *mf;
  int id, ret, len;

  memset(nombre, 0, sizeof(nombre));
  if (argstr(0, ruta, MAXPATH) < 0)
    return TRASH_EINVAL;

  // Transacción 1: asegurar que exista /.trash.
  begin_op();
  td = trashdir(1);
  end_op();
  if (td == 0)
    return TRASH_EINVAL;

  // Transacción 2: mover el archivo (enlazar en /.trash y desenlazar el original).
  begin_op();
  ret = TRASH_EINVAL;
  if ((dp = nameiparent(ruta, nombre)) == 0) {
    ret = TRASH_ENOENT;
    goto fallo_td;
  }
  if (namecmp(nombre, ".") == 0 || namecmp(nombre, "..") == 0)
    goto fallo_dp;
  if (dp->inum == td->inum)      // no se recicla lo que ya está en la papelera
    goto fallo_dp;
  if (dirpath(dp, orig, MAXPATH) < 0)
    goto fallo_dp;

  // Ruta absoluta original = ruta del directorio + "/" + nombre.
  len = strlen(orig);
  if (len + 1 + strlen(nombre) + 1 > MAXPATH)
    goto fallo_dp;
  safestrcpy(completa, orig, MAXPATH);
  if (len > 1)
    completa[len++] = '/';
  safestrcpy(completa + len, nombre, MAXPATH - len);

  ilock(dp);
  ip = dirlookup(dp, nombre, 0);
  iunlock(dp);
  if (ip == 0) {
    ret = TRASH_ENOENT;
    goto fallo_dp;
  }
  ilock(ip);
  if (ip->type != T_FILE) {      // directorios y dispositivos no
    iunlockput(ip);
    goto fallo_dp;
  }
  ip->nlink++;                   // el nuevo enlace en /.trash
  iupdate(ip);
  iunlock(ip);

  // Buscar el primer id libre y enlazar el inodo como /.trash/<id>.
  ilock(td);
  for (id = 1; id <= TRASH_MAXID; id++) {
    idnames(id, idn, idp);
    if ((x = dirlookup(td, idn, 0)) == 0 && (x = dirlookup(td, idp, 0)) == 0)
      break;
    iput(x);
  }
  if (id > TRASH_MAXID || dirlink(td, idn, ip->inum) < 0) {
    iunlock(td);
    ilock(ip);
    ip->nlink--;                 // deshacer
    iupdate(ip);
    iunlockput(ip);
    ret = TRASH_EFULL;
    goto fallo_dp;
  }
  iunlock(td);

  // Quitar la entrada del directorio original (como unlink).
  ilock(dp);
  if (removeent(dp, nombre, ip->inum) < 0) {
    // La entrada cambió mientras tanto: deshacer el enlace en la papelera.
    iunlockput(dp);
    ilock(td);
    removeent(td, idn, ip->inum);
    iunlock(td);
    iput(ip);
    goto fallo_td;
  }
  iunlockput(dp);
  iput(ip);
  end_op();

  // Transacción 3: escribir los metadatos /.trash/<id>.p con la ruta original.
  begin_op();
  safestrcpy(meta, TRASH_DIR "/", MAXPATH);
  safestrcpy(meta + strlen(meta), idp, MAXPATH - strlen(meta));
  if ((mf = create(meta, T_FILE, 0, 0)) != 0) {
    writei(mf, 0, (uint64)completa, 0, strlen(completa));
    iunlockput(mf);
  }
  iput(td);
  end_op();
  return id;

fallo_dp:
  iput(dp);
fallo_td:
  iput(td);
  end_op();
  return ret;
}

// (restore y emptytrash se agregan en el siguiente commit)
