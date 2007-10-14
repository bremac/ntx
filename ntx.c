#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <zlib.h>
#include "hash_table.h"

#define FILE_MAX      1024
#define BUFFER_MAX    1024
#define SUMMARY_WIDTH 58
#define PADDING_WIDTH 6

char *home; /* Keep us from repeatedly calling getenv. */

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

  if(!editor) {
    puts("ERROR - The environment variable EDITOR is unset, cannot continue.");
    exit(EXIT_FAILURE);
  }

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
  fgets(note+4, SUMMARY_WIDTH+2, nout);
  fclose(nout);

  /* Get the summary line, and write it in abbreviated form to each index. */
  /* Set a null at the maximum position to ensure we don't overdo it. */
  temp = strchr(note+4, '\n');
  if(temp) temp[1] = '\0';
  else {
    unsigned int end = strlen(note+4);

    temp = note+4;
    temp[end-1] = '\n';
    temp[end-2] = '.';
    temp[end-3] = '.';
    temp[end-4] = '.';
  }

  /* Fill in the identification information. */
  note[0] = file[6];
  note[1] = file[7];
  note[2] = file[8];
  note[3] = '\t';

  for(; *tags != NULL; tags++) {
    if(!snprintf(file, FILE_MAX, "tags/%s", *tags)) return -1;
    f = gzopen(file, "a");
    gzputs(f, note);
    gzclose(f);
  }

  f = gzopen("index", "a");
  gzputs(f, note);
  gzclose(f);

  return 0;
}

int ntx_edit(char *id)
{
  FILE *f;
  char file[FILE_MAX];
  char *editor = getenv("EDITOR");

  if(!editor) {
    puts("ERROR - The environment variable EDITOR is unset, cannot continue.");
    exit(EXIT_FAILURE);
  }

  if(!snprintf(file, FILE_MAX, "notes/%s", id)) return -1;

  /* Check that the note exists first. */
  if(!(f = fopen(file, "r"))) {
    printf("ERROR - Invalid ID %s\n", id);
    exit(EXIT_FAILURE);
  }
  fclose(f);

  execlp(editor, editor, file, (char*)NULL);
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
  return hasht_hash((char*)v, strlen((char*)v), 0);
}

hash_t *ntx_hashload(char *file)
{
  gzFile *f = gzopen(file, "r");
  char line[SUMMARY_WIDTH + PADDING_WIDTH];
  hash_t *table;

  if(!f) {
    printf("ERROR - Invalid index %s\n", file);
    exit(EXIT_FAILURE);
  }

  table = hasht_init(120, free, hash_line, hash_line, 
                    (int(*)(void*, void*))strcmp,
                    (int(*)(void*, void*))strcmp);

  while(gzgets(f, line, SUMMARY_WIDTH + PADDING_WIDTH))
    hasht_add(table, strdup(line));

  return table;
}

/* We use compression as much as possible to minimize loading times. */
int ntx_list(char **tags, int tagc)
{
  char line[SUMMARY_WIDTH + PADDING_WIDTH];
  gzFile *f;

  if(tagc == 0) { /* No tags specified, open the index. */
    f = gzopen("index", "r");
    if(!f) return 0;

    /* Duplicate each line from the index to STDOUT. */
    while(gzgets(f, line, SUMMARY_WIDTH + PADDING_WIDTH)) fputs(line, stdout);
  } else { /* Calculate the union of the sets from the tag files. */
    char *name;
    hash_t **hashes = calloc(tagc-1, sizeof(hash_t *));
    struct stat temp;
    struct fstats *files = malloc(sizeof(struct fstats) * tagc);
    unsigned int i, len, exists;

    /* Sort the files; We'll likely be best starting with the smallest. */
    /* Stat and load the files into the buffers for sorting. */
    for(i = 0; i < tagc; i++) {
      name = malloc(6 + strlen(tags[i]));
      strcpy(name, "tags/");
      strcat(name, tags[i]);
      if(stat(name, &temp) != 0) {
        printf("ERROR - No such tag: %s\n", tags[i]);
        exit(EXIT_FAILURE);
      }
      files[i].path = name;
      files[i].size = temp.st_size;
    }

    qsort(files, tagc, sizeof(struct fstats), ntx_sortstat);

    /* Now, load the first, and check it in each hash. */
    f = gzopen(files[0].path, "r");
    while(gzgets(f, line, SUMMARY_WIDTH + PADDING_WIDTH)) {
      exists = 1;
      for(i = 0; i < tagc-1; i++) {
        /* Load the file into the hash, if necessary. */
        if(hashes[i] == NULL) hashes[i] = ntx_hashload(files[i+1].path);
        if(hasht_get(hashes[i], line) == NULL) {
          exists = 0;
          break;
        }
      }
      if(exists) fputs(line, stdout);
    }

    /* Clean up the hashes and fstats structures. */
    for(i = 0; i < tagc; i++) {
      if(i < tagc-1) hasht_free(hashes[i]);
      free(files[i].path);
    }
    free(files);
    free(hashes);
  }

  fputc('\n', stdout);
  gzclose(f);

  return 0;
}

