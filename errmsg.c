#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdarg.h>

#include "errmsg.h"
#include "utils.h"


/* error (returns doesn't exit) with simpsh: builtin: message: (errno message),
 * format */
void
warn(const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fprintf(stderr, ": %s\n", strerror(errno));
}

void
err(int eval, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fprintf(stderr, ": %s\n", strerror(errno));
  exit(eval);
}

int
sherr(int r, const char *str, const char *msg)
{
  warn("%s: %s", str, msg);
  return r;
}

int
sherrx(int r, const char *msg)
{
  warn("%s\n", msg);
  return r;
}

/* warning with simpsh: builtin: message, format */
int
shwarn(const char *str, const char *msg)
{
  fprintf(stderr, "%s: %s: %s\n", shname, str, msg);
  return 1;
}

/* error message with simpsh: builtin: arg: message, format */
int
shwarn_arg(char *str, const char *arg, const char *msg)
{
  fprintf(stderr, "%s: %s: %s: %s\n", shname, str, arg, msg);
  return 1;
}

static const char caseusg[] = "WORD in [PATTERN [| PATTERN]) COMMANDS ;;] ... esac";
static const char execusg[] = "[redirection] [command [arg]]";
static const char forusg[] = "NAME [in WORDS ...] ; do COMMANDs; done";
static const char ifusg[] = "COMMANDS; then COMMANDS; [elif COMMANDS; then COMMANDS;]... [else COMMANDS;] fi";
static const char killusg[] = "[ -s sigspec | -signum | -signame ] [ pid | job ] ... or \n" " kill -l [exitstatus]...";
static const char setusg[] = "[-abCefhiImnsuvVx] [-o option] [-- args]";
static const char untilusg[] = "COMMANDS; do COMMANDS-2; done";
static const char ulimitusg[] = "[-aHS] [-c core] [-d data] [-f blocks] "
  "[-l locked] [-m memory] [-n nofiles] [-p processes] [-s stack] [-t time] [limit]";
static const char whileusg[] = "COMMANDS; do COMMANDS-2; done";

static const char dothelp[] =
  ". filename [arguments]\n"
  "\n"
  "    Execute commands from a file in the current shell.\n"
  "\n"
  "    Read and execute commands from the specified file in the current\n"
  "    shell environment.  If the file does not contain a slash, the shell\n"
  "    searches the PATH to find the file.\n"
  "\n"
  "Exit Status:\n"
  "    Returns the exit status of the last command executed from the file,\n"
  "    or 1 if the file could not be read.";

static const char testhelp[] =
  "[ expression ]\n"
  "\n"
  "    Evaluate a conditional expression.\n"
  "\n"
  "    The `test' builtin evaluates conditional expressions.  For the `['\n"
  "    form, the last argument must be `]'.\n"
  "\n"
  "    File operators:\n"
  "      -b FILE    True if FILE is a block device.\n"
  "      -c FILE    True if FILE is a character device.\n"
  "      -d FILE    True if FILE is a directory.\n"
  "      -e FILE    True if FILE exists.\n"
  "      -f FILE    True if FILE is a regular file.\n"
  "      -G FILE    True if FILE is owned by effective group ID.\n"
  "      -g FILE    True if FILE has set-group-ID bit set.\n"
  "      -h FILE    True if FILE is a symbolic link.\n"
  "      -k FILE    True if FILE has sticky bit set.\n"
  "      -L FILE    True if FILE is a symbolic link.\n"
  "      -O FILE    True if FILE is owned by effective user ID.\n"
  "      -p FILE    True if FILE is a named pipe (FIFO).\n"
  "      -r FILE    True if FILE exists and is readable.\n"
  "      -s FILE    True if FILE exists and has size > 0.\n"
  "      -S FILE    True if FILE is a socket.\n"
  "      -t FD      True if file descriptor FD is open on a terminal.\n"
  "      -u FILE    True if FILE has set-user-ID bit set.\n"
  "      -w FILE    True if FILE exists and is writable.\n"
  "      -x FILE    True if FILE exists and is executable.\n"
  "\n"
  "    String operators:\n"
  "      -n STRING    True if STRING length is non-zero.\n"
  "      -z STRING    True if STRING length is zero.\n"
  "      STRING = STRING   True if strings are equal.\n"
  "      STRING != STRING  True if strings differ.\n"
  "      STRING < STRING   True if STRING sorts before (locale).\n"
  "      STRING > STRING   True if STRING sorts after (locale).\n"
  "\n"
  "    Integer operators:\n"
  "      INT -eq INT    True if INT1 equals INT2.\n"
  "      INT -ne INT    True if INT1 does not equal INT2.\n"
  "      INT -gt INT    True if INT1 is greater than INT2.\n"
  "      INT -ge INT    True if INT1 is >= INT2.\n"
  "      INT -lt INT    True if INT1 is less than INT2.\n"
  "      INT -le INT    True if INT1 is <= INT2.\n"
  "\n"
  "    File comparison operators:\n"
  "      FILE -ef FILE   True if files refer to the same inode.\n"
  "      FILE -nt FILE   True if FILE1 is newer (mtime) than FILE2.\n"
  "      FILE -ot FILE   True if FILE1 is older than FILE2.\n"
  "\n"
  "    Logical operators:\n"
  "      ! EXPR          Negates the expression.\n"
  "      EXPR -a EXPR    True if both expressions are true.\n"
  "      EXPR -o EXPR    True if either expression is true.\n"
  "      ( EXPR )        Grouping.\n"
  "\n"
  "Exit Status:\n"
  "    Returns 0 if the expression is true, 1 if false, 2 on error.";

