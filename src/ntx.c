#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <zlib.h>
#include <errno.h>
#include "except.h"
#include "exc_io.h"
#include "hash_table.h"

/* Buffer size definitions. */
#define FILE_MAX      (FILENAME_MAX+1)
#define BUFFER_MAX     8192

/* ID: Four hexidecimal digits. */
#define ID_LENGTH      4

/* Single character seperator. */
#define SEP_LENGTH     1

/* Maximum summary length to read. */
#define SUMMARY_LENGTH 58 

/* "\n\0" line terminator. */
#define PADDING_LENGTH 2    

#define SUMREC_LENGTH  (ID_LENGTH + SEP_LENGTH + SUMMARY_LENGTH + PADDING_LENGTH)
#define SUMBASE_LENGTH (ID_LENGTH + SEP_LENGTH + PADDING_LENGTH)
#define SUMMARY_OFFSET (ID_LENGTH + SEP_LENGTH)


/* Default (_one character_) separators.            *
 * \n the a hard-coded record separator, due to the *
 * fgets style of line handling.                    */
const char  ID_SEP    = '\t';
const char *FIELD_SEP = ";";


/* Builtin Subdirectories. */
#define TAGS_DIR   "tags"
#define REFS_DIR   "refs"
#define NOTES_DIR  "notes"
#define INDEX_FILE "index"


/* Prototypes of system-dependent functions. */
void ntx_editor(char *file);
void ntx_homedir(char *sub, ...);
long int ntx_flen(char *file);

typedef void * n_dir;
n_dir ntx_dopen(char *dir);
char *ntx_dread(n_dir dir);
void ntx_dclose(n_dir dir);


void die(const char *fmt, ...)
{
  va_list args;

  va_start(args, fmt);
  fputs("ERROR: ", stderr);
  vfprintf(stderr, fmt, args);
  va_end(args);
  fputs("\n", stderr);

  exit(EXIT_FAILURE);
}

char *strrtok(char *string, char **state, const char *delim)
{
  char *token, *end;

  if(string == NULL && (string = *state) == NULL)
    return NULL;

  if(*(token = string + strspn(string, delim)) == '\0') {
    *state = NULL;
    return NULL;
  }

  if((end = strpbrk(token, delim)) == NULL) {
    *state = NULL;
  } else {
    *end   = '\0';
    *state = end + 1;
  }

  return token;
}

char **strtokens(char *str, const char *delim)
{
  unsigned int curtoken = 0, maxtokens = 8;
  char **tokens = alloc(maxtokens * sizeof(char *));
  char *token, *state = NULL;

  if(strlen(delim) == 0) {
    if(tokens) release(tokens);
    return NULL;
  }

  /* Collect tokens from strrtok. */
  for(token = strrtok(str, &state, delim);
      token != NULL;
      token = strrtok(NULL, &state, delim)) {
    if(curtoken == maxtokens)
      tokens = ralloc(tokens, (maxtokens *= 2));
    tokens[curtoken++] = token;
  }

  /* Terminate with a NULL token. */
  if(curtoken+1 == maxtokens)
    tokens = ralloc(tokens, maxtokens+1);
  tokens[curtoken] = NULL;

  return tokens;
}

char **stratokens(const char *str, char **buffer, const char *delim)
{
  *buffer = strdupe((char*)str);
  return strtokens(*buffer, delim);
}

/* 'buf' should be SUMMARY_LENGTH + PADDING_LENGTH bytes long. */
void ntx_summary(char *file, char *buf)
{
  FILE *f = raw_open(file, "r");
  char *temp;

  temp = raw_getl(f, buf, SUMMARY_LENGTH + PADDING_LENGTH);
  release(f);
  if(!temp || strlen(temp) == 0) throw(E_INVAL, file);

  /* Format the string into SUMMARY_LENGTH bytes. */
  if((temp = strchr(buf, '\n'))) temp[1] = '\0';
  else {
    unsigned int end = strlen(buf);

    end = (end >= 4) ? end : 4; /* Prevent illogical accesses. */
    strcpy(buf+end-4, "...\n"); /* Change the last four bytes. */
  }
}

