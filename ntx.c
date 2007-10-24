#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <dirent.h>
#include <zlib.h>
#include "hash_table.h"

/* Buffer size definitions. */
#define FILE_MAX      1024
#define BUFFER_MAX    8192
#define SUMMARY_WIDTH 58
#define PADDING_WIDTH 7

/* Builtin Subdirectories. */
#define NTX_DIR       ".ntx"
#define TAGS_DIR      "tags"
#define REFS_DIR      "refs"
#define NOTES_DIR     "notes"
#define INDEX_FILE    "index"


/* Utility/Refactored functions. */
void die(char *fmt, ...)
{
  va_list args;

  va_start(args, fmt);
  printf(fmt, args);
  va_end(args);
  exit(EXIT_FAILURE);
}

/* 'buf' should be SUMMARY_WIDTH + 2 bytes long. */
int ntx_summary(char *file, char *buf)
{
  FILE *f = fopen(file, "r");
  char *temp;

  if(!f) return -1;

  fgets(buf, SUMMARY_WIDTH+2, f);
  fclose(f);

  /* Set a null at the maximum position to ensure we don't overdo it. */
  temp = strchr(buf, '\n');
  if(temp) temp[1] = '\0';
  else {
    unsigned int end = strlen(buf);

    buf[end-4] = '.';
    buf[end-3] = '.';
    buf[end-2] = '.';
    buf[end-1] = '\n';
  }

  return 1;
}

/* Open the file, read the whole thing in a line at a time,
 * replacing the line beginning with the 4-digit hex 'id' with
 * the line 'fix'.
 * Because we don't know the max line length as we do when
 * index files are guaranteed, we need to parse after we load.
 */
char *ntx_buffer(char *file)
{
  char *bbuf = NULL;
  unsigned int blen = BUFFER_MAX, bpos = 0;
  unsigned int len;
  gzFile *f;

  if(!(f = gzopen(file, "r"))) return NULL;
  if(!(bbuf = malloc(blen))) return NULL;

  /* Read the entire file into the buffer. */
  while((len = gzread(f, bbuf + bpos, BUFFER_MAX))) {
    bpos += len;
    if((blen - bpos) < BUFFER_MAX) {
      blen += BUFFER_MAX;
      if(!(bbuf = realloc(bbuf, blen))) return NULL;
    }
  }
  gzclose(f);
  return bbuf;
}

/* XXX: Error checking. */
char *ntx_tagstolist(char *id, char **tags)
{
  char *list, *pos, **cur;
  unsigned int len = 7; /* ID + tab + newline + NULL. */

  /* Precompute the length of the tags. */
  for(cur = tags; *cur; cur++) len += strlen(*cur) + 1;
  list = malloc(len);

  strncpy(list, id, 4);
  list[4] = '\t';

  pos = list + 5;

  for(cur = tags; *cur; cur++) {
    strcpy(pos, *cur);
    len = strlen(*cur);
    pos[len] = ';';
    pos += len + 1;
  }

  strcpy(pos, "\n");
  return list;
}

int ntx_replace(char *file, char *id, char *fix)
{
  char *buf;
  char *ptr, *end;
  gzFile *f;

  if(!(buf = ntx_buffer(file))) return -1;
  if(!(f = gzopen(file, "w"))) return -1;

  /* Parse the buffer contents to find the given position. */
  for(ptr = buf; ptr; ptr = end+1) {
    end = strchr(ptr, '\n');
    if(strncmp(id, ptr, 4) == 0) {
      if(fix) gzputs(f, fix);
    } else {
      if(end) gzwrite(f, ptr, end-ptr);
      else gzputs(f, ptr);
    }
  }
  gzclose(f);
  free(buf);

  return 1;
}

char *ntx_find(char *file, char *id)
{
  char *buf;
  char *ptr, *end;

  if(!(buf = ntx_buffer(file))) return NULL;

  /* Parse the buffer contents to find the given position. */
  for(ptr = buf; ptr; ptr = end+1) {
    end = strchr(ptr, '\n');
    if(strncmp(id, ptr, 4) == 0) {
      *end = '\0';
      ptr = strdup(ptr);
      free(buf);
      return ptr;
    }
  }
  free(buf);

  return NULL;
}