static const char truehelp[] = "\n"
                               "\n"
                               "    Returns success. does nothing else.\n"
                               "\n"
                               "Exit Status:\n"
                               "    Always returns 0.";

static const char aliashelp[] =
  "alias [name[=value] ...]\n"
  "\n"
  "    Define or display aliases.\n"
  "\n"
  "    Without arguments, prints all defined aliases.  With\n"
  "    `name=value', defines an alias.  With `name' only, prints\n"
  "    the alias for that name.\n"
  "\n"
  "Exit Status:\n"
  "    Returns 0 unless an error occurs.";

static const char bghelp[] =
  "bg [job]\n"
  "\n"
  "    Resume a job in the background.\n"
  "\n"
  "    Puts the specified job (or the current job) into the background.\n"
  "    The job is continued as if it had been started with `&'.\n"
  "\n"
  "Exit Status:\n"
  "    Returns 0 unless an invalid job is specified.";

static const char breakhelp[] =
  "break [n]\n"
  "\n"
  "    Exit from a for, while, or until loop.\n"
  "\n"
  "    If N is specified, break N levels of nesting.  If N is greater\n"
  "    than the current nesting level, all loops are exited.\n"
  "\n"
  "Exit Status:\n"
  "    Returns 0 unless the shell is not currently in a loop.";

static const char casehelp[] =
  "case WORD in [PATTERN [| PATTERN]...) COMMANDS ;;]... esac\n"
  "\n"
  "    Execute commands based on pattern matching.\n"
  "\n"
  "    The WORD is expanded and matched against each PATTERN in order.\n"
  "    Patterns may contain wildcards (`*', `?', `[...]').  When a\n"
  "    match is found, the corresponding COMMANDS are executed, and\n"
  "    the case statement completes.\n"
  "\n"
  "Exit Status:\n"
  "    Returns the exit status of the last command executed, or 0 if\n"
  "    no pattern matches.";

static const char cdhelp[] =
  "cd [-LP] [dir]\n"
  "\n"
  "    Change the current working directory.\n"
  "\n"
  "    Without arguments, changes to HOME.  With `-', changes to\n"
  "    OLDPWD and prints the new directory.  If CDPATH is set, each\n"
  "    path entry is searched for the target directory.\n"
  "\n"
  "    Options:\n"
  "      -L    Follow symbolic links (default).\n"
  "      -P    Resolve the path physically, do not follow symlinks.\n"
  "\n"
  "Exit Status:\n"
  "    Returns 0 if the directory is changed, 1 on error.";

static const char continuehelp[] =
  "continue [n]\n"
  "\n"
  "    Resume the next iteration of a for, while, or until loop.\n"
  "\n"
  "    If N is specified, resume at the N-th enclosing loop level.\n"
  "\n"
  "Exit Status:\n"
  "    Returns 0 unless the shell is not currently in a loop.";

static const char echohelp[] =
  "echo [-n] [string ...]\n"
  "\n"
  "    Write arguments to standard output.\n"
  "\n"
  "    Writes each argument separated by a space character, followed\n"
  "    by a newline, to stdout.\n"
  "\n"
  "    Options:\n"
  "      -n    Do not append a trailing newline.\n"
  "\n"
  "Exit Status:\n"
  "    Returns 0 unless an error occurs writing to stdout.";

static const char exechelp[] =
  "exec [command [arg ...]] [redirection]\n"
  "\n"
  "    Execute a command in place of the current shell, or manipulate\n"
  "    file descriptors.\n"
  "\n"
  "    If COMMAND is specified, it replaces the shell.  No new process\n"
  "    is created.  If only redirections are given, they apply to the\n"
  "    current shell environment.\n"
  "\n"
  "Exit Status:\n"
  "    If COMMAND is executed, the shell does not return.  If only\n"
  "    redirections are given, returns 0 on success or 1 on failure.";

