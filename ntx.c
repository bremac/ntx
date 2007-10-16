#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <zlib.h>
#include "hash_table.h"

#define FILE_MAX      1024
#define BUFFER_MAX    8192
#define SUMMARY_WIDTH 58
#define PADDING_WIDTH 7

void die(char *fmt, ...)
{
  va_list args;

  va_start(args, fmt);
  printf(fmt, args);
  va_end(args);
  exit(EXIT_FAILURE);
}

/* XXX: Keep us from creating more than 1024 notes. */
int ntx_add(char **tags)
{
  FILE *nout;
  gzFile *f;
  char file[FILE_MAX], note[SUMMARY_WIDTH + PADDING_WIDTH], *temp;
  char *editor = getenv("EDITOR");
  pid_t child;
  unsigned int hash;

  srand(time(NULL));
  hash = rand() & 0xffff;

  if(!editor) die("ERROR - The environment variable EDITOR is unset.");

  while(1) {
    if(!snprintf(file, FILE_MAX, "notes/%04x", hash))
       return -1;
    if(!(nout = fopen(file, "r"))) break;
    fclose(nout);
    hash = (hash + 1) & 0xffff;
  }

  /* Fire up EDITOR to create the note. */
  child = fork();

  if(child == 0) execlp(editor, editor, file, (char*)NULL);
  else waitpid(child, NULL, 0);

  /* Ensure that it was written, or die. */
  if(!(nout = fopen(file, "r"))) {
    puts("No new note was recorded.");
    return 0;
  }
  fgets(note+5, SUMMARY_WIDTH+2, nout);
  fclose(nout);

  /* Get the summary line, and write it in abbreviated form to each index. */
  /* Set a null at the maximum position to ensure we don't overdo it. */
  temp = strchr(note+5, '\n');
  if(temp) temp[1] = '\0';
  else {
    unsigned int end = strlen(note+5);

    temp = note+5;
    temp[end-1] = '\n';
    temp[end-2] = '.';
    temp[end-3] = '.';
    temp[end-4] = '.';
  }

  /* Fill in the identification information. */
  note[0] = file[6];
  note[1] = file[7];
  note[2] = file[8];
  note[3] = file[9];
  note[4] = '\t';

  for(; *tags != NULL; tags++) {
    if(!snprintf(file, FILE_MAX, "tags/%s", *tags)) return -1;
    f = gzopen(file, "a");
    gzputs(f, note);
    gzclose(f);
  }

  /* XXX: Write the tags to the composite tags database. */

  f = gzopen("index", "a");
  gzputs(f, note);
  gzclose(f);

  /* Dump the summary to STDOUT as confirmation that everything went well. */
  fputs(note, stdout);

  return 0;
}