int ntx_append(char *file, char *str)
{
  gzFile *f = gzopen(file, "a");

  if(!f) return -1;
  if(gzputs(f, str) == -1) return -1;
  gzclose(f);

  return 1;
}

void ntx_editor(char *file)
{
  char *editor = getenv("EDITOR");
  pid_t child;

  if(!editor) die("ERROR - The environment variable EDITOR is unset.");

  if((child = fork()) == 0) execlp(editor, editor, file, (char*)NULL);
  else waitpid(child, NULL, 0);
}


/* Front-end functions, user interaction. */
int ntx_add(char **tags)
{
  char file[FILE_MAX], note[SUMMARY_WIDTH + PADDING_WIDTH];
  char **ptr, *tmp;
  unsigned int hash;

  srand(time(NULL));
  hash = rand() & 0xffff;

  while(1) {
    FILE *nout;

    if(!snprintf(file, FILE_MAX, NOTES_DIR"/%04x", hash)) return -1;
    if(!(nout = fopen(file, "r"))) break;
    fclose(nout);
    hash = (hash + 1) & 0xffff;
  }

  /* Fire up the editor to create the note. */
  ntx_editor(file);

  /* Get the summary line, and write it in abbreviated form to each index. */
  if(ntx_summary(file, note+5) < 1) {
    puts("No new note was recorded.");
    return 0;
  }

  /* Fill in the identification information. */
  strncpy(note, file+6, 4);
  note[4] = '\t';

  for(ptr = tags; *ptr != NULL; ptr++) {
    if(!snprintf(file, FILE_MAX, TAGS_DIR"/%s", *ptr)) return -1;
    if(ntx_append(file, note) < 1) return -1;
  }

  /* Add the new note to the base index. */
  if(ntx_append(INDEX_FILE, note) < 1) return -1;

  /* Append the tags to the backreference file. */
  if(!snprintf(file, FILE_MAX, REFS_DIR"/%2s", file+6)) return -1;
  
  tmp = ntx_tagstolist(note, tags);
  if(ntx_append(file, tmp) < 1) return -1;
  free(tmp);

  /* Dump the summary to STDOUT as confirmation that everything went well. */
  fputs(note, stdout);

  return 0;
}

int ntx_edit(char *id)
{
  char file[FILE_MAX], head[SUMMARY_WIDTH + PADDING_WIDTH];
  char note[SUMMARY_WIDTH + PADDING_WIDTH];

  if(!snprintf(file, FILE_MAX, NOTES_DIR"/%s", id)) return -1;

  /* Check that the note exists first. */
  if(ntx_summary(file, head+5) < 1) die("ERROR - Invalid ID %s\n", id);
  ntx_editor(file);

  /* See if the header has changed; If so, rewrite the headers. */
  if(ntx_summary(file, note+5) < 1) die("ERROR - Unable to read %s\n", id);
  if(strcmp(head+5, note+5)) {
    char *tags, *cur, *ptr;

    /* Fill in the identification information. */
    strncpy(note, id, 4);
    note[4] = '\t';

    if(!snprintf(file, FILE_MAX, REFS_DIR"/%2s", id)) return -1;
    tags = ntx_find(file, id);

    cur = tags + 5; /* Skip the ID, plus the tab. */
    while((ptr = strchr(cur, ';'))) {
      *ptr = '\0';
      /* Update the tags - O(n) search through the affected indices. */
      if(!snprintf(file, FILE_MAX, TAGS_DIR"/%s", cur)) return -1;
      if(ntx_replace(file, id, note) < 1) return -1;
      cur = ptr + 1;
    }
    free(tags);
  }

  /* Dump the summary to STDOUT as confirmation that everything went well. */
  fputs(note, stdout);

  return 0;
}

/* Structure for sorting files by size. */
struct fstats {
  char *path;
  unsigned int size;
};

int ntx_sortstat(const void *a, const void *b)
{
  return ((struct fstats*)a)->size - ((struct fstats*)b)->size;
}

/* Helper functions for calculating multi-tag intersection. */
unsigned long hash_line(void *v)
{
  /* Just hash the 4-char prefix. */
  return hasht_hash(((char*)v)+1, 4, 0);
}