static const char exithelp[] =
  "exit [n]\n"
  "\n"
  "    Exit the shell with exit status N.\n"
  "\n"
  "    If N is omitted, the exit status is that of the last command\n"
  "    executed.  If the shell is interactive and IGNOREEOF is set,\n"
  "    EOF does not cause the shell to exit.\n"
  "\n"
  "Exit Status:\n"
  "    Returns N to the parent process.";

static const char exporthelp[] =
  "export name[=value] ...\n"
  "\n"
  "    Set the export attribute for shell variables.\n"
  "\n"
  "    Marks each NAME for automatic export to the environment of\n"
  "    subsequent commands.  If VALUE is provided, the variable is\n"
  "    assigned that value and marked for export.\n"
  "\n"
  "Exit Status:\n"
  "    Returns 0 unless an invalid variable name is given.";

static const char falsehelp[] = "\n"
                                "\n"
                                "    Null command that always fails.\n"
                                "\n"
                                "    Does nothing and ignores all arguments.\n"
                                "\n"
                                "Exit Status:\n"
                                "    Always returns 1.";

static const char forhelp[] =
  "for NAME [in WORDS ...] ; do COMMANDS ; done\n"
  "\n"
  "    Execute commands for each member in a list.\n"
  "\n"
  "    For each WORD in the list following `in', NAME is set to that\n"
  "    word and COMMANDS are executed.  If `in WORDS' is omitted,\n"
  "    NAME is set to each positional parameter in turn.\n"
  "\n"
  "Exit Status:\n"
  "    Returns the exit status of the last command executed.";

static const char fghelp[] =
  "fg [job]\n"
  "\n"
  "    Bring a job to the foreground.\n"
  "\n"
  "    Brings the specified job (or the current job) into the\n"
  "    foreground and gives it control of the terminal.\n"
  "\n"
  "Exit Status:\n"
  "    Returns 0 unless an invalid job is specified.";

static const char hashhelp[] =
  "hash [-r] [name ...]\n"
  "\n"
  "    Remember or display program locations.\n"
  "\n"
  "    For each NAME, the full path is located and stored in the\n"
  "    hash cache, avoiding PATH searches on subsequent invocations.\n"
  "\n"
  "    Options:\n"
  "      -r    Flush the entire hash cache.\n"
  "\n"
  "Exit Status:\n"
  "    Returns 0 unless a name is not found.";

static const char helphelp[] =
  "help [builtin ...]\n"
  "\n"
  "    Display information about builtin commands.\n"
  "\n"
  "    Without arguments, lists all builtins with their usage.\n"
  "    With arguments, displays detailed help for each named\n"
  "    builtin or shell keyword.\n"
  "\n"
  "Exit Status:\n"
  "    Returns 0 if help is displayed for all named builtins,\n"
  "    or 1 if a builtin is not found.";

static const char ifhelp[] =
  "if COMMANDS ; then COMMANDS ; [ elif COMMANDS ; then COMMANDS ; ] ... [ "
  "else COMMANDS ; ] fi\n"
  "\n"
  "    Execute commands based on conditional.\n"
  "\n"
  "    The `if COMMANDS' list is executed.  If its exit status is\n"
  "    zero, the `then COMMANDS' are executed.  Otherwise, each\n"
  "    `elif' list is tried in turn.  If no condition succeeds,\n"
  "    the `else' branch is executed if present.\n"
  "\n"
  "Exit Status:\n"
  "    Returns the exit status of the last command executed, or 0\n"
  "    if no condition tested true and there is no else branch.";

static const char jobshelp[] =
  "jobs [job ...]\n"
  "\n"
  "    Display the status of active jobs.\n"
  "\n"
  "    Lists each job with its job number, current/previous marker,\n"
  "    state (Running, Stopped, Done), and command string.\n"
  "\n"
  "Exit Status:\n"
  "    Returns 0.";

static const char localhelp[] =
  "local name[=value] ...\n"
  "\n"
  "    Define local variables.\n"
  "\n"
  "    Creates a local variable scoped to the current function.\n"
  "    The variable's previous value is restored when the function\n"
  "    returns.  Only valid inside a shell function definition.\n"
  "\n"
  "Exit Status:\n"
  "    Returns 0 unless used outside a function.";