char *ntx_buffer(char *file)
{
  unsigned int blen = BUFFER_MAX, bpos = 0;
  unsigned int len;
  gzFile *f = gzf_open(file, "r");
  char *bbuf = alloc(blen);

  /* Read the entire file into the buffer. */
  while((len = gzf_read(f, bbuf + bpos, BUFFER_MAX))) {
    bpos += len;
    if((blen - bpos - 1) < BUFFER_MAX) { /* The extra space is for the \0. */
      blen += BUFFER_MAX;
      bbuf = ralloc(bbuf, blen);
    }
  }
  bbuf[bpos] = '\0'; /* NULL-terminate the input. */
  release(f);
  return bbuf;
}

char *ntx_tagstolist(char *id, char **tags)
{
  char *list, *pos, **cur;
  unsigned int len = SUMBASE_LENGTH; /* Allocate from the base summary size. */

  /* Precompute the length of the tags. */
  for(cur = tags; *cur; cur++) len += strlen(*cur) + 1;
  list = alloc(len);

  strncpy(list, id, 4);
  list[4] = ID_SEP;

  pos = list + 5;

  for(cur = tags; *cur; cur++) {
    strcpy(pos, *cur);
    len = strlen(*cur);
    pos[len] = *FIELD_SEP;
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
  char *ptr, *end;
  unsigned int written, found = 0;
  char *buf = ntx_buffer(file);
  gzFile *f = gzf_open(file, "w");

  /* Parse the buffer contents to find the given position. */
  for(ptr = buf; *ptr; ptr = end+1) {
    if(!(end = strchr(ptr, '\n'))) throw(E_INVAL, file);
    if(strncmp(id, ptr, 4) == 0) {
      if(fix) gzf_putl(f, fix);
      found = 1;
    } else gzf_write(f, ptr, end-ptr+1);
  }
  written = gztell(f);
  release(f);
  release(buf);

  if(written == 0) remove(file); /* Remove the file if it is empty. */
  return found;
}

char *ntx_find(char *file, char *id)
{
  char *buf = ntx_buffer(file);
  char *ptr, *end;

  /* Parse the buffer contents to find the given position. */
  for(ptr = buf; *ptr; ptr = end + 1) {
    if(!(end = strchr(ptr, '\n'))) throw(E_INVAL, file);
    if(strncmp(id, ptr, 4) == 0) {
      *end = '\0';
      ptr = strdupe(ptr);
      release(buf);
      return ptr;
    }
  }

  release(buf);
  return NULL;
}

void ntx_append(char *file, char *str)
{
  gzFile *f = gzf_open(file, "a");
  gzf_putl(f, str);
  release(f);
}

/* Front-end functions, user interaction. */
void ntx_add(char **tags)
{
  char file[FILE_MAX], note[SUMREC_LENGTH];
  char **ptr, *tmp;
  unsigned int num;
  exception_t exc;

  srand(time(NULL));
  num = rand() & 0xffff;

  while(1) {
    FILE *nout = NULL;

    seprintf(file, FILE_MAX, NOTES_DIR"/%04x", num);
    try nout = raw_open(file, "r");
    catch(exc) if(exc.type == E_FACCESS) break;
    release(nout);
    num = (num + 1) & 0xffff;
  }

  /* Fire up the editor to create the note. */
  ntx_editor(file);

  /* Get the summary line, and write it in abbreviated form to each index. */
  try ntx_summary(file, note + SUMMARY_OFFSET);
  catch(exc) {
    if((exc.type == E_FACCESS || exc.type == E_INVAL) &&
       strcmp(file, exc.value) == 0) {
      fputs("No new note was recorded.\n", stderr);
      remove(file);
      exit(EXIT_SUCCESS);
    } else throw(exc.type, exc.value);
  }

  /* Fill in the identification information. */
  strncpy(note, file + strlen(NOTES_DIR) + 1, ID_LENGTH);
  note[4] = ID_SEP;

  for(ptr = tags; *ptr != NULL; ptr++) {
    seprintf(file, FILE_MAX, TAGS_DIR"/%s", *ptr);
    ntx_append(file, note);
  }

  /* Add the new note to the base index. */
  ntx_append(INDEX_FILE, note);

  /* Append the tags to the backreference file. */
  seprintf(file, FILE_MAX, REFS_DIR"/%.2s", note);
  tmp = ntx_tagstolist(note, tags);
  ntx_append(file, tmp);
  release(tmp);

  /* Dump the summary to STDOUT as confirmation that everything went well. */
  fputs(note, stdout);
}

void ntx_edit(char *id)
{
  char file[FILE_MAX], head[SUMMARY_LENGTH + PADDING_LENGTH];
  char note[SUMREC_LENGTH];
  char *state = NULL;

  seprintf(file, FILE_MAX, NOTES_DIR"/%s", id);

  /* Check that the note exists first, then edit it. */
  ntx_summary(file, head);
  ntx_editor(file);

  /* See if the header has changed; If so, rewrite the headers. */
  /* XXX: If we can't reread the file, do we need to take action? */
  ntx_summary(file, note + SUMMARY_OFFSET);
  if(strcmp(head, note + SUMMARY_OFFSET)) {
    char *tags, *cur;

    /* Fill in the identification information. */
    strncpy(note, id, ID_LENGTH);
    note[ID_LENGTH] = ID_SEP;

    seprintf(file, FILE_MAX, REFS_DIR"/%.2s", id);
    if(!(tags = ntx_find(file, id)))
      die("Unable to locate note %s in %s.", id, file);

    for(cur = strrtok(tags + SUMMARY_OFFSET, &state, FIELD_SEP);
        cur != NULL;
        cur = strrtok(NULL, &state, FIELD_SEP))
    {
      /* Update the tags - O(n) search through the affected indices. */
      seprintf(file, FILE_MAX, TAGS_DIR"/%s", cur);
      if(ntx_replace(file, id, note) == 0)
        die("Unable to locate note %s in %s.", id, file);
    }
    release(tags);

    /* Update the index file. */
    if(ntx_replace(INDEX_FILE, id, note) == 0)
      die("Unable to locate note %s in %s.", id, file);
  }

  /* Dump the summary to STDOUT as confirmation that everything went well. */
  fputs(note, stdout);
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

/* XXX: hasht_* error checking. */
void ntx_list(char **tags, unsigned int tagc)
{
  char line[SUMREC_LENGTH];
  gzFile *f;

  /* Too many tags for proper ref-counting, or even sane evaluation. */
  if(tagc > 127) die("Too many (more than 127) tags.");

  if(tagc == 0) { /* No tags specified, open the index. */
    f = gzf_open(INDEX_FILE, "r");

    /* Duplicate each line from the index to STDOUT. */
    while(gzf_getl(f, line, SUMREC_LENGTH)) fputs(line, stdout);
    release(f);
  } else if(tagc == 1) { /* No need to calculate the intersection. */
    char name[FILE_MAX];

    seprintf(name, FILE_MAX, TAGS_DIR"/%s", *tags);
    f = gzf_open(name, "r");

    /* Duplicate each line from the index to STDOUT. */
    while(gzgets(f, line, SUMREC_LENGTH)) fputs(line, stdout);
    release(f);
  } else { /* Calculate the union of the sets from the tag files. */
    char *name;
    hash_t *table;
    struct fstats *files = alloc(sizeof(struct fstats) * tagc);
    unsigned int i, len, exists;
    long int size;

    /* Sort the files; We'll likely be best starting with the smallest. */
    /* Stat and load the files into the buffers for sorting. */
    for(i = 0; i < tagc; i++) {
      len = 6 + strlen(tags[i]);
      name = alloc(len);
      seprintf(name, len, TAGS_DIR"/%s", tags[i]);

      if((size = ntx_flen(name)) == -1) die("Unable to open %s.", name);
      files[i].path = name;
      files[i].size = size;
    }

    qsort(files, tagc, sizeof(struct fstats), ntx_sortstat);

    /* Now, load the first, and check it with each hash. */
    f = gzf_open(files[0].path, "r");
    table = hasht_init(120, release, hash_line, hash_val, cmp_line, cmp_val);
    if(!table) return;

    /* Read each line in the index into the hash. */
    while(gzf_getl(f, line, SUMREC_LENGTH)) {
      name = alloc(strlen(line) + 2); /* Save room for the ref and null. */
      name[0] = 1;
      strcpy(name+1, line);
      if((name = hasht_add(table, name))) release(name);
    }
    release(f);
   
    /* Load and check each hash against this one, incrementing found refs. *
     * We will abort if exists == 0, meaning none were found.              */
    for(i = 1; i < tagc; i++) {
      exists = 0;
      f = gzf_open(files[i].path, "r");

      while(gzgets(f, line, SUMREC_LENGTH)) {
        if((name = hasht_get(table, line))) {
          name[0]++; /* Increment the refcount. */
          exists = 1;
        }
      }
      release(f);

      if(exists == 0) 
        die("No notes exist in the intersection of those tags.");
    }

    /* Print all of the tags which had 'tagc' references. */
    while((name = hasht_next(table)))
      if(name[0] == tagc) fputs(name+1, stdout);

    /* Clean up the hash and fstats structures. */
    hasht_free(table);
    release(files);
  }
}

void ntx_put(char *id)
{
  FILE *f;
  char file[FILE_MAX];
  char buffer[BUFFER_MAX];

  seprintf(file, FILE_MAX, NOTES_DIR"/%s", id);
  f = raw_open(file, "r");

  /* Duplicate each line from the note to STDOUT. */
  while(raw_getl(f, buffer, BUFFER_MAX)) fputs(buffer, stdout);
  release(f);
}

void ntx_del(char *id)
{
  char file[FILE_MAX];
  char *buf, *cur;
  char *state = NULL;

  seprintf(file, FILE_MAX, REFS_DIR"/%.2s", id);

  /* Find the line describing the tags of the given ID. */
  if(!(buf = ntx_find(file, id)))
    die("Unable to locate note %s in %s.", id, file);

  for(cur = strrtok(buf + SUMMARY_OFFSET, &state, FIELD_SEP);
      cur != NULL;
      cur = strrtok(NULL, &state, FIELD_SEP)) {
    /* Delete the tags - O(n) search through the affected indices. */
    seprintf(file, FILE_MAX, TAGS_DIR"/%s", cur);
    if(ntx_replace(file, id, NULL) == 0)
      die("Problem removing info for note %s from %s.", id, file);
  }
  release(buf);

  /* Remove it from the index. */
  if(ntx_replace(INDEX_FILE, id, NULL) == 0)
    die("Problem removing info for note %s from index.", id);

  /* Remove the backreference. */
  seprintf(file, FILE_MAX, REFS_DIR"/%.2s", id);
  if(ntx_replace(file, id, NULL) == 0)
    die("Problem removing info for note %s from %s.", id, file);

  /* Remove the note itself from NOTES_DIR. */
  seprintf(file, FILE_MAX, NOTES_DIR"/%s", id);
  if(remove(file) != 0) die("Unable to remove note %s.", id);
}

void ntx_tags(char *id)
{
  if(!id) { /* List all tags in the database. */
    char *name;
    n_dir dir = ntx_dopen(TAGS_DIR);

    if(!dir) die("Unable to read directory %s.", TAGS_DIR);
    while((name = ntx_dread(dir))) if(name[0] != '.') puts(name);

    ntx_dclose(dir);
  } else { /* List all tags of a note. */
    char file[FILE_MAX];
    char *buf, *cur;
    char *state = NULL;

    seprintf(file, FILE_MAX, REFS_DIR"/%.2s", id);
    if(!(buf = ntx_find(file, id)))
      die("Unable to locate note %s in %s.", id, file);

    for(cur = strrtok(buf + SUMMARY_OFFSET, &state, FIELD_SEP);
        cur != NULL;
        cur = strrtok(NULL, &state, FIELD_SEP))
      puts(cur);

    release(buf);
  }
}

void ntx_retag(char *id, char **tags)
{
  char file[FILE_MAX], desc[SUMREC_LENGTH];
  char **ntag, **otag, **otags;
  char *buffer;

  /* Get the summary in case we need to write it. */
  seprintf(file, FILE_MAX, NOTES_DIR"/%s", id); 
  ntx_summary(file, desc + SUMMARY_OFFSET);

  /* Fill in the identification information. */
  strncpy(desc, id, ID_LENGTH);
  desc[ID_LENGTH] = ID_SEP;

  /* Read in the original listing of tags for this file. */
  seprintf(file, FILE_MAX, REFS_DIR"/%.2s", id);
  if(!(buffer = ntx_find(file, id)))
    die("Unable to locate note %s in %s.", id, file);

  /* Tokenize the original line. */
  otags = strtokens(buffer + SUMMARY_OFFSET, FIELD_SEP);

  /* Add any tags which don't yet exist. */
  for(ntag = tags; *ntag != NULL; ntag++) {
    for(otag = otags; *otag != NULL; otag++)
      if(strcmp(*ntag, *otag) == 0)
        break;

    if(!*otag) { /* Add the tag to the file. */
      seprintf(file, FILE_MAX, TAGS_DIR"/%s", *ntag);
      ntx_append(file, desc);
    }
  }

  /* Remove any tags which don't exist any more. */
  for(otag = otags; *otag != NULL; otag++) {
    for(ntag = tags; *ntag != NULL; ntag++)
      if(strcmp(*otag, *ntag) == 0)
        break;

    if(!*ntag) { /* Remove deleted tag. */
      seprintf(file, FILE_MAX, TAGS_DIR"/%s", *otag);
      if(ntx_replace(file, id, NULL) == 0)
        die("Unable to locate note %s in %s.", id, file);
    }
  }

  release(otags);
  release(buffer);

  /* Update the backreference with the new set of tags. */
  buffer = ntx_tagstolist(id, tags);
  seprintf(file, FILE_MAX, REFS_DIR"/%.2s", id);
  if(ntx_replace(file, id, buffer) == 0)
    die("Unable to locate note %s in %s.", id, file);

  release(buffer);
}

void ntx_usage(int retcode)
{
  /* Abbreviated usage information for ntx. */
  puts("Usage:\tntx [mode] [arguments] ..\n");
  puts("Modes:\tadd  [tags ..]\t\tAdd a note to tags.");
  puts("\tedit [hex]\t\tEdit the note with the ID 'hex'.");
  puts("\tlist <tags ..>\t\tList the notes in the intersection of 'tags'.");
  puts("\tput  [hex]\t\tPrint the note with the ID 'hex' to STDOUT.");
  puts("\trm   [hex]\t\tDelete the note identified by the ID 'hex'.");
  puts("\ttag  <hex>\t\tPrint all tags, or those attached to the ID 'hex'.\n");
  puts("\ttag  [hex] [tags ..]\tRe-tag 'hex' with the list 'tags'.");
  puts("\t-h or --help\t\tPrint this information.\n");

  /* Explanation of the output of 'ntx list'. */
  puts("The focus of ntx is displaying tag intersections, as performed by");
  puts("'ntx list'. This outputs a four-byte hexidecimal ID, a tab, and");
  printf("then a brief summary composed of the first %d bytes of the\n",
          SUMMARY_LENGTH);
  puts("first line of the saved note. This ID is serves as a reference to");
  puts("the note when using the 'edit', 'put', 'rm', and 'tag' modes.\n");

  /* Paragraph concerning the user interface when adding/altering notes. */
  puts("When adding or editing notes, the interface presented to the user");
  puts("depends on the host operating environment. On POSIX-style systems,");
  puts("if ntx is receiving input from a tty, the editor specified in the");
  puts("environment variable EDITOR is used; otherwise, input is read from");
  puts("STDIN to the target file.");

  exit(retcode);
}

/* Very few arguments, so we use a hand-written parser. */
int main(int argc, char **argv)
{
  exception_t exc;
  const char *error;
  int errnum;

  atexit(release_all);

  if(argc < 2) ntx_usage(EXIT_FAILURE);
  if(!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h"))
    ntx_usage(EXIT_SUCCESS);

  /* Change to/create our root directory. */
  ntx_homedir(TAGS_DIR, REFS_DIR, NOTES_DIR, NULL);

  try {
    if(!strcmp(argv[1], "add")    &&    argc >= 3) ntx_add(argv+2);
    else if(!strcmp(argv[1], "edit") && argc == 3) ntx_edit(argv[2]);
    else if(!strcmp(argv[1], "list") && argc >= 2) ntx_list(argv+2, argc - 2);
    else if(!strcmp(argv[1], "put") &&  argc == 3) ntx_put(argv[2]);
    else if(!strcmp(argv[1], "rm")  &&  argc == 3) ntx_del(argv[2]);
    else if(!strcmp(argv[1], "tag") &&  argc > 3)  ntx_retag(argv[2], argv+3);
    else if(!strcmp(argv[1], "tag") && (argc == 2 || argc == 3))
                                                   ntx_tags(argv[2]);
    else ntx_usage(EXIT_FAILURE);
  } catch(exc) {
    switch(exc.type) {
      case E_FIOERR:   fclose(exc.value);
                       die(strerror(errno));
      case E_GZFIOERR: error = gzerror(exc.value, &errnum);
                       fprintf(stderr, "ERROR: %s\n", error);
                       gzclose(exc.value);
                       exit(EXIT_FAILURE);
      case E_OVRFLO:   die("Buffer overflow prevented.");
      case E_FACCESS:  die("Unable to open %s.", exc.value);
      case E_NOMEM:    die("Unable to allocate sufficient memory.");
      case E_BADFREE:  die("Double free of %d.", (int)exc.value);
      case E_INVAL:    die("%s is corrupt.", exc.value);
      case E_NONE:
      case E_USER:     die("Unknown exception caught.");
    }
  }
  return EXIT_SUCCESS;
}