unsigned long hash_val(void *v)
{
  /* Just hash the 4-char prefix. */
  return hasht_hash((char*)v, 4, 0);
}

int cmp_line(void *a, void *b)
{
  /* Just compare the 4-char prefix. */
#define Cabv(v) (((char*)a)[v] == ((char*)b)[v])
  return ((Cabv(1) && Cabv(2) && Cabv(3) && Cabv(4)) ? 0 : 1);
#undef  Cabv
}

int cmp_val(void *a, void *b)
{
  /* Just compare the 4-char prefix. */
#define Cabv(v) (((char*)a)[v] == ((char*)b)[v-1])
  return ((Cabv(1) && Cabv(2) && Cabv(3) && Cabv(4)) ? 0 : 1);
#undef  Cabv
}

/* We use compression as much as possible to minimize loading times. */
int ntx_list(char **tags, unsigned int tagc)
{
  char line[SUMMARY_WIDTH + PADDING_WIDTH];
  gzFile *f;

  /* Too many tags for proper ref-counting, or even sane evaluation. */
  if(tagc > 127) die("ERROR - Too many (more than 127) tags.");

  if(tagc == 0) { /* No tags specified, open the index. */
    f = gzopen(INDEX_FILE, "r");
    if(!f) return 0;

    /* Duplicate each line from the index to STDOUT. */
    while(gzgets(f, line, SUMMARY_WIDTH + PADDING_WIDTH)) fputs(line, stdout);
  } else if(tagc == 1) { /* No need to calculate the intersection. */
    char name[FILE_MAX];

    if(!snprintf(name, FILE_MAX, TAGS_DIR"/%s", *tags)) return -1;

    f = gzopen(name, "r");
    if(!f) die("ERROR - No such tag: %s\n", tags[0]);

    /* Duplicate each line from the index to STDOUT. */
    while(gzgets(f, line, SUMMARY_WIDTH + PADDING_WIDTH)) fputs(line, stdout);
  } else { /* Calculate the union of the sets from the tag files. */
    char *name;
    hash_t *table;
    struct stat temp;
    struct fstats *files = malloc(sizeof(struct fstats) * tagc);
    unsigned int i, len, exists;

    /* Sort the files; We'll likely be best starting with the smallest. */
    /* Stat and load the files into the buffers for sorting. */
    for(i = 0; i < tagc; i++) {
      len = 6 + strlen(tags[i]);
      name = malloc(len);
      if(!snprintf(name, len, TAGS_DIR"/%s", tags[i])) return -1;
      if(stat(name, &temp) != 0) die("ERROR - No such tag: %s\n", tags[i]);

      files[i].path = name;
      files[i].size = temp.st_size;
    }

    qsort(files, tagc, sizeof(struct fstats), ntx_sortstat);

    /* Now, load the first, and check it with each hash. */
    f = gzopen(files[0].path, "r");
    table = hasht_init(120, free, hash_line, hash_val, cmp_line, cmp_val);

    if(!table) return -1;

    /* Read each line in the index into the hash. */
    while(gzgets(f, line, SUMMARY_WIDTH + PADDING_WIDTH)) {
      name = malloc(strlen(line)+2); /* Save room for the ref and null. */
      if(!name) return -1;
      name[0] = 1;
      strcpy(name+1, line);
      name = hasht_add(table, name);
      if(name) {
        puts(name);
        free(name);
      }
    }
    gzclose(f);
   
    /* Load and check each hash against this one, incrementing found refs. *
     * We will abort if exists == 0, meaning none were found.              */
    for(i = 1; i < tagc; i++) {
      exists = 0;
      f = gzopen(files[i].path, "r");
      while(gzgets(f, line, SUMMARY_WIDTH + PADDING_WIDTH)) {
        if((name = hasht_get(table, line))) {
          name[0]++; /* Increment the refcount. */
          exists = 1;
        }
      }
      gzclose(f);
      if(exists == 0) die("No notes exist in the intersection of those tags.");
    }

    /* Print all of the tags which had 'tagc' references. */
    while((name = hasht_next(table)))
      if(name[0] == tagc) fputs(name+1, stdout);

    /* Clean up the hash and fstats structures. */
    hasht_free(table);
    free(files);
  }

  fputc('\n', stdout);

  return 0;
}