static const char pwdhelp[] =
  "pwd [-LP]\n"
  "\n"
  "    Print the current working directory.\n"
  "\n"
  "    Options:\n"
  "      -L    Print the logical path from $PWD (default).\n"
  "      -P    Print the physical path with all symlinks resolved.\n"
  "\n"
  "Exit Status:\n"
  "    Returns 0 on success, 1 on error.";

static const char readhelp[] =
  "read [-p prompt] [-r] var ...\n"
  "\n"
  "    Read a line from standard input.\n"
  "\n"
  "    Reads one line from stdin, splits it according to IFS, and\n"
  "    assigns each field to a variable.  Remaining fields go to\n"
  "    the last variable.\n"
  "\n"
  "    Options:\n"
  "      -p PROMPT  Display PROMPT on stderr before reading.\n"
  "      -r         Raw input: backslash does not act as escape.\n"
  "\n"
  "Exit Status:\n"
  "    Returns 0 on success, 1 on EOF or error.";

static const char readonlyhelp[] =
  "readonly name[=value] ...\n"
  "\n"
  "    Mark shell variables as unchangeable.\n"
  "\n"
  "    For each NAME, marks the variable as readonly.  If VALUE is\n"
  "    given, the variable is set first.  Subsequent attempts to\n"
  "    unset or modify a readonly variable will fail.\n"
  "\n"
  "Exit Status:\n"
  "    Returns 0 unless an error occurs.";

static const char returnhelp[] =
  "return [n]\n"
  "\n"
  "    Return from a shell function or sourced script.\n"
  "\n"
  "    Causes a function or sourced script to exit with the return\n"
  "    value specified by N.  If N is omitted, the return status is\n"
  "    that of the last command executed.\n"
  "\n"
  "Exit Status:\n"
  "    Returns N, or 1 if not inside a function or sourced script.";

static const char sethelp[] =
  "set [-abCefhiImnsuvVx] [-o option] [+abCefhiImnsuvVx] [+o option] [-- "
  "args]\n"
  "\n"
  "    Change or show shell options and positional parameters.\n"
  "\n"
  "    Without arguments, prints all shell variables and their values\n"
  "    (sorted by name, quoted for re-input).\n"
  "\n"
  "    Options:\n"
  "      -a      Mark variables for export on assignment.\n"
  "      -b      Notify of job termination immediately.\n"
  "      -C      Do not overwrite existing files with >.\n"
  "      -e      Exit immediately on error (errexit).\n"
  "      -f      Disable pathname expansion (noglob).\n"
  "      -h      Hash commands as they are located.\n"
  "      -i      Interactive shell.\n"
  "      -I      Ignore EOF from input (ignoreeof).\n"
  "      -m      Enable job control (monitor).\n"
  "      -n      Read commands but do not execute (noexec).\n"
  "      -s      Read commands from stdin.\n"
  "      -u      Treat unset variables as an error (nounset).\n"
  "      -v      Print input lines as they are read (verbose).\n"
  "      -V      Vi-style line editing (not yet implemented).\n"
  "      -x      Print commands and arguments as executed (xtrace).\n"
  "\n"
  "    Using `+' instead of `-' unsets the option.  `-o NAME' and\n"
  "    `+o NAME' set or unset an option by name.  `set -o' (no name)\n"
  "    lists current option states.  A bare `--' marks the end of\n"
  "    options; subsequent arguments become positional parameters.\n"
  "\n"
  "Exit Status:\n"
  "    Returns 0 on success, 1 on failure.";

static const char untilhelp[] =
  "until COMMANDS ; do COMMANDS ; done\n"
  "\n"
  "    Execute commands until a test succeeds.\n"
  "\n"
  "    The `until COMMANDS' list is executed.  If its exit status\n"
  "    is non-zero, the `do COMMANDS' are executed and the loop\n"
  "    repeats.  If the exit status is zero, the loop terminates.\n"
  "\n"
  "Exit Status:\n"
  "    Returns the exit status of the last command executed in the\n"
  "    `do' body, or 0 if the condition was true initially.";

static const char umaskhelp[] =
  "umask [-S] [mode]\n"
  "\n"
  "    Display or set the file mode creation mask.\n"
  "\n"
  "    Without arguments, prints the current mask in octal.  With\n"
  "    a MODE argument (octal or symbolic), sets the mask.\n"
  "\n"
  "    Options:\n"
  "      -S    Print the mask in symbolic (rwx) format.\n"
  "\n"
  "Exit Status:\n"
  "    Returns 0 on success, 1 on error.";

static const char unaliashelp[] =
  "unalias name ...\n"
  "\n"
  "    Remove each NAME from the alias list.\n"
  "\n"
  "Exit Status:\n"
  "    Returns 0 unless an alias does not exist.";

