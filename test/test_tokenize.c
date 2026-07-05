/* assert-based smoke test for WM_LC_Tokenize_Line's config-line path handling */
#include <assert.h>
#include <string.h>
#include <stdlib.h>

extern char **WM_LC_Tokenize_Line(char *line_data);

/* ponytail: leaks the strdup'd line buffer, tokens point into it and the
 * process exits right after main(); not worth tracking for a smoke test. */
static char **tokenize(const char *line) {
    return WM_LC_Tokenize_Line(strdup(line));
}

int main(void) {
    char **toks;

    /* quoted path with embedded spaces stays one token */
    toks = tokenize("dir \"/Users/foo/Application Support/patches\"");
    assert(toks && !strcmp(toks[0], "dir"));
    assert(!strcmp(toks[1], "/Users/foo/Application Support/patches"));
    assert(toks[2] == NULL);
    free(toks);

    /* empty quoted string yields an empty token, not a dropped one */
    toks = tokenize("dir \"\"");
    assert(toks && !strcmp(toks[0], "dir"));
    assert(toks[1] && toks[1][0] == '\0');
    assert(toks[2] == NULL);
    free(toks);

    /* unterminated quote still parses what it can, doesn't crash */
    toks = tokenize("dir \"unterminated");
    assert(toks && !strcmp(toks[0], "dir"));
    assert(!strcmp(toks[1], "unterminated"));
    free(toks);

    /* plain unquoted args are unaffected */
    toks = tokenize("a b c");
    assert(toks && !strcmp(toks[0], "a") && !strcmp(toks[1], "b") && !strcmp(toks[2], "c"));
    assert(toks[3] == NULL);
    free(toks);

    /* comments outside quotes are stripped */
    toks = tokenize("dir /patches # comment");
    assert(toks && !strcmp(toks[0], "dir") && !strcmp(toks[1], "/patches"));
    assert(toks[2] == NULL);
    free(toks);

    return 0;
}
