#ifndef PATH_H
#define PATH_H

struct path;
struct dir;

struct path *path_create(const char *path_str);
void path_release(struct path *path);
struct path *path_copy(struct path *path);
struct dir *path_get_dir(struct path *path);
void path_push_back(struct path *path, const char *str);
char *path_pop_back(struct path *path);
char *path_get_string(struct path *path);

#endif /* filesys/path.h */
