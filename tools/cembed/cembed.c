/*
** Copyright (c) 2020 rxi
**
** Permission is hereby granted, free of charge, to any person obtaining a copy
** of this software and associated documentation files (the "Software"), to
** deal in the Software without restriction, including without limitation the
** rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
** sell copies of the Software, and to permit persons to whom the Software is
** furnished to do so, subject to the following conditions:
**
** The above copyright notice and this permission notice shall be included in
** all copies or substantial portions of the Software.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
** AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
** LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
** FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
** IN THE SOFTWARE.
**/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

#define VERSION "v0.3"
#define MAX_FILENAME 256

typedef struct {
  FILE *fp;
  unsigned char buf[4096];
  int idx;
  int eof_idx;
} BufferedFile;


static BufferedFile bf_writer(FILE *fp) {
  BufferedFile bf = { .fp = fp };
  return bf;
}


static BufferedFile bf_reader(FILE *fp) {
  BufferedFile bf = { .fp = fp, .idx = sizeof(bf.buf), .eof_idx = -1 };
  return bf;
}


static void bf_flush(BufferedFile *bf) {
  fwrite(bf->buf, 1, bf->idx, bf->fp);
  bf->idx = 0;
}


static void bf_write_byte(BufferedFile *bf, char b) {
  bf->buf[bf->idx++] = b;
  if (bf->idx == sizeof(bf->buf)) { bf_flush(bf); }
}


static int bf_read_byte(BufferedFile *bf) {
  if (bf->idx == sizeof(bf->buf)) {
    int n = fread(bf->buf, 1, sizeof(bf->buf), bf->fp);
    if (n != sizeof(bf->buf)) { bf->eof_idx = n; }
    bf->idx = 0;
  }
  if (bf->idx == bf->eof_idx) { return EOF; }
  return bf->buf[bf->idx++];
}


static void error(const char *fmt, ...) {
  va_list vp;
  fprintf(stderr, "Error: ");
  va_start(vp, fmt);
  vfprintf(stderr, fmt, vp);
  va_end(vp);
  fprintf(stderr, "\n");
  exit(EXIT_FAILURE);
}


static void safename(char *dst, const char *filename) {
  const char *p = filename;
  char *q = dst;
  while (*p) {
    if (isalpha(*p) || isdigit(*p)) {
      *q = *p;
    } else {
      *q = '_';
    }
    q++; p++;
  }
  *q = '\0';
}


static void write_byte_string(BufferedFile *bf, unsigned char n) {
  if (n >= 100) { bf_write_byte(bf, '0' + (n / 100) % 10); }
  if (n >=  10) { bf_write_byte(bf, '0' + (n /  10) % 10); }
  bf_write_byte(bf, '0' + n % 10);
}


static void write_embedded(FILE *fp, const char *filename,
  const char *varprefix, int nostatic, int zerobyte)
{
  FILE *infp = fopen(filename, "rb");
  if (!infp) {
    error("failed to open file '%s'", filename);
  }

  char varname[MAX_FILENAME];
  if (strlen(filename) >= MAX_FILENAME) {
    error("filename too long");
  }
  safename(varname, filename);

  if (!nostatic) { fprintf(fp, "static "); }
  fprintf(fp, "unsigned char golf_data_zip[] = {");
  BufferedFile inbf = bf_reader(infp);
  BufferedFile bf = bf_writer(fp);
  int n = 0;

  for (;;) {
    int chr = bf_read_byte(&inbf);
    if (chr == EOF) { break; }
    if (n > 0) { bf_write_byte(&bf, ','); }
    if (n % 20 == 0) { bf_write_byte(&bf, '\n'); }
    write_byte_string(&bf, chr);
    n++;
  }

  bf_flush(&bf);
  if (zerobyte) { fprintf(fp, ",0"); }
  fprintf(fp, "\n};\n\n");

  fclose(infp);
}


static void print_help(void) {
  printf(
    "Usage: cembed [OPTION]... [FILE]...\n"
    "Create C header with file data embedded in char arrays\n"
    "\n"
    "  -o <filename>  output file\n"
    "  -p <prefix>    prefix to place before variable names\n"
    "  -s             omit `static` keyword\n"
    "  -z             add zero byte to end of array\n"
    "  -t <name>      create table of { filename, data, size }\n"
    "  -h             display this help message\n"
    "  -v             display version number\n");
}


int main(int argc, char **argv) {
  char **arg = argv + 1;
  char **arg_end = argv + argc;

  /* defaults */
  const char *outfile = NULL;
  const char *prefix = "";
  const char *tablename = NULL;
  int zerobyte = 0;
  int nostatic = 0;

  /* handle options */
  while (arg != arg_end && (*arg)[0] == '-') {
    switch ((*arg)[1]) {
      case 'h':
        print_help();
        exit(EXIT_SUCCESS);
        break;

      case 'v':
        printf("cembed " VERSION "\n");
        exit(EXIT_SUCCESS);
        break;

      case 's':
        nostatic = 1;
        break;

      case 'z':
        zerobyte = 1;
        break;

      case 't':
        arg++;
        if (arg == arg_end) { error("expected name after option '-t'"); }
        tablename = *arg;
        break;

      case 'o':
        arg++;
        if (arg == arg_end) { error("expected filename after option '-o'"); }
        outfile = *arg;
        break;

      case 'p':
        arg++;
        if (arg == arg_end) { error("expected prefix after option '-p'"); }
        prefix = *arg;
        break;

      default:
        error("invalid option '%s'", *arg);
        break;
    }

    arg++;
  }

  /* no file arguments: print help */
  if (arg == arg_end) {
    print_help();
    exit(EXIT_SUCCESS);
  }

  /* open output */
  FILE *fp = outfile ? fopen(outfile, "wb") : stdout;
  if (!fp) { error("failed to open output file '%s'", outfile); }

  /* write files */
  for (char **a = arg; a < arg_end; a++) {
    write_embedded(fp, *a, prefix, nostatic, zerobyte);
  }

  /* write table */
  if (tablename) {
    if (!nostatic) { fprintf(fp, "static "); }
    fprintf(fp, "struct { char *filename; unsigned char *data; int size; } ");
    fprintf(fp, "%s[] = {\n", tablename);
    for (char **a = arg; a < arg_end; a++) {
      char varname[MAX_FILENAME];
      safename(varname, *a);
      fprintf(fp, "{ \"%s\", %s, (int) sizeof(%s) ", *a, varname, varname);
      if (zerobyte) { fprintf(fp, "- 1 "); }
      fprintf(fp, "},\n");
    }
    fprintf(fp, "{ 0 }\n");
    fprintf(fp, "};\n");
  }

  /* clean up */
  if (fp != stdout) { fclose(fp); }
  return EXIT_SUCCESS;
}
