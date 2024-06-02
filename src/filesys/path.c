#include "path.h"
#include <list.h>
#include <string.h>
#include "filesys/directory.h"
#include "filesys/filesys.h"
#include "threads/malloc.h"

struct path_elem
  {
    char *name;
    struct list_elem list_elem;
  };

struct path
  {
    struct list elem_list;
  };

static char *
copy_string(const char *str)
{
  char *s = malloc (strlen (str) + 1);
  memcpy (s, str, strlen (str));
  s[strlen (str)] = '\0';
  return s;
}

struct path *
path_create (const char *path_str)
{
  struct path *path = malloc (sizeof (path));
  list_init (&path->elem_list);

  char *s = copy_string (path_str);
  char *token;
  char *save_ptr;
  for (token = strtok_r (s, "/", &save_ptr); token != NULL; token = strtok_r (NULL, "/", &save_ptr))
    {
      struct path_elem *path_elem = malloc (sizeof (path_elem));
      path_elem->name = copy_string (token);
      list_push_back (&path->elem_list, &path_elem->list_elem);
    }
  return path;
}

void
path_release(struct path *path)
{
  struct list_elem *e;
  while (!list_empty (&path->elem_list))
    {
      e = list_pop_front (&path->elem_list);
      struct path_elem *elem = list_entry (e, struct path_elem, list_elem);

      free (elem->name);
      free (elem);
    }

  free (path);
}

struct path *
path_copy(struct path *path)
{
  struct path *new_path = malloc (sizeof (new_path));
  list_init (&new_path->elem_list);

  for (struct list_elem *e = list_begin (&path->elem_list); e != list_end (&path->elem_list);
       e = list_next (e))
    {
      struct path_elem *elem = list_entry (e, struct path_elem, list_elem);
      struct path_elem *new_path_elem = malloc (sizeof (new_path_elem));
      new_path_elem->name = copy_string (elem->name);
      list_push_back (&new_path->elem_list, &new_path_elem->list_elem);
    }

  return new_path;
}

struct dir *
path_get_dir(struct path *path)
{
  struct dir *dir = dir_open_root ();

  for (struct list_elem *e = list_begin (&path->elem_list); e != list_end (&path->elem_list);
       e = list_next (e))
    {
      struct path_elem *elem = list_entry (e, struct path_elem, list_elem);
      struct dir *prev_dir = dir;
      dir = filesys_open_dir (dir, elem->name);
      dir_close (prev_dir);
    }

  return dir;
}

void
path_push_back(struct path *path, const char *str)
{
  char *s = copy_string (str);
  char *token;
  char *save_ptr;
  for (token = strtok_r (s, "/", &save_ptr); token != NULL; token = strtok_r (NULL, "/", &save_ptr))
    {
      struct path_elem *path_elem = malloc (sizeof (path_elem));
      path_elem->name = copy_string (token);
      list_push_back (&path->elem_list, &path_elem->list_elem);
    }
}

char *
path_pop_back(struct path *path)
{
  if (list_empty (&path->elem_list))
    return NULL;

  struct list_elem *e = list_pop_back (&path->elem_list);
  struct path_elem *elem = list_entry (e, struct path_elem, list_elem);
  char *name = elem->name;
  free (elem);
  return name;
}

void
path_sanitize (struct path *path)
{
  struct list_elem *e = list_begin (&path->elem_list);
  while (e != list_end (&path->elem_list))
    {
      struct path_elem *elem = list_entry (e, struct path_elem, list_elem);
      if (strcmp (elem->name, ".") == 0)
        {
          e = list_remove (e);
          continue;
        }
      e = list_next (e);
    }
}

char *path_get_string(struct path *path)
{
  if (list_empty (&path->elem_list))
    return copy_string ("/");

  size_t path_str_length = 0;
  for (struct list_elem *e = list_begin (&path->elem_list); e != list_end (&path->elem_list);
       e = list_next (e))
    {
      struct path_elem *elem = list_entry (e, struct path_elem, list_elem);
      path_str_length += strlen (elem->name) + 1;
    }

  size_t cursor = 0;
  char *path_str = malloc (path_str_length + 1);
  for (struct list_elem *e = list_begin (&path->elem_list); e != list_end (&path->elem_list);
       e = list_next (e))
    {
      struct path_elem *elem = list_entry (e, struct path_elem, list_elem);
      path_str[cursor] = '/';
      cursor++;
      memcpy (&path_str[cursor], elem->name, strlen (elem->name));
      cursor += strlen (elem->name);
    }
  
  path_str[path_str_length] = '\0';
  return path_str;
}