static const char unsethelp[] =
  "unset [-fv] name ...\n"
  "\n"
  "    Unset values and attributes of shell variables and functions.\n"
  "\n"
  "    Options:\n"
  "      -f    Treat each NAME as a function name.\n"
  "      -v    Treat each NAME as a variable name (default).\n"
  "\n"
  "Exit Status:\n"
  "    Returns 0 on success, 1 on error.";

static const char whilehelp[] =
  "while COMMANDS ; do COMMANDS ; done\n"
  "\n"
  "    Execute commands while a test succeeds.\n"
  "\n"
  "    The `while COMMANDS' list is executed.  If its exit status\n"
  "    is zero, the `do COMMANDS' are executed and the loop\n"
  "    repeats.  If the exit status is non-zero, the loop terminates.\n"
  "\n"
  "Exit Status:\n"
  "    Returns the exit status of the last command executed in the\n"
  "    `do' body, or 0 if the condition was false initially.";

static const char bracehelp[] =
  "{ COMMANDS ; }\n"
  "\n"
  "    Group commands as a single unit.\n"
  "\n"
  "    Execute COMMANDS in the current shell environment.  The group\n"
  "    behaves as a single compound command for redirection.  Unlike a\n"
  "    subshell, variables set inside affect the current shell.\n"
  "\n"
  "Exit Status:\n"
  "    Returns the exit status of the last command executed.";

static const char commandhelp[] =
  "command [-pvV] name [argument]\n"
  "\n"
  "    Execute a command, bypassing shell functions and aliases.\n"
  "\n"
  "    With -p, uses the default PATH. With -v, prints a description\n"
  "    of NAME. With -V, prints all matching descriptions.\n"
  "\n"
  "Exit Status:\n"
  "    Returns the exit status of the executed command, or 126/127\n"
  "    on failure.";

static const char evalhelp[] =
  "eval [arg ...]\n"
  "\n"
  "    Read and execute arguments as shell input.\n"
  "\n"
  "    The arguments are concatenated into a single string, parsed,\n"
  "    and executed as shell commands.\n"
  "\n"
  "Exit Status:\n"
  "    Returns the exit status of the last command executed.";

static const char getoptshelp[] =
  "getopts optstring name [arg ...]\n"
  "\n"
  "    Parse positional parameters with option flags.\n"
  "\n"
  "    OPTSTRING contains the option letters to be recognized. Each\n"
  "    time it is called, the next option is assigned to NAME and\n"
  "    its argument (if any) to OPTARG. OPTIND is updated.\n"
  "\n"
  "Exit Status:\n"
  "    Returns 0 if an option was found, 1 if no more options, 2\n"
  "    on error.";

static const char killhelp[] =
  "kill [-s sigspec | -signum | -signame] [pid | job] ...\n"
  "kill -l [exitstatus]\n"
  "\n"
  "    Send signals to processes, or list signal names.\n"
  "\n"
  "    With -l, prints signal names. Otherwise, sends the specified\n"
  "    signal (default SIGTERM) to each given PID or job.\n"
  "\n"
  "Exit Status:\n"
  "    Returns 0 unless an invalid option or signal is given.";

static const char printhelp[] =
  "printf format [argument ...]\n"
  "\n"
  "    Format and print arguments according to FORMAT.\n"
  "\n"
  "    Supports standard printf format specifiers and escape\n"
  "    sequences. %%b interprets backslash escapes in arguments,\n"
  "    %%q produces shell-quoted output.\n"
  "\n"
  "Exit Status:\n"
  "    Returns 0 on success, non-zero on error.";

static const char shifthelp[] =
  "shift [n]\n"
  "\n"
  "    Shift positional parameters to the left.\n"
  "\n"
  "    Discards the first N (default 1) positional parameters. The\n"
  "    remaining parameters are renumbered starting from $1.\n"
  "\n"
  "Exit Status:\n"
  "    Returns 0 unless N is greater than the number of positional\n"
  "    parameters.";

static const char timeshelp[] =
  "\n"
  "\n"
  "    Print shell and child process times.\n"
  "\n"
  "    Displays the user and system CPU time consumed by the shell\n"
  "    and its children.\n"
  "\n"
  "Exit Status:\n"
  "    Always returns 0.";

static const char traphelp[] =
  "trap [-lp] [arg] [signal ...]\n"
  "\n"
  "    Trap signals and run commands on receipt.\n"
  "\n"
  "    With no arguments, prints trapped signals. With -l, prints\n"
  "    signal names and numbers. With -p, prints trap actions.\n"
  "    Otherwise, ARG is a command triggered by each SIGNAL. If\n"
  "    ARG is omitted or empty, the signal is reset to its default.\n"
  "    Use ERR as a special signal for command failure traps.\n"
  "\n"
  "Exit Status:\n"
  "    Returns 0 unless an invalid option is given.";

