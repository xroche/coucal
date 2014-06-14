#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "coucal.h"

int main(int argc, char **argv) {
  int i;
  coucal hashtable;
  struct_coucal_enum enumerator;
  coucal_item *item;

  if (argc == 1 || ( argc % 2 ) != 1 ) {
    printf("usage: %s [key value] ..\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  /* Create a new hashtable with default settings */
  hashtable = coucal_new(0);
  assert(hashtable != NULL);

  /* Fill hashtable */
  for(i = 1 ; i < argc ; i += 2) {
    coucal_write_pvoid(hashtable, argv[i], argv[i + 1]);
  }
  printf("stored %zu keys\n", coucal_nitems(hashtable));

  /* Check we have the items */
  printf("Checking keys:\n");
  for(i = 1 ; i < argc ; i += 2) {
    void *value = NULL;
    if (!coucal_read_pvoid(hashtable, argv[i], &value)) {
      assert(! "hashtable internal error!");
    }
    printf("%s=%s\n", argv[i], (const char*) value);
  }

  /* Enumerate */
  enumerator = coucal_enum_new(hashtable);
  printf("Enumerating keys:\n");
  while((item = coucal_enum_next(&enumerator)) != NULL) {
    printf("%s=%s\n", (const char*) item->name, (const char*) item->value.ptr);
  }

  /* Delete hashtable */
  coucal_delete(&hashtable);

  return EXIT_SUCCESS;
}

