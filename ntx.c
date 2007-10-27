#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <zlib.h>
#include "hash_table.h"

/* XXX: Report I/O errors, memory errors, corrupt files, etc. */

/* Buffer size definitions. */
#define FILE_MAX      (FILENAME_MAX+1)
#define BUFFER_MAX    8192
#define SUMMARY_WIDTH 58
#define PADDING_WIDTH 7

/* Builtin Subdirectories. */
#define TAGS_DIR      "tags"
#define REFS_DIR      "refs"
#define NOTES_DIR     "notes"
#define INDEX_FILE    "index"


/* Prototypes of system-dependent functions. */
void ntx_editor(char *file);
void ntx_homedir(char *sub, ...);
long int ntx_flen(char *file);

typedef void * n_dir;
n_dir ntx_dopen(char *dir);
char *ntx_dread(n_dir dir);
int ntx_dclose(n_dir dir);


/* Utility/Refactored functions. */
void die(char *fmt, ...)
{
  va_list args;

  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);
  exit(EXIT_FAILURE);
}

/* 'buf' should be SUMMARY_WIDTH + 2 bytes long. */
int ntx_summary(char *file, char *buf)
{
  FILE *f = fopen(file, "r");
  char *temp;

  if(!f) return -1;

  if(!fgets(buf, SUMMARY_WIDTH+2, f)) return -2;
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
    if((blen - bpos - 1) < BUFFER_MAX) { /* The extra space is for the \0. */
      blen += BUFFER_MAX;
      if(!(bbuf = realloc(bbuf, blen))) return NULL;
    }
  }
  bbuf[bpos] = '\0'; /* NULL-terminate the input. */
  gzclose(f);
  return bbuf;
}