int ntx_put(char *id)
{
  FILE *f;
  char file[FILE_MAX];
  char buffer[BUFFER_MAX];

  if(!snprintf(file, FILE_MAX, "notes/%s", id)) return -1;

  if(!(f = fopen(file, "r"))) {
    printf("ERROR - Invalid ID %s\n", id);
    exit(EXIT_FAILURE);
  }

  /* Duplicate each line from the note to STDOUT. */
  while(fgets(buffer, BUFFER_MAX, f)) fputs(buffer, stdout);
  return 0;
}

int ntx_del(char *id)
{
  return 0;
}

void ntx_usage(int retcode)
{
  puts("Usage:\tntx [mode] [arguments] ..\n");
  puts("Modes:\t-a [tags]\t\tAdd a note from STDIN to tags.");
  puts("\t-e [hex]\t\tEdit the note identified by the ID hex.");
  puts("\t-p [hex]\t\tPrint the note identified by the ID hex to STDOUT.");
  puts("\t-l <tags>\t\tList the set of notes from the union of tags.");
  puts("\t-d [hex]\t\tDelete the node identified by the ID hex.\n");
  puts("\t-h or --help\t\tPrint this information.\n");

  puts("Input file format:");
  puts("\tntx assumes that a short first line may be used as a summary of");
  puts("\tthe note in question. No further assumptions are made about the");
  puts("\ttype, format, or size of the file.\n");

  puts("Notes on the output of 'ntx -l':");
  puts("\t'ntx -l' outputs a hexidecimal ID, followed by a tab, and then a");
  printf("\tbrief summary comprised of the first %d bytes of the first line\n", SUMMARY_WIDTH);
  puts("\tof the saved note. This is currently not updated if a note changes.\n");

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
  if(!snprintf(file, FILE_MAX, "%s/.ntx", getenv("HOME"))) {
    printf("ERROR - Unknown execution failure.\n");
    exit(EXIT_FAILURE);
  }

  if(chdir(file) == -1) {
    mkdir(file, S_IRWXU);
    if(chdir(file) == -1) {
      printf("ERROR - Unknown execution failure.\n");
      exit(EXIT_FAILURE);
    }
    mkdir("tags", S_IRWXU);
    mkdir("notes", S_IRWXU);
  }

  if(argv[1][1] == 'a' && argc >= 3) {
    if(ntx_add(argv+2) != 0) {
      printf("ERROR - Unknown execution failure.\n");
      exit(EXIT_FAILURE);
    }
  } else if(argv[1][1] == 'e' && argc == 3) {
    if(ntx_edit(argv[2]) != 0) {
      printf("ERROR - Unknown execution failure.\n");
      exit(EXIT_FAILURE);
    }
  } else if(argv[1][1] == 'l' && argc >= 2) {
    if(ntx_list(argv+2, argc - 2) != 0) {
      printf("ERROR - Unknown execution failure.\n");
      exit(EXIT_FAILURE);
    }
  } else if(argv[1][1] == 'p' && argc == 3) {
    if(ntx_put(argv[2]) != 0) {
      printf("ERROR - Unknown execution failure.\n");
      exit(EXIT_FAILURE);
    }
  } else if(argv[1][1] == 'd' && argc == 3) {
    printf("ERROR - Feature not yet implemented.\n");
  } else ntx_usage(EXIT_FAILURE);

  return EXIT_SUCCESS;
}