static const char typehelp[] =
  "type name [name ...]\n"
  "\n"
  "    Display how each NAME would be interpreted.\n"
  "\n"
  "    Shows whether each NAME is a shell keyword, alias, function,\n"
  "    builtin, or external command, and its location if applicable.\n"
  "\n"
  "Exit Status:\n"
  "    Returns 0 if all names are found, 1 otherwise.";

static const char ulimithelp[] =
  "ulimit [-aHS] [-c core] [-d data] [-f blocks]\n"
  "  [-l locked] [-m memory] [-n nofiles] [-p processes]\n"
  "  [-s stack] [-t time] [limit]\n"
  "\n"
  "    Set or display resource limits.\n"
  "\n"
  "    Without arguments, prints the current soft limit for all\n"
  "    resources. With -a, prints all limits. -H sets the hard\n"
  "    limit, -S sets the soft limit (default). If LIMIT is given,\n"
  "    the specified resource limit is set.\n"
  "\n"
  "Exit Status:\n"
  "    Returns 0 on success, 1 on error.";

static const char waitforhelp[] =
  "wait [id ...]\n"
  "\n"
  "    Wait for background processes to complete.\n"
  "\n"
  "    Waits for each specified process ID or job to finish. Without\n"
  "    arguments, waits for all background processes.\n"
  "\n"
  "Exit Status:\n"
  "    Returns the exit status of the last waited-for process.";

#ifdef ENABLE_BASENAME
static const char basenamehelp[] =
  "basename name [suffix]\n"
  "\n"
  "    Print the final component of a pathname.\n"
  "\n"
  "    Strips leading directory components. If SUFFIX is given and\n"
  "    matches the end of NAME, it is also stripped.\n"
  "\n"
  "Exit Status:\n"
  "    Always returns 0.";
#endif /* ENABLE_BASENAME */

#ifdef ENABLE_CAT
static const char cathelp[] =
  "cat [file ...]\n"
  "\n"
  "    Concatenate files and write to standard output.\n"
  "\n"
  "    Reads each FILE in order and writes its contents to stdout.\n"
  "    If no files are given, reads from stdin.\n"
  "\n"
  "Exit Status:\n"
  "    Returns 0 on success, 1 on error.";
#endif /* ENABLE_CAT */

#ifdef ENABLE_DIRNAME
static const char dirnamehelp[] =
  "dirname name\n"
  "\n"
  "    Print the directory component of a pathname.\n"
  "\n"
  "    Strips the last component from NAME and prints the remaining\n"
  "    directory path.\n"
  "\n"
  "Exit Status:\n"
  "    Always returns 0.";
#endif /* ENABLE_DIRNAME */

#ifdef ENABLE_CUT
static const char cutusg[] = "[-b list] [-c list] [-d delim] [-f list] [-s] [file ...]";
static const char cuthelp[] =
  "cut -b list [-d delim] [-s] [file ...]\n"
  "cut -c list [file ...]\n"
  "cut -f list [-d delim] [-s] [file ...]\n"
  "\n"
  "    Cut out selected portions of each line.\n"
  "\n"
  "    -b list       Select only these bytes.\n"
  "    -c list       Select only these characters.\n"
  "    -d delim      Use DELIM instead of TAB for field delimiter.\n"
  "    -f list       Select only these fields.\n"
  "    -s            Suppress lines with no delimiter (with -f only).\n"
  "\n"
  "    LIST is a comma-separated list of numbers or ranges:\n"
  "      N       byte/character/field number N\n"
  "      N-M     from N to M inclusive\n"
  "      N-      from N to end of line\n"
  "      -M      from start to M inclusive\n"
  "\n"
  "Exit Status:\n"
  "    Returns 0 on success, 1 on error.";
#endif /* ENABLE_CUT */

#ifdef ENABLE_HEAD
static const char headhelp[] =
  "head [-n count] [file ...]\n"
  "\n"
  "    Print the first lines of files.\n"
  "\n"
  "    Writes the first COUNT (default 10) lines of each FILE to\n"
  "    stdout. If no FILE is given, reads from stdin.\n"
  "\n"
  "Exit Status:\n"
  "    Returns 0 on success, 1 on error.";
#endif /* ENABLE_HEAD */

#ifdef ENABLE_REALPATH
static const char realpathhelp[] = 
  "Usage: realpath [OPTION]... FILE...\n"
  "\n"
  "Print the resolved absolute file name\n"
  "\n"
  "all but the last component must exist\n"
  "Exit Status:\n"
  "     Returns 0 on success, 1 on error";