char *ntx_tagstolist(char *id, char **tags)
{
  char *list, *pos, **cur;
  unsigned int len = 7; /* ID + tab + newline + NULL. */

  /* Precompute the length of the tags. */
  for(cur = tags; *cur; cur++) len += strlen(*cur) + 1;
  if(!(list = malloc(len))) return NULL;

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

/* Open the file, read the whole thing in a line at a time,
 * replacing the line beginning with the 4-digit hex 'id' with
 * the line 'fix'.
 * Because we don't know the max line length as we do when
 * index files are guaranteed, we need to parse after we load.
 */
int ntx_replace(char *file, char *id, char *fix)
{
  char *buf;
  char *ptr, *end;
  unsigned int written;
  gzFile *f;

  if(!(buf = ntx_buffer(file))) return -1;
  if(!(f = gzopen(file, "w"))) return -2;

  /* Parse the buffer contents to find the given position. */
  for(ptr = buf; *ptr; ptr = end+1) {
    end = strchr(ptr, '\n');
    if(strncmp(id, ptr, 4) == 0) {
      if(fix) gzputs(f, fix);
    } else {
      if(end) gzwrite(f, ptr, end-ptr);
      else gzputs(f, ptr);
    }
    if(!end) break;
  }
  written = gztell(f);
  gzclose(f);
  free(buf);

  if(written == 0) remove(file); /* Remove the file if it is empty. */

  return 1;
}

char *ntx_find(char *file, char *id)
{
  char *buf;
  char *ptr, *end;

  if(!(buf = ntx_buffer(file))) return NULL;

  /* Parse the buffer contents to find the given position. */
  for(ptr = buf; *ptr; ptr = end + 1) {
    end = strchr(ptr, '\n');
    if(strncmp(id, ptr, 4) == 0) {
      *end = '\0';
      ptr = strdup(ptr);
      free(buf);
      return ptr;
    }
    if(!end) break; /* This isn't normal, and shouldn't occur. */
  }
  free(buf);

  return NULL;
}

int ntx_append(char *file, char *str)
{
  gzFile *f = gzopen(file, "a");

  if(!f) return -1;
  if(gzputs(f, str) == -1) return -2;
  gzclose(f);

  return 1;
}


/* Front-end functions, user interaction. */
int ntx_add(char **tags)
{
  char file[FILE_MAX], note[SUMMARY_WIDTH + PADDING_WIDTH];
  char **ptr, *tmp;
  unsigned int num;
  int ret;

  srand(time(NULL));
  num = rand() & 0xffff;

  while(1) {
    FILE *nout;

    if(!snprintf(file, FILE_MAX, NOTES_DIR"/%04x", num)) return -1;
    if(!(nout = fopen(file, "r"))) break;
    fclose(nout);
    num = (num + 1) & 0xffff;
  }

  /* Fire up the editor to create the note. */
  ntx_editor(file);

  /* Get the summary line, and write it in abbreviated form to each index. */
  if((ret = ntx_summary(file, note+5)) < 1) {
    puts("No new note was recorded.");
    if(ret == -2) remove(file);
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
  if(!snprintf(file, FILE_MAX, REFS_DIR"/%.2s", note)) return -1;

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
  if(ntx_summary(file, head+5) < 1) die("No such note: %s\n", id);
  ntx_editor(file);

  /* See if the header has changed; If so, rewrite the headers. */
  /* XXX: If we can't reread the file, do we need to take action? */
  if(ntx_summary(file, note+5) < 1) die("Unable to read %s\n", id);
  if(strcmp(head+5, note+5)) {
    char *tags, *cur, *ptr;

    /* Fill in the identification information. */
    strncpy(note, id, 4);
    note[4] = '\t';

    if(!snprintf(file, FILE_MAX, REFS_DIR"/%.2s", id)) return -1;
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

    /* Update the index file. */
    if(ntx_replace(INDEX_FILE, id, note) < 1) return -1;
  }

  /* Dump the summary to STDOUT as confirmation that everything went well. */
  fputs(note, stdout);

  return 0;
}


/* Helper functions for calculating multi-tag intersection. */

struct fstats { /* Structure for sorting files by size. */
  char *path;
  unsigned int size;
};

int ntx_sortstat(const void *a, const void *b)
{
  return ((struct fstats*)a)->size - ((struct fstats*)b)->size;
}

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

int ntx_list(char **tags, unsigned int tagc)
{
  char line[SUMMARY_WIDTH + PADDING_WIDTH];
  gzFile *f;

  /* Too many tags for proper ref-counting, or even sane evaluation. */
  if(tagc > 127) die("Too many (more than 127) tags.");

  if(tagc == 0) { /* No tags specified, open the index. */
    f = gzopen(INDEX_FILE, "r");
    if(!f) return 0;

    /* Duplicate each line from the index to STDOUT. */
    while(gzgets(f, line, SUMMARY_WIDTH + PADDING_WIDTH)) fputs(line, stdout);
  } else if(tagc == 1) { /* No need to calculate the intersection. */
    char name[FILE_MAX];

    if(!snprintf(name, FILE_MAX, TAGS_DIR"/%s", *tags)) return -1;

    f = gzopen(name, "r");
    if(!f) die("No such tag: %s\n", tags[0]);

    /* Duplicate each line from the index to STDOUT. */
    while(gzgets(f, line, SUMMARY_WIDTH + PADDING_WIDTH)) fputs(line, stdout);
  } else { /* Calculate the union of the sets from the tag files. */
    char *name;
    hash_t *table;
    struct fstats *files = malloc(sizeof(struct fstats) * tagc);
    unsigned int i, len, exists;
    long int size;

    /* Sort the files; We'll likely be best starting with the smallest. */
    /* Stat and load the files into the buffers for sorting. */
    for(i = 0; i < tagc; i++) {
      len = 6 + strlen(tags[i]);
      name = malloc(len);
      if(!snprintf(name, len, TAGS_DIR"/%s", tags[i])) return -1;

      if((size = ntx_flen(name)) == -1) die("No such tag: %s\n", tags[i]);
      files[i].path = name;
      files[i].size = size;
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

  return 0;
}

int ntx_put(char *id)
{
  FILE *f;
  char file[FILE_MAX];
  char buffer[BUFFER_MAX];

  if(!snprintf(file, FILE_MAX, NOTES_DIR"/%s", id)) return -1;
  if(!(f = fopen(file, "r"))) die("No such note: %s\n", id);

  /* Duplicate each line from the note to STDOUT. */
  while(fgets(buffer, BUFFER_MAX, f)) fputs(buffer, stdout);
  return 0;
}

int ntx_del(char *id)
{
  char file[FILE_MAX], *buf, *ptr, *cur;

  if(!snprintf(file, FILE_MAX, REFS_DIR"/%.2s", id)) return -1;

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
  if(!snprintf(file, FILE_MAX, REFS_DIR"/%.2s", id)) return -1;
  if(ntx_replace(file, id, NULL) < 1) return -1;

  /* Remove the note itself from NOTES_DIR. */
  if(!snprintf(file, FILE_MAX, NOTES_DIR"/%s", id)) return -1;
  if(remove(file) != 0) return -1;

  free(buf);
  return 0;
}

int ntx_tags(char *id)
{
  if(!id) { /* List all tags in the database. */
    char *name;
    n_dir dir = ntx_dopen(TAGS_DIR);

    if(!dir) return -1;
    while((name = ntx_dread(dir))) if(name[0] != '.') puts(name);

    ntx_dclose(dir);
  } else { /* List all tags of a note. */
    char file[FILE_MAX], *buf, *ptr, *cur;

    if(!snprintf(file, FILE_MAX, REFS_DIR"/%.2s", id)) return -1;
    if(!(buf = ntx_find(file, id))) die("No such note: %s\n", id);

    cur = buf + 5; /* Skip the ID, plus the tab. */
    while((ptr = strchr(cur, ';'))) { /* Write out each of the tags. */
      *ptr = '\0';
      puts(cur);
      cur = ptr + 1;
    }

    free(buf);
  }

  return 0;
}

int ntx_retag(char *id, char **tags)
{
  char file[FILE_MAX], desc[SUMMARY_WIDTH + PADDING_WIDTH];
  char *tmp, *next, *orig, **cur;

  /* Get the summary in case we need to write it. */
  if(!snprintf(file, FILE_MAX, NOTES_DIR"/%s", id)) return -1;
  if(ntx_summary(file, desc+5) < 1) die("No such note: %s\n", id);

  /* Fill in the identification information. */
  strncpy(desc, id, 4);
  desc[4] = '\t';

  /* Read in the original listing of tags for this file. */
  if(!snprintf(file, FILE_MAX, REFS_DIR"/%.2s", id)) return -1;
  if(!(orig = ntx_find(file, id))) die("No such note: %s\n", id);

  /* Add any tags which don't yet exist. */
  for(cur = tags; *cur; cur++) {
    if(!strstr(orig, *cur)) { /* Add the tag to the file. */
      if(!snprintf(file, FILE_MAX, TAGS_DIR"/%s", *cur)) return -1;
      if(!ntx_append(file, desc)) return -1;
    }
  }
   
  /* Remove any tags which don't exist any more. */
  for(tmp = orig+5; (next = strchr(tmp, ';')); tmp = next + 1) {
    *next = '\0';
    for(cur = tags; *cur; cur++)
      if(strcmp(*cur, tmp) == 0) break;

    if(!*cur) { /* Remove any tags we didn't find. */
      if(!snprintf(file, FILE_MAX, TAGS_DIR"/%s", tmp)) return -1;
      if(ntx_replace(file, id, NULL) < 1) return -1;
    }
  }

  /* Update the backreference with the new set of tags. */
  tmp = ntx_tagstolist(id, tags);
  if(!snprintf(file, FILE_MAX, REFS_DIR"/%.2s", id)) return -1;
  if(ntx_replace(file, id, tmp) < 1) {
    free(tmp);
    return -1;
  }
  free(tmp);

  return 0;
}

void ntx_usage(int retcode)
{
  puts("Usage:\tntx [mode] [arguments] ..\n");
  puts("Modes:\tadd  [tags ..]\t\tAdd a note to tags, with $EDITOR.");
  puts("\tedit [hex]\t\tEdit the note identified by the ID hex.");
  puts("\tlist <tags ..>\t\tList the notes in the intersection of tags.");
  puts("\tput  [hex]\t\tPrint the note with the ID 'hex' to STDOUT.");
  puts("\trm   [hex]\t\tDelete the note identified by the ID hex.");
  puts("\ttag  <hex>\t\tPrint a list of the tags in the database.\n");
  puts("\ttag  [hex] [tags ..]\t\tRe-tag hex with the list tags.");
  puts("\t-h or --help\t\tPrint this information.\n");

  /* XXX: Include a brief description of each mode here. */

  puts("Input file format:");
  puts("\tntx assumes that a short first line may be used as a summary of");
  puts("\tthe note in question. No further assumptions are made about the");
  puts("\ttype, format, or size of the file.\n");

  puts("Notes on the output of 'ntx list':");
  puts("\t'ntx list' outputs a four-byte hexidecimal ID, followed by a tab,");
  printf("\tand then a brief summary comprised of the first %d bytes of the\n", SUMMARY_WIDTH);
  puts("\tfirst line of the saved note.");

  exit(retcode);
}

int main(int argc, char **argv)
{
  /* Very few arguments, so we use a hand-written parser. */
  if(argc < 2) ntx_usage(EXIT_FAILURE);

  if(!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h"))
    ntx_usage(EXIT_SUCCESS);

  /* Change to/create our root directory. */
  ntx_homedir(TAGS_DIR, REFS_DIR, NOTES_DIR, NULL);

  if(!strcmp(argv[1], "add") && argc >= 3) {
    if(ntx_add(argv+2) != 0) die("Unknown execution failure.\n");
  } else if(!strcmp(argv[1], "edit") && argc == 3) {
    if(ntx_edit(argv[2]) != 0) die("Unknown execution failure.\n");
  } else if(!strcmp(argv[1], "list") && argc >= 2) {
    if(ntx_list(argv+2, argc - 2) != 0) die("Unknown execution failure.\n");
  } else if(!strcmp(argv[1], "put") && argc == 3) {
    if(ntx_put(argv[2]) != 0) die("Unknown execution failure.\n");
  } else if(!strcmp(argv[1], "rm") && argc == 3) {
    if(ntx_del(argv[2]) != 0) die("Unknown execution failure.\n");
  } else if(!strcmp(argv[1], "tag") && argc > 3) {
    if(ntx_retag(argv[2], argv+3) != 0) die("Unknown execution failure.\n");
  } else if(!strcmp(argv[1], "tag") && (argc == 2 || argc == 3)) {
    if(ntx_tags(argv[2]) != 0) die("Unknown execution failure.\n");
  } else ntx_usage(EXIT_FAILURE);

  return EXIT_SUCCESS;
}