int ntx_put(char *id)
{
  FILE *f;
  char file[FILE_MAX];
  char buffer[BUFFER_MAX];

  if(!snprintf(file, FILE_MAX, NOTES_DIR"/%s", id)) return -1;
  if(!(f = fopen(file, "r"))) die("ERROR - Invalid ID %s\n", id);

  /* Duplicate each line from the note to STDOUT. */
  while(fgets(buffer, BUFFER_MAX, f)) fputs(buffer, stdout);
  return 0;
}

int ntx_del(char *id)
{
  char file[FILE_MAX], *buf, *ptr, *cur;

  if(!snprintf(file, FILE_MAX, REFS_DIR"/%2s", id)) return -1;

  /* Find the line describing the tags of the given ID. */
  buf = ntx_find(file, id);

  cur = buf + 5; /* Skip the ID, plus the tab. */
  while((ptr = strchr(cur, ';'))) {
    *ptr = '\0';
    /* Delete the tags - O(n) search through the affected indices. */
    if(!snprintf(file, FILE_MAX, TAGS_DIR"/%s", cur)) return -1;
    if(ntx_replace(file, id, NULL) < 1) return -1;
    cur = ptr + 1;
  }

  /* Remove it from the index. */
  if(ntx_replace(INDEX_FILE, id, NULL) < 1) return -1;

  /* Remove the backreference. */
  if(!snprintf(file, FILE_MAX, REFS_DIR"/%2s", id)) return -1;
  if(ntx_replace(file, id, NULL) < 1) return -1;

  /* Remove the note itself from notes/ */
  if(!snprintf(file, FILE_MAX, NOTES_DIR"/%s", id)) return -1;
  if(unlink(file) != 0) return -1;

  free(buf);
  return 0;
}

int ntx_tags(char **argv, unsigned int argc)
{
  if(argc == 0) { /* List all tags in the database. */
    DIR *dir = opendir(TAGS_DIR);
    struct dirent *cur;

    if(!dir) return -1;

    while((cur = readdir(dir))) if(cur->d_name[0] != '.') puts(cur->d_name);
    fputc('\n', stdout);

    closedir(dir);
  } else if(argc == 1) { /* List all tags of a note. */
    char file[FILE_MAX], *buf, *ptr, *cur;

    if(!snprintf(file, FILE_MAX, REFS_DIR"/%2s", *argv)) return -1;
    if(!(buf = ntx_find(file, *argv))) die("ERROR - No such note %s\n", *argv);

    cur = buf + 5; /* Skip the ID, plus the tab. */
    while((ptr = strchr(cur, ';'))) { /* Write out each of the tags. */
      *ptr = '\0';
      puts(cur);
      cur = ptr + 1;
    }

    free(buf);
  } else { /* Re-tag a note. */
    char file[FILE_MAX], desc[SUMMARY_WIDTH + 2];
    char *tmp, *orig, **cur;

    /* Get the summary in case we need to write it. */
    if(!snprintf(file, FILE_MAX, NOTES_DIR"/%s", *argv)) return -1;
    if(ntx_summary(file, desc+5) < 1) die("ERROR - No such note %s\n", *argv);

    /* Fill in the identification information. */
    strncpy(desc, *argv, 4);
    desc[4] = '\t';

    /* Read in the original listing of tags for this file. */
    if(!snprintf(file, FILE_MAX, REFS_DIR"/%2s", *argv)) return -1;
    if(!(orig = ntx_find(file, *argv))) die("ERROR - No such note %s\n", *argv);

    /* Add any tags which don't yet exist. */
    for(cur = argv+1; *cur; cur++) {
      if(!strstr(orig, *cur)) { /* Add the tag to the file. */
        if(!snprintf(file, FILE_MAX, TAGS_DIR"/%s", *cur)) return -1;
        if(!ntx_append(file, desc)) return -1;
      }
    }
    
    /* NULL-terminate the tags in the backreferences. */
    for(tmp = orig; (tmp = strchr(tmp, ';')); tmp++) *tmp = '\0';

    /* Remove any tags which don't exist any more. */
    for(tmp = orig; tmp; tmp += strlen(tmp) + 1) {
      for(cur = argv+1; *cur; cur++)
        if(strcmp(*cur, tmp) == 0) break;

      if(!*cur) { /* Remove any tags we didn't find. */
        if(!snprintf(file, FILE_MAX, TAGS_DIR"/%s", tmp)) return -1;
        if(ntx_replace(file, *argv, NULL) < 1) return -1;
      }
    }

    /* Update the backreference with the new set of tags. */
    tmp = ntx_tagstolist(*argv, argv+1);
    if(!snprintf(file, FILE_MAX, REFS_DIR"/%2s", *argv)) return -1;
    if(ntx_replace(file, *argv, tmp) < 1) {
      free(tmp);
      return -1;
    }

    free(tmp);
  }

  return 0;
}