int ntx_edit(char *id)
{
  FILE *f;
  char file[FILE_MAX];
  char *editor = getenv("EDITOR");

  if(!editor) die("ERROR - The environment variable EDITOR is unset.");
  if(!snprintf(file, FILE_MAX, "notes/%s", id)) return -1;

  /* Check that the note exists first. */
  if(!(f = fopen(file, "r"))) die("ERROR - Invalid ID %s\n", id);
  fclose(f);

  execlp(editor, editor, file, (char*)NULL);

  /* XXX: Re-read file, compare headers, and rewrite if necessary. */

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
    f = gzopen("index", "r");
    if(!f) return 0;

    /* Duplicate each line from the index to STDOUT. */
    while(gzgets(f, line, SUMMARY_WIDTH + PADDING_WIDTH)) fputs(line, stdout);
  } else if(tagc == 1) { /* No need to calculate the intersection. */
    char name[FILE_MAX];

    strcpy(name, "tags/");
    strncat(name, tags[0], FILE_MAX - strlen(name));

    f = gzopen(name, "r");
    if(!f) die("ERROR - No such tag: %s\n", tags[0]);

    /* Duplicate each line from the index to STDOUT. */
    while(gzgets(f, line, SUMMARY_WIDTH + PADDING_WIDTH)) fputs(line, stdout);
  } else { /* Calculate the union of the sets from the tag files. */
    char *name;
    hash_t *table;
    struct stat temp;
    struct fstats *files = malloc(sizeof(struct fstats) * tagc);
    unsigned int i, exists;

    /* Sort the files; We'll likely be best starting with the smallest. */
    /* Stat and load the files into the buffers for sorting. */
    for(i = 0; i < tagc; i++) {
      name = malloc(6 + strlen(tags[i]));
      strcpy(name, "tags/");
      strcat(name, tags[i]);
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

  if(!snprintf(file, FILE_MAX, "notes/%s", id)) return -1;
  if(!(f = fopen(file, "r"))) die("ERROR - Invalid ID %s\n", id);

  /* Duplicate each line from the note to STDOUT. */
  while(fgets(buffer, BUFFER_MAX, f)) fputs(buffer, stdout);
  return 0;
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

  return 0;
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

int ntx_del(char *id)
{
  char file[FILE_MAX], *buf, *ptr, *cur;
  unsigned int len;

  if(!strcpy(file, "tagged/")) return -1;
  len = strlen(file);
  file[len]   = id[0];
  file[len+1] = id[1];
  file[len+2] = '\0';

  /* Find the line describing the tags of the given ID. */
  buf = ntx_find(file, id);

  cur = buf + 5; /* Skip the ID, plus the tab. */
  while((ptr = strchr(cur, ';'))) {
    *ptr = '\0';
    /* Delete the tags - O(n) search through the affected indices. */
    /* XXX: Error checking. */
    if(!strcpy(file, "tags/")) return -1;
    if(!strncat(file, cur, FILE_MAX-strlen(file))) return -1;
    if(!ntx_replace(file, id, NULL)) return -1;
    cur = ptr + 1;
  }

  /* Remove it from the index. */
  if(!ntx_replace("index", id, NULL)) return -1;

  /* Remove the backreference. */
  if(!strcpy(file, "tagged/")) return -1;
  len = strlen(file);
  file[len]   = id[0];
  file[len+1] = id[1];
  file[len+2] = '\0';
  if(!ntx_replace(file, id, NULL)) return -1;

  /* Remove the note itself from notes/ */
  if(!strcpy(file, "tagged/")) return -1;
  if(!strncat(file, id, FILE_MAX-strlen(file))) return -1;
  if(unlink(file) != 0) return -1;

  free(buf);
  return 0;
}

void ntx_usage(int retcode)
{
  puts("Usage:\tntx [mode] [arguments] ..\n");
  puts("Modes:\t-a [tags]\t\tAdd a note from STDIN to tags.");
  puts("\t-e [hex]\t\tEdit the note identified by the ID hex.");
  puts("\t-p [hex]\t\tPrint the note with the ID 'hex' to STDOUT.");
  puts("\t-l <tags>\t\tList the notes in the intersection of tags.");
  puts("\t-d [hex]\t\tDelete the node identified by the ID hex.\n");
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
  if(!snprintf(file, FILE_MAX, "%s/.ntx", getenv("HOME")))
    die("ERROR - Unknown execution failure.\n");

  if(chdir(file) == -1) {
    mkdir(file, S_IRWXU);
    if(chdir(file) == -1) die("ERROR - Unknown execution failure.\n");
    mkdir("tags", S_IRWXU);
    mkdir("notes", S_IRWXU);
  }

  if(argv[1][1] == 'a' && argc >= 3) {
    if(ntx_add(argv+2) != 0) die("ERROR - Unknown execution failure.\n");
  } else if(argv[1][1] == 'e' && argc == 3) {
    if(ntx_edit(argv[2]) != 0) die("ERROR - Unknown execution failure.\n");
  } else if(argv[1][1] == 'l' && argc >= 2) {
    if(ntx_list(argv+2, argc - 2) != 0)
      die("ERROR - Unknown execution failure.\n");
  } else if(argv[1][1] == 'p' && argc == 3) {
    if(ntx_put(argv[2]) != 0) die("ERROR - Unknown execution failure.\n");
  } else if(argv[1][1] == 'd' && argc == 3) {
    printf("ERROR - Feature not yet implemented.\n");
  } else ntx_usage(EXIT_FAILURE);

  return EXIT_SUCCESS;
}
