// trash.h - Constantes de la papelera de reciclaje (kernel y programas de usuario)

#define TRASH_DIR    "/.trash"   // directorio de la papelera
#define TRASH_MAXID  9999        // máximo número de elementos

// Códigos de retorno de trash() y restore()
#define TRASH_ENOENT  -1   // no existe el archivo / el elemento
#define TRASH_EEXIST  -2   // ya existe un archivo con ese nombre en el destino
#define TRASH_ENODIR  -3   // el directorio original ya no existe
#define TRASH_EINVAL  -4   // operación inválida (directorio, dispositivo, ya está en la papelera)
#define TRASH_EFULL   -5   // la papelera está llena