#endif /* ENABLE_REALPATH */

#ifdef ENABLE_READLINK
static const char readlinkhelp[] =
  "Usage: readlink [OPTION]... FILE...\n"
  "\n"
  "Print value of a symbolic link or canonical file name\n"
  "\n"
  "Exit Status:\n"
  "    Returns 0 if file is link, 1 otherwise";
#endif /* ENABLE_READLINK */

#ifdef ENABLE_SLEEP
static const char sleephelp[] =
  "sleep [n[.n][smhd]] ...\n"
  "\n"
  "    Suspend execution for at least the given time.\n"
  "\n"
  "    Each argument is a non-negative number, optionally followed by\n"
  "    a suffix: s (seconds, default), m (minutes), h (hours), or\n"
  "    d (days).  A fractional part is allowed (e.g., 1.5h).  When\n"
  "    multiple arguments are given, their durations are added.\n"
  "\n"
  "Exit Status:\n"
  "    Returns 0 on success, 1 on error.";
#endif /* ENABLE_SLEEP */

#ifdef ENABLE_TAIL
static const char tailhelp[] =
  "tail [-f] [-n count] [file ...]\n"
  "\n"
  "    Print the last lines of files.\n"
  "\n"
  "    Writes the last COUNT (default 10) lines of each FILE to\n"
  "    stdout. With -f, follows new data as it is appended. If no\n"
  "    FILE is given, reads from stdin.\n"
  "\n"
  "Exit Status:\n"
  "    Returns 0 on success, 1 on error.";
#endif /* ENABLE_TAIL */

#ifdef ENABLE_TEE
static const char teehelp[] =
  "Usage: tee [OPTION]... [FILE]...\n"
  "Copy standard input to each FILE, and also to standard output.\n"
  "\n"
  "-a,            append to the given FILESs, do not overwrite\n"
  "\n"
  "Exit Status:\n"
  "    Returns 0 on sucess, 1 on file, or read errors.";
#endif /* ENABLE_TEE */

#ifdef ENABLE_WC
static const char wchelp[] =
  "Usage: wc [OPTION]... [FILE]...\n"
  "Print newline, word, and byte counts for each FILE, and a total line if\n"
  "more than one FILE is specified.  A word is a non-zero-length sequence of\n"
  "printable characters delimited by white space.\n"
  "\n"
  "With no FILE, or when FILE is -, read standard input.\n"
  "\n"
  "The options below may be used to select which counts are printed, always in\n"
  "the following order: newline, word, character, byte\n"
  "-c,            print the byte counts"
  // "-m,            print the character counts"
  "-l,            print the newline counts"
  "-w,            print the word counts"
  "\n"
  "Exit Status:\n"
  "    Returns 0 on sucess, 1 on file, or read errors.";
#endif /* ENABLE_WC */

  /* clang-format off */
