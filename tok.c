#include "simpsh.h"
#include "parser.h"
#include "malloc.h"

/*
 * shell language tokenizer. parses a line extracts a word, handles quoting,
 * variable expansion, escaping.
 *
 * it's used in tokenize, it returns immediately if it's at the position of an
 * opterator tokenize handles finding those. also whitespace etc.
 *
 * it advances the position for the caller, then the caller needs to advance
 * through whitespace, and operators when it handles them.
 */
char *
get_word(char *line, size_t *pos) {
  quoted state;
  size_t bufpos = 0, i = *pos;
  size_t bufsize = BUF_S;
  size_t var_l;
  char *var;
  char c, n;

  // caller frees
  char *buf = malloc(bufsize);
  if (!buf)
    return NULL;

  state = QNONE;

  while (line[i]) {
    c = line[i];
    n = line[i + 1];
    if (bufpos + 1 >= bufsize) {
      buf = s_realloc(buf, &bufsize);
      if (!buf)
        return NULL;
    }

    switch (state) {
    case QNONE:
      if (c == ' ' || c == '\t' || c == '\n') {
        goto done;
      } else if (c == '&' || c == '|' || c == ';') {
        goto done;
      } else if (c == '\'') {
        state = QSINGLE;
        i++;
      } else if (c == '"') {
        state = QDOUBLE;
        i++;
      } else if (c == '\\') {
        if (n == '\0') {
          i++;
          goto done;
        } else {
          buf[bufpos++] = n;
          i += 2;
        }
      } else if (c == '$') {
        var = exp_var(line, &i, &var_l); /* NOTE: exp_var updates i so no i++ */
        if (!var)
          return NULL;
        if (bufpos + var_l >= bufsize) {
          buf = s_realloc(buf, &bufsize);
          if (!buf) {
            perror("realloc failed");
            return NULL;
          }
        }
        memcpy(buf + bufpos, var, var_l);
        free(var);
        bufpos += var_l;
      } else {
        buf[bufpos++] = c;
        i++;
      }
      break;

    case QSINGLE:
      if (c == '\'') {
        state = QNONE;
        i++;
      } else {
        buf[bufpos++] = c;
        i++;
      }
      break;

    case QDOUBLE:
      if (c == '"') {
        state = QNONE;
        i++;
      } else if (c == '\\') {
        if (n == '\n') {
          i += 2;
          continue;  // ? idk
        } else if (n == '$' || n == '"' || n == '\\') {
          buf[bufpos++] = n;
          i += 2;
        } else {
          buf[bufpos++] = c;
          i++;
        }
      } else if (c == '$') {
        var = exp_var(line, &i, &var_l); /* NOTE: exp_var updates i so no i++ */
        if (!var)
          return NULL;
        if (bufpos + var_l >= bufsize) {
          buf = s_realloc(buf, &bufsize);
          if (!buf) {
            perror("realloc failed");
            return NULL;
          }
        }
        memcpy(buf + bufpos, var, var_l);
        free(var);
        bufpos += var_l;
      } else {
        buf[bufpos++] = c;
        i++;
      }
      break;
    }
  }

done:
  buf[bufpos] = '\0';
  *pos = i;
  return buf;
}

sh_tok *
tokenize(char *line, int *cnt) {
  size_t p = 0, c = 0, n;
  *cnt = 0;
  char *buf = NULL;

  if (!line) {
    *cnt = -1;
    return NULL;
  }
  if (strlen(line) == 0)
    return NULL;

  sh_tok *tokens = malloc(MAX_CMDS * (sizeof(sh_tok)));
  if (!tokens)
    return NULL;

  /* go until we hit the null byte */
  while (line[p]) {
    // /* save first word in the list */
    // if (!(buf = get_word(line, &p)))
    //   return NULL;
    // if (buf && buf[0] != '\0') { 
    //   tokens[c].type = TWORD;
    //   tokens[c].cmd = buf;
    //   // maybe try this
    //   //c++;
    //   buf = NULL;
    // } else {
    //   free(buf);
    //   buf = NULL;
    // }

    /* skip whitespace */
    while (line[p] == ' ' || line[p] == '\n' || line[p] == '\t')
      p++;
    if (!line[p])
      break;

    n = p + 1;

    // removed free buff on each of these put back if needed
    if (line[p] == '&' && line[n] == '&') {
      tokens[c].type = TAND;
      tokens[c].cmd = NULL;
      c++;
      p += 2;
      continue;
    } else if (line[p] == '|' && line[n] == '|') {
      tokens[c].type = TOR;
      tokens[c].cmd = NULL;
      c++;
      p += 2;
      continue;
    } else if (line[p] == ';') {
      tokens[c].type = TSEMI;
      tokens[c].cmd = NULL;
      c++;
      p++;
      continue;
    } else {

      if (!(buf = get_word(line, &p)))
        return NULL;
      if (buf[0] != '\0') { 
        tokens[c].type = TWORD;
        tokens[c].cmd = buf;
        c++;
      } else {
        free(buf);
      }
    }
  }
  tokens[c].type = TEOF;
  tokens[c].cmd = NULL;

  *cnt = c;
  return tokens;
}

  // //debug
  // for (int j = 0; j < c; j++) {
  //   fprintf(stderr, "token[%d]: type=%d cmd='%s'\n", j, tokens[j].type, tokens[j].cmd);
  // }
