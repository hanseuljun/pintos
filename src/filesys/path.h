#ifndef PATH_H
#define PATH_H

struct path;

struct path *path_create(const char *path_str);
char *path_to_string(struct path *path);

#endif /* filesys/path.h */