const builtinhelp helpmsgs[] = {
  [DOTH] =      { ".",        "filename [arguments]",   dothelp         },
  [LBRACKH] =   { "[",        "arg... ]",               testhelp        },
  [COLONH] =    { ":",        " ",                      truehelp        },
  [ALIASH] =    { "alias",    "[name[=value] ... ]",    aliashelp       },
  [BGH] =       { "bg",       "[job]",                  bghelp          },
  [BREAKH] =    { "break",    "[n]",                    breakhelp       },
  [CASEH] =     { "case",     caseusg,                  casehelp        },
  [CDH] =       { "cd",       "[-LP] dir",              cdhelp          },
  [COMMANDH] =  { "command",  "[-pvV] name [argument]", commandhelp     },
  [CONTINUEH] = { "continue", "[n]",                    continuehelp    },
  [ECHOH] =     { "echo",     "[-n] arg",               echohelp        },
  [EVALH] =     { "eval",     "[arg ..]",               evalhelp        },
  [EXECH] =     { "exec",     execusg,                  exechelp        },
  [EXITH] =     { "exit",     "[n]",                    exithelp        },
  [EXPORTH] =   { "export",   "[-fn] [name[=value]]",   exporthelp      },
  [FALSEH] =    { "false",    " ",                      falsehelp       },
  [FORH] =      { "for",      forusg,                   forhelp         },
  [FGH] =       { "fg",       "[job]",                  fghelp          },
  [GETOPTSH] =  { "getopts",  "optstring name [arg]",   getoptshelp     },
  [HASHH] =     { "hash",     "[-r] [pathname][name]",  hashhelp        },
  [HELPH] =     { "help",     "[builtin]",              helphelp        },
  [IFH] =       { "if",       ifusg,                    ifhelp          },
  [JOBSH] =     { "jobs",     "[job]",                  jobshelp        },
  [KILLH] =     { "kill",     killusg,                  killhelp        },
  [LOCALH] =    { "local",    "name[=value]",           localhelp       },
  [PWDH] =      { "pwd",      "[-LP]",                  pwdhelp         },
  [PRINTFH] =   { "printf",   "format [arg...]",        printhelp       },
  [READH] =     { "read",     "[-p prompt][-r] var",    readhelp        },
  [READONLYH] = { "readonly", "name[=value]",           readonlyhelp    },
  [RETURNH] =   { "return",   "[n]",                    returnhelp      },
  [SETH] =      { "set",      setusg,                   sethelp         },
  [SHIFTH] =    { "shift",    "[n]",                    shifthelp       },
  [TESTH] =     { "test",     "[expr]",                 testhelp        },
  [TIMESH] =    { "times",    " ",                      timeshelp       },
  [TRAPH] =     { "trap",     "[-lp] [arg] [signal]",   traphelp        },
  [TRUEH] =     { "true",     " ",                      truehelp        },
  [TYPEH] =     { "type",     "name [name ...]",        typehelp        },
  [ULIMITH] =   { "ulimit",   ulimitusg,                ulimithelp      },
  [UMASKH] =    { "umask",    "[-S] mode",              umaskhelp       },
  [UNTILH] =    { "until",    untilusg,                 untilhelp       },
  [UNALIASH] =  { "unalias",  "name",                   unaliashelp     },
  [UNSETH] =    { "unset",    "[-fv] name",             unsethelp       },
  [WAITH] =     { "wait",     "[id ...]",               waitforhelp     },
  [WHILEH] =    { "while",    whileusg,                 whilehelp       },
  [BRACEH] =    { " { ",      "COMMANDS ;}",            bracehelp       },
#ifdef ENABLE_BASENAME
  [BASENAMEH] = { "basename", "name [suffix]",          basenamehelp    },
#endif
#ifdef ENABLE_CAT
  [CATH] =      { "cat",      "[file ...]",             cathelp         },
#endif
#ifdef ENABLE_CUT
  [CUTH] =      { "cut",      cutusg,                   cuthelp         },
#endif
#ifdef ENABLE_DIRNAME
  [DIRNAMEH] =  { "dirname",  "name",                   dirnamehelp     },
#endif
#ifdef ENABLE_HEAD
  [HEADH] =     { "head",     "[-n count] [file ...]",  headhelp        },
#endif
#ifdef ENABLE_READLINK
  [READLINKH] = { "readlink", "[-n] file",              readlinkhelp    },
#endif
#ifdef ENABLE_REALPATH
  [REALPATHH] = { "realpath", "[-n] file",              realpathhelp    },
#endif
#ifdef ENABLE_SLEEP
  [SLEEPH] =    { "sleep",    "[n[.n][smhd]]",          sleephelp       },
#endif
#ifdef ENABLE_TAIL
  [TAILH] =     { "tail",     "[-f] [-n count] [file]", tailhelp        },
#endif
#ifdef ENABLE_TEE
  [TEEH] =      { "tee",      "[OPTION]... [FILE]...",  teehelp         },
#endif
#ifdef ENABLE_WC
  [WCH] =      { "wc",        "[OPTION]... [FILE]...",  wchelp         },
#endif
}; /* clang-format on */

int
helpcmd(char **argv)
{
  size_t argc = 0, n = (sizeof(helpmsgs) / sizeof(builtinhelp));
  const builtinhelp *h = helpmsgs;
  int status = 0;

  array_len(argv, argc);
  if (argc < 2) {
    printf("simpsh version (pre alpha) - "
           "https://codeberg.org/someoneelse/simpsh.git\n\n"
           "These are the builtin commands included with simpsh:\n");
    for (size_t i = 0; i < n; i++) {
      printf("%s - %s %s \n", h[i].name, h[i].name,
             h[i].usage);
    }
  } else {
    for (size_t i = 1; i < argc; i++) {
      int fnd = 0;
      for (size_t j = 0; j < n; j++) {
        if (strcmp(helpmsgs[j].name, argv[i]) == 0) {
          printf("%s: %s\n", h[j].name, h[j].help);
          fnd = 1;
          break;
        }
      }
      if (!fnd) {
        fprintf(stderr, "%s: %s: no help topics match '%s'\n", SHARGV0, argv[0], argv[i]);
        status = 1;
        break;
      }
    }
  }

  return status;
}
