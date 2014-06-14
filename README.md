coucal
======

**Coucal**, Cuckoo-hashing-based hashtable with stash area C library.

> [Wikipedia] A coucal is one of about 30 species of birds in the cuckoo family. All of them belong in the subfamily Centropodinae and the genus Centropus. Unlike many Old World cuckoos, coucals are not brood parasites.

This is an implementation of the cuckoo hashing algorithm (http://en.wikipedia.org/wiki/Cuckoo_hashing) with a stash area (http://research.microsoft.com/pubs/73856/stash-full.9-30.pdf).

This allows an efficient generic hashtable implementation, with the following features:
* guaranteed constant time (Θ(1)) lookup
* guaranteed constant time (Θ(1)) delete or replace
* average constant time (O(1)) insert
* one large memory chunk for table (and one for the key pool)
* simple enumeration

This library has been thoroughly tested, and is currently used by the [HTTrack](http://www.httrack.com/) project in production.

**Example**

```c
coucal hashtable = coucal_new(0);
coucal_write_pvoid(hashtable, "foo", "bar");

void *value;
coucal_read_pvoid(hashtable, "foo", &value);
printf("value==%s\n", (char*) value);

struct_coucal_enum enumerator = coucal_enum_new(hashtable);
coucal_item *item;
while((item = coucal_enum_next(&enumerator)) != NULL) {
  printf("%s=%s\n", (const char*) item->name, (const char*) item->value.ptr);
}
```