void ntx_usage(int retcode)
{
  puts("Usage:\tntx [mode] [arguments] ..\n");
  puts("Modes:\t-a [tags]\t\tAdd a note from STDIN to tags.");
  puts("\t-e [hex]\t\tEdit the note identified by the ID hex.");
  puts("\t-p [hex]\t\tPrint the note with the ID 'hex' to STDOUT.");
  puts("\t-l <tags>\t\tList the notes in the intersection of tags.");
  puts("\t-d [hex]\t\tDelete the node identified by the ID hex.");
  puts("\t-t\t\tPrint a list of the tags in the database.\n");
  puts("\t-h or --help\t\tPrint this information.\n");

  puts("Input file format:");
  puts("\tntx assumes that a short first line may be used as a summary of");
  puts("\tthe note in question. No further assumptions are made about the");
  puts("\ttype, format, or size of the file.\n");

  puts("Notes on the output of 'ntx -l':");
  puts("\t'ntx -l' outputs a four-byte hexidecimal ID, followed by a tab,");
  printf("\tand then a brief summary comprised of the first %d bytes of the\n", SUMMARY_WIDTH);
  puts("\tfirst line of the saved note. This is currently not updated when");
  puts("\ta note changes.\n");

  exit(retcode);
}

int main(int argc, char **argv)
{
  char file[FILE_MAX];

  /* Very few arguments, so we use a hand-written parser. */
  if(argc < 2) ntx_usage(EXIT_FAILURE);

  if(strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)
    ntx_usage(EXIT_SUCCESS);

  if(strlen(argv[1]) != 2 || argv[1][0] != '-') ntx_usage(EXIT_FAILURE);

  /* Ensure our target directory exists. */
  if(!snprintf(file, FILE_MAX, "%s/"NTX_DIR, getenv("HOME")))
    die("ERROR - Unknown execution failure.\n");

  if(chdir(file) == -1) {
    mkdir(file, S_IRWXU);
    if(chdir(file) == -1) die("ERROR - Unknown execution failure.\n");
    mkdir(TAGS_DIR, S_IRWXU);
    mkdir(REFS_DIR, S_IRWXU);
    mkdir(NOTES_DIR, S_IRWXU);
  }

  if(argv[1][1] == 'a' && argc >= 3) {
    if(ntx_add(argv+2) != 0) die("ERROR - Unknown execution failure.\n");
  } else if(argv[1][1] == 'e' && argc == 3) {
    if(ntx_edit(argv[2]) != 0) die("ERROR - Unknown execution failure.\n");
  } else if(argv[1][1] == 'l' && argc >= 2) {
    if(ntx_list(argv+2, argc - 2) != 0)
      die("ERROR - Unknown execution failure.\n");
  } else if(argv[1][1] == 't') {
    if(ntx_tags(argv+2, argc - 2) != 0)
      die("ERROR - Unknown execution failure.\n");
  } else if(argv[1][1] == 'p' && argc == 3) {
    if(ntx_put(argv[2]) != 0) die("ERROR - Unknown execution failure.\n");
  } else if(argv[1][1] == 'd' && argc == 3) {
    printf("ERROR - Feature not yet implemented.\n");
  } else ntx_usage(EXIT_FAILURE);

  return EXIT_SUCCESS;
}
