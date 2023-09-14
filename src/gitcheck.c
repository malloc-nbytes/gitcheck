#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <dirent.h>
#include <sys/stat.h>

bool uncommitted_changes(const char *path)
{
  char cmd[1024];
  snprintf(cmd, sizeof(cmd), "git -C %s status --porcelain", path);

  FILE *fp = popen(cmd, "r");
  if (!fp) {
    fprintf(stderr, "[gitcheck ERR] failed to run cmd: %s\nIs git installed?", cmd);
    exit(EXIT_FAILURE);
  }

  char line[1024];
  while (fgets(line, sizeof(line), fp) != NULL) {
    if (line[0] == 'M' || line[1] == 'M') {
      return true;
    }
  }

  pclose(fp);
  return false;
}

bool is_git_repo(const char *path)
{
  char gpath[1024];
  snprintf(gpath, sizeof(gpath), "%s/.git", path);
  struct stat file_info;
  return stat(gpath, &file_info) == 0 && S_ISDIR(file_info.st_mode);
}

typedef struct {
  char **data;
  size_t len, cap;
} StrArray;

StrArray strarr = {0};

void walkdirs(const char *path)
{
  DIR *dir;
  struct dirent *entry;
  struct stat file_info;

  dir = opendir(path);
  if (!dir) {
    perror("[gitcheck ERR] could not open directory");
    return;
  }

  while ((entry = readdir(dir)) != NULL) {
    // Skip '.' and ".." dirs.
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }

    char fullpath[1024];
    if (path[strlen(path) - 1] == '/') {
      snprintf(fullpath, sizeof(fullpath), "%s%s", path, entry->d_name);
    } else {
      snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->d_name);
    }

    // Get information about the entry.
    if (stat(fullpath, &file_info) == -1) {
      perror("[gitcheck ERR] could not get entry information");
      continue;
    }

    if (is_git_repo(fullpath) && uncommitted_changes(fullpath)) {
      if (strarr.len == strarr.cap) {
        strarr.cap *= 2;
        strarr.data = realloc(strarr.data, sizeof(char *) * strarr.cap);
      }
      strarr.data[strarr.len++] = strdup(fullpath);
    } else if (S_ISDIR(file_info.st_mode)) {
      walkdirs(fullpath);
    }
  }

  closedir(dir);
}

void usage(void)
{
  fprintf(stderr, "[gitcheck] usage: ./gitcheck <filepath>\n");
  exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
  if (argc != 2) {
    usage();
  }
  argv++;

  strarr.data = malloc(sizeof(char *) * 10);
  strarr.cap = 10;

  walkdirs(*argv);

  size_t i;
  for (i = 0; i < strarr.len; i++) {
    printf("[gitcheck] Git repo with unstaged/uncommitted/unpushed changes: %s\n", strarr.data[i]);
  }

  if (i == 0) {
    printf("[gitcheck] No git repos with unstaged/uncommitted/unpushed changes found.\n");
  }

  return 0;
}
