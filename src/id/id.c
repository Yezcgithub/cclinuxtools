/**
 * @file id.c
 * @brief Cross-platform implementation of the coreutils id command
 * @author Yezc
 * @note encoding utf-8
 *
 * Reimplemented in portable C99 for Windows, Linux, macOS,
 * FreeBSD, OpenBSD, NetBSD, and other Unix-like systems.
 * Behavior is compatible with coreutils id(1) (coreutils 9.11+).
 *
 * Key behaviors:
 *   - -u, --user:     print only the effective user ID
 *   - -g, --group:    print only the effective group ID
 *   - -G, --groups:   print all group IDs
 *   - -n, --name:     print names instead of numbers (with -u, -g, -G)
 *   - -r, --real:     print real ID instead of effective ID
 *   - -z, --zero:     delimit entries with NUL instead of whitespace
 *   - -a:             ignored (for compatibility)
 *   - --help / --version: display help or version information
 *   - With no options, prints full identity in default format:
 *       uid=1000(user) gid=1000(user) groups=1000(user),4(adm),...
 *
 * Build (Windows):  gcc -O2 -std=c99 -Wall -o id.exe id.c
 * Build (Linux):    gcc -O2 -std=c99 -Wall -D_POSIX_C_SOURCE=200809L -o id id.c
 * Build (macOS):    gcc -O2 -std=c99 -Wall -D_DARWIN_C_SOURCE -o id id.c
 * Build (FreeBSD):  cc -O2 -std=c99 -Wall -o id id.c
 * Build (OpenBSD):  cc -O2 -std=c99 -Wall -o id id.c
 * Build (NetBSD):   cc -O2 -std=c99 -Wall -D_NETBSD_SOURCE -o id id.c
 *
 * ============================
 * - license
 * ============================
 * - MIT
 * https://mit-license.org/
 *
 * Copyright (C) 2025-2026 <Yezc/id>
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 * and associated documentation files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 * BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * Platform detection macros — must appear before any system includes
 * so that POSIX feature macros are defined correctly.
 */
#if defined(_WIN32) || defined(_WIN64)
    #define ID_PLATFORM_WINDOWS 1
#elif defined(__linux__)
    #define ID_PLATFORM_LINUX   1
    #define ID_PLATFORM_POSIX   1
#elif defined(__APPLE__) && defined(__MACH__)
    #define ID_PLATFORM_MACOS   1
    #define ID_PLATFORM_POSIX   1
#elif defined(__FreeBSD__)
    #define ID_PLATFORM_FREEBSD 1
    #define ID_PLATFORM_POSIX   1
#elif defined(__OpenBSD__)
    #define ID_PLATFORM_OPENBSD 1
    #define ID_PLATFORM_POSIX   1
#elif defined(__NetBSD__)
    #define ID_PLATFORM_NETBSD  1
    #define ID_PLATFORM_POSIX   1
#elif defined(__unix__) || defined(__unix)
    #define ID_PLATFORM_POSIX   1
#else
    #define ID_PLATFORM_POSIX   1
#endif

/* POSIX feature macros — must be defined before including any headers */
#ifdef ID_PLATFORM_LINUX
    #ifndef _GNU_SOURCE
        #define _GNU_SOURCE
    #endif
    #ifndef _POSIX_C_SOURCE
        #define _POSIX_C_SOURCE 200809L
    #endif
#endif

#ifdef ID_PLATFORM_MACOS
    #ifndef _DARWIN_C_SOURCE
        #define _DARWIN_C_SOURCE
    #endif
#endif

#ifdef ID_PLATFORM_NETBSD
    #ifndef _NETBSD_SOURCE
        #define _NETBSD_SOURCE
    #endif
#endif

/********************************
 *    includes
 ********************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include <errno.h>

#ifdef ID_PLATFORM_WINDOWS
    #include <fcntl.h>
    #include <io.h>
    #include <windows.h>
    #include <lm.h>
    #include <sddl.h>
#else
    #include <unistd.h>
    #include <sys/types.h>
    #include <pwd.h>
    #include <grp.h>
#endif

/********************************
 *    defines
 ********************************/

/** @brief Version string */
#define ID_VERSION_STR "v1.0.0"

/** @brief Maximum number of groups to fetch */
#define ID_MAX_GROUPS 4096

/** @brief Buffer size for name lookups */
#define ID_NAME_BUF_SIZE 256

/********************************
 *    typedefs
 ********************************/

/**
 * @brief Options structure for id
 */
typedef struct {
    bool print_user;         /**< -u: print only effective user ID */
    bool print_group;        /**< -g: print only effective group ID */
    bool print_groups;       /**< -G: print all group IDs */
    bool print_name;         /**< -n: print names instead of numbers */
    bool print_real;         /**< -r: print real ID instead of effective */
    bool zero_delim;         /**< -z: delimit with NUL instead of space */
    bool compatibility_a;    /**< -a: ignored (compatibility) */
} id_opts;

/**
 * @brief Holds resolved user identity information
 */
typedef struct {
    unsigned long uid;          /**< User ID (real or effective) */
    unsigned long gid;          /**< Primary group ID (real or effective) */
    char user_name[ID_NAME_BUF_SIZE];   /**< User name */
    char group_name[ID_NAME_BUF_SIZE];  /**< Primary group name */
} id_identity;

/**
 * @brief A single group entry (ID + name)
 */
typedef struct {
    unsigned long gid;          /**< Group ID */
    char name[ID_NAME_BUF_SIZE]; /**< Group name */
} id_group_entry;

/********************************
 *    static prototypes
 ********************************/
static void         _id_print_help(void);
static void         _id_print_version(void);
static bool         _id_streq(const char * a, const char * b);

static bool         _id_get_current_identity(id_identity * real_id,
                                              id_identity * eff_id,
                                              id_group_entry * groups,
                                              int * group_count,
                                              bool want_real);
static bool         _id_get_named_identity(const char * username,
                                           id_identity * ident,
                                           id_group_entry * groups,
                                           int * group_count);

static int          _id_parse_options(int argc, char ** argv, id_opts * opts,
                                     const char ** username);
static int          _id_print_default(const id_identity * real_id,
                                      const id_identity * eff_id,
                                      const id_group_entry * groups,
                                      int group_count,
                                      bool zero_delim);
static int          _id_print_single(const id_identity * real_id,
                                     const id_identity * eff_id,
                                     const id_opts * opts);
static int          _id_print_all_groups(const id_group_entry * groups,
                                        int group_count,
                                        bool print_name,
                                        bool zero_delim);

/********************************
 *    macros
 ********************************/

/**
 * @brief Default output stream.
 *        Defaults to libc @c stdout .
 */
#ifndef id_out_stream
    #define id_out_stream stdout
#endif

/**
 * @brief Default error stream.
 *        Defaults to libc @c stderr .
 */
#ifndef id_err_stream
    #define id_err_stream stderr
#endif

/**
 * @brief Formatted print (printf-compatible).
 */
#ifndef id_printf
    #define id_printf(fmt, ...) printf(fmt, ##__VA_ARGS__)
#endif

/**
 * @brief Formatted print to error stream (fprintf-compatible).
 */
#ifndef id_err_printf
    #define id_err_printf(fmt, ...) fprintf(id_err_stream, fmt, ##__VA_ARGS__)
#endif

/**
 * @brief Write a single character to the output stream.
 */
#ifndef id_putchar
    #define id_putchar(ch) (void)putchar((int)(unsigned char)(ch))
#endif

/**
 * @brief Flush a stdio stream's output buffer.
 */
#ifndef id_fflush
    #define id_fflush(stream) (void)fflush(stream)
#endif

/********************************
 *    static variables
 ********************************/

/* (none) */

/********************************
 *    global functions
 ********************************/

/**
 * @brief Entry point for the id command
 *
 * Processing flow:
 *   1. Parse command-line options
 *   2. Retrieve identity (current user or named user)
 *   3. Print requested information based on options
 *
 * @param argc  argument count
 * @param argv  argument vector
 * @return 0 on success, 1 on error
 */
int main(int argc, char ** argv)
{
#ifdef ID_PLATFORM_WINDOWS
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
    _setmode(_fileno(stderr), _O_BINARY);
#endif

    id_opts opts;
    memset(&opts, 0, sizeof(opts));

    const char * username = NULL;

    int ret = _id_parse_options(argc, argv, &opts, &username);
    if (ret != 0) {
        return (ret < 0) ? 0 : 1;
    }

    /* Validate option combinations */
    if (opts.print_name && !opts.print_user && !opts.print_group && !opts.print_groups) {
        id_err_printf("id: --name option requires --user, --group, or --groups\n");
        id_err_printf("Try 'id --help' for more information.\n");
        return 1;
    }

    if (opts.print_real && !opts.print_user && !opts.print_group && !opts.print_groups) {
        id_err_printf("id: --real option requires --user, --group, or --groups\n");
        id_err_printf("Try 'id --help' for more information.\n");
        return 1;
    }

    id_identity real_id;
    id_identity eff_id;
    id_group_entry groups[ID_MAX_GROUPS];
    int group_count = 0;

    memset(&real_id, 0, sizeof(real_id));
    memset(&eff_id, 0, sizeof(eff_id));

    bool ok;
    if (username) {
        ok = _id_get_named_identity(username, &eff_id, groups, &group_count);
        if (!ok) {
            id_err_printf("id: '%s': no such user\n", username);
            return 1;
        }
        real_id = eff_id;
    }
    else {
        ok = _id_get_current_identity(&real_id, &eff_id, groups, &group_count,
                                      opts.print_real);
        if (!ok) {
            id_err_printf("id: cannot retrieve identity information\n");
            return 1;
        }
    }

    int exit_code;
    if (opts.print_user || opts.print_group) {
        exit_code = _id_print_single(&real_id, &eff_id, &opts);
    }
    else if (opts.print_groups) {
        exit_code = _id_print_all_groups(groups, group_count,
                                         opts.print_name, opts.zero_delim);
    }
    else {
        exit_code = _id_print_default(&real_id, &eff_id, groups,
                                      group_count, opts.zero_delim);
    }

    id_fflush(id_out_stream);
    return exit_code;
}

/********************************
 *    static functions
 ********************************/

/**
 * @brief Parse command-line options for id
 *
 * @param argc     argument count
 * @param argv     argument vector
 * @param opts     output options struct
 * @param username output: username if specified (NULL otherwise)
 * @return 0 on success, -1 if help/version was printed (exit 0),
 *         1 on error
 */
static int _id_parse_options(int argc, char ** argv, id_opts * opts,
                             const char ** username)
{
    *username = NULL;

    for (int i = 1; i < argc; i++) {
        const char * arg = argv[i];

        if (_id_streq(arg, "--")) {
            if (i + 1 < argc) {
                *username = argv[i + 1];
            }
            return 0;
        }

        /* Long options */
        if (arg[0] == '-' && arg[1] == '-') {
            if (_id_streq(arg, "--help")) {
                _id_print_help();
                return -1;
            }
            if (_id_streq(arg, "--version")) {
                _id_print_version();
                return -1;
            }
            if (_id_streq(arg, "--user")) {
                opts->print_user = true;
                continue;
            }
            if (_id_streq(arg, "--group")) {
                opts->print_group = true;
                continue;
            }
            if (_id_streq(arg, "--groups")) {
                opts->print_groups = true;
                continue;
            }
            if (_id_streq(arg, "--name")) {
                opts->print_name = true;
                continue;
            }
            if (_id_streq(arg, "--real")) {
                opts->print_real = true;
                continue;
            }
            if (_id_streq(arg, "--zero")) {
                opts->zero_delim = true;
                continue;
            }

            id_err_printf("id: unrecognized option '%s'\n", arg);
            id_err_printf("Try 'id --help' for more information.\n");
            return 1;
        }

        /* Short options */
        if (arg[0] == '-' && arg[1] != '\0') {
            const char * p = arg + 1;
            while (*p) {
                switch (*p) {
                    case 'u':
                        opts->print_user = true;
                        p++;
                        break;

                    case 'g':
                        opts->print_group = true;
                        p++;
                        break;

                    case 'G':
                        opts->print_groups = true;
                        p++;
                        break;

                    case 'n':
                        opts->print_name = true;
                        p++;
                        break;

                    case 'r':
                        opts->print_real = true;
                        p++;
                        break;

                    case 'z':
                        opts->zero_delim = true;
                        p++;
                        break;

                    case 'a':
                        opts->compatibility_a = true;
                        p++;
                        break;

                    default:
                        id_err_printf("id: invalid option -- '%c'\n", *p);
                        id_err_printf("Try 'id --help' for more information.\n");
                        return 1;
                }
            }
            continue;
        }

        /* Not an option — treat as username */
        *username = arg;

        /* If there are more args after the username, it's an error */
        if (i + 1 < argc) {
            id_err_printf("id: extra operand '%s'\n", argv[i + 1]);
            id_err_printf("Try 'id --help' for more information.\n");
            return 1;
        }
        break;
    }

    return 0;
}

/**
 * @brief Print the default format output:
 *        uid=X(name) gid=Y(group) groups=Z(g1),W(g2),...
 *
 * @param real_id     real identity
 * @param eff_id      effective identity
 * @param groups      supplementary groups
 * @param group_count number of supplementary groups
 * @param zero_delim  use NUL delimiter
 * @return 0 on success
 */
static int _id_print_default(const id_identity * real_id,
                             const id_identity * eff_id,
                             const id_group_entry * groups,
                             int group_count,
                             bool zero_delim)
{
    const id_identity * uid_src = eff_id;
    const id_identity * gid_src = eff_id;

    /* Determine which identity to use for uid/gid display.
     * In default mode, GNU id shows effective IDs (matching coreutils behavior). */
    (void)real_id;

    /* uid=X(name) */
    id_printf("uid=%lu", uid_src->uid);
    if (uid_src->user_name[0] != '\0') {
        id_printf("(%s)", uid_src->user_name);
    }

    /* gid=Y(group) */
    id_printf(" gid=%lu", gid_src->gid);
    if (gid_src->group_name[0] != '\0') {
        id_printf("(%s)", gid_src->group_name);
    }

    /* groups=Z(g1),W(g2),... */
    if (group_count > 0) {
        id_printf(" groups=");
        for (int i = 0; i < group_count; i++) {
            if (i > 0) {
                id_putchar(',');
            }
            id_printf("%lu", groups[i].gid);
            if (groups[i].name[0] != '\0') {
                id_printf("(%s)", groups[i].name);
            }
        }
    }

    if (zero_delim) {
        id_putchar('\0');
    }
    else {
        id_putchar('\n');
    }

    return 0;
}

/**
 * @brief Print a single ID (user or group) based on options
 *
 * @param real_id  real identity
 * @param eff_id   effective identity
 * @param opts     options (print_user, print_group, print_name, print_real)
 * @return 0 on success
 */
static int _id_print_single(const id_identity * real_id,
                           const id_identity * eff_id,
                           const id_opts * opts)
{
    const id_identity * src = opts->print_real ? real_id : eff_id;

    if (opts->print_user && opts->print_group) {
        /* Both -u and -g: print uid then gid */
        if (opts->print_name) {
            if (src->user_name[0] != '\0') {
                id_printf("%s", src->user_name);
            }
            else {
                id_printf("%lu", src->uid);
            }
            if (opts->zero_delim) {
                id_putchar('\0');
            }
            else {
                id_putchar(' ');
            }
            if (src->group_name[0] != '\0') {
                id_printf("%s", src->group_name);
            }
            else {
                id_printf("%lu", src->gid);
            }
        }
        else {
            id_printf("%lu", src->uid);
            if (opts->zero_delim) {
                id_putchar('\0');
            }
            else {
                id_putchar(' ');
            }
            id_printf("%lu", src->gid);
        }
    }
    else if (opts->print_user) {
        if (opts->print_name) {
            if (src->user_name[0] != '\0') {
                id_printf("%s", src->user_name);
            }
            else {
                id_printf("%lu", src->uid);
            }
        }
        else {
            id_printf("%lu", src->uid);
        }
    }
    else {
        /* print_group only */
        if (opts->print_name) {
            if (src->group_name[0] != '\0') {
                id_printf("%s", src->group_name);
            }
            else {
                id_printf("%lu", src->gid);
            }
        }
        else {
            id_printf("%lu", src->gid);
        }
    }

    if (opts->zero_delim) {
        id_putchar('\0');
    }
    else {
        id_putchar('\n');
    }

    return 0;
}

/**
 * @brief Print all group IDs (-G mode)
 *
 * @param groups      group entries
 * @param group_count number of groups
 * @param print_name  print names instead of numbers
 * @param zero_delim  use NUL delimiter
 * @return 0 on success
 */
static int _id_print_all_groups(const id_group_entry * groups,
                               int group_count,
                               bool print_name,
                               bool zero_delim)
{
    char delim = zero_delim ? '\0' : ' ';

    for (int i = 0; i < group_count; i++) {
        if (i > 0) {
            id_putchar(delim);
        }
        if (print_name) {
            if (groups[i].name[0] != '\0') {
                id_printf("%s", groups[i].name);
            }
            else {
                id_printf("%lu", groups[i].gid);
            }
        }
        else {
            id_printf("%lu", groups[i].gid);
        }
    }

    if (zero_delim) {
        id_putchar('\0');
    }
    else {
        id_putchar('\n');
    }

    return 0;
}

/* ==================== Platform-specific implementations ==================== */

#ifdef ID_PLATFORM_WINDOWS

/**
 * @brief Extract the RID (last sub-authority) from a SID as a numeric ID.
 * @param sid  Pointer to a SID structure
 * @return Numeric ID (RID), or 0 on failure
 */
static unsigned long _id_sid_to_rid(PSID sid)
{
    if (!sid || !IsValidSid(sid)) {
        return 0;
    }
    PUCHAR count_ptr = GetSidSubAuthorityCount(sid);
    if (!count_ptr || *count_ptr == 0) {
        return 0;
    }
    DWORD * sub_auth = GetSidSubAuthority(sid, (DWORD)(*count_ptr - 1));
    if (!sub_auth) {
        return 0;
    }
    return (unsigned long)*sub_auth;
}

/**
 * @brief Lookup a name for a SID (account name).
 * @param sid       SID to look up
 * @param buf       output buffer
 * @param buf_size  buffer size
 * @return true on success
 */
static bool _id_sid_to_name(PSID sid, char * buf, size_t buf_size)
{
    if (!sid || !buf || buf_size == 0) {
        return false;
    }
    buf[0] = '\0';

    char name[ID_NAME_BUF_SIZE];
    char domain[ID_NAME_BUF_SIZE];
    DWORD name_len = ID_NAME_BUF_SIZE;
    DWORD domain_len = ID_NAME_BUF_SIZE;
    SID_NAME_USE use;

    if (!LookupAccountSidA(NULL, sid, name, &name_len, domain, &domain_len, &use)) {
        return false;
    }
    if (name_len == 0) {
        return false;
    }

    /* Only copy the account name, not the domain (matches GNU id behavior) */
    if (strlen(name) >= buf_size) {
        return false;
    }
    strcpy(buf, name);
    return true;
}

/**
 * @brief Get the current process token SID (user or primary group).
 * @param token_info_class  TokenUser or TokenPrimaryGroup
 * @param sid_out           output SID pointer (points into allocated buffer)
 * @param token_buf         buffer to hold token info (caller must free)
 * @return true on success
 */
static bool _id_get_token_sid(TOKEN_INFORMATION_CLASS token_info_class,
                              PSID * sid_out, unsigned char ** token_buf)
{
    *sid_out = NULL;
    *token_buf = NULL;

    HANDLE token;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        return false;
    }

    DWORD ret_len = 0;
    GetTokenInformation(token, token_info_class, NULL, 0, &ret_len);
    if (ret_len == 0) {
        CloseHandle(token);
        return false;
    }

    unsigned char * buf = (unsigned char *)malloc(ret_len);
    if (!buf) {
        CloseHandle(token);
        return false;
    }

    if (!GetTokenInformation(token, token_info_class, buf, ret_len, &ret_len)) {
        free(buf);
        CloseHandle(token);
        return false;
    }

    CloseHandle(token);

    if (token_info_class == TokenUser) {
        TOKEN_USER * tu = (TOKEN_USER *)buf;
        *sid_out = tu->User.Sid;
    }
    else {
        TOKEN_PRIMARY_GROUP * tpg = (TOKEN_PRIMARY_GROUP *)buf;
        *sid_out = tpg->PrimaryGroup;
    }

    *token_buf = buf;
    return true;
}

/**
 * @brief Retrieve current user identity and group list on Windows.
 *
 * Uses the process token to get the user SID (as UID), the primary group SID
 * (as GID), and the token group SIDs (as supplementary groups).
 *
 * @param real_id    output: real identity (same as effective on Windows)
 * @param eff_id     output: effective identity
 * @param groups     output: supplementary groups array
 * @param group_count output: number of supplementary groups
 * @param want_real  whether real IDs are requested (ignored on Windows,
 *                   real == effective)
 * @return true on success
 */
static bool _id_get_current_identity(id_identity * real_id,
                                     id_identity * eff_id,
                                     id_group_entry * groups,
                                     int * group_count,
                                     bool want_real)
{
    (void)want_real;

    /* Get user SID from token */
    PSID user_sid = NULL;
    unsigned char * user_buf = NULL;
    if (!_id_get_token_sid(TokenUser, &user_sid, &user_buf)) {
        return false;
    }

    unsigned long uid = _id_sid_to_rid(user_sid);
    char user_name[ID_NAME_BUF_SIZE];
    user_name[0] = '\0';
    _id_sid_to_name(user_sid, user_name, ID_NAME_BUF_SIZE);

    /* Get primary group SID from token */
    PSID pg_sid = NULL;
    unsigned char * pg_buf = NULL;
    unsigned long gid = 0;
    char group_name[ID_NAME_BUF_SIZE];
    group_name[0] = '\0';

    if (_id_get_token_sid(TokenPrimaryGroup, &pg_sid, &pg_buf)) {
        gid = _id_sid_to_rid(pg_sid);
        _id_sid_to_name(pg_sid, group_name, ID_NAME_BUF_SIZE);
    }

    eff_id->uid = uid;
    eff_id->gid = gid;
    strncpy(eff_id->user_name, user_name, ID_NAME_BUF_SIZE - 1);
    eff_id->user_name[ID_NAME_BUF_SIZE - 1] = '\0';
    strncpy(eff_id->group_name, group_name, ID_NAME_BUF_SIZE - 1);
    eff_id->group_name[ID_NAME_BUF_SIZE - 1] = '\0';

    *real_id = *eff_id;

    /* Get supplementary groups from token */
    *group_count = 0;

    HANDLE token;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        free(user_buf);
        if (pg_buf) {
            free(pg_buf);
        }
        return true; /* user/group info was retrieved */
    }

    DWORD ret_len = 0;
    GetTokenInformation(token, TokenGroups, NULL, 0, &ret_len);
    if (ret_len > 0) {
        unsigned char * grp_buf = (unsigned char *)malloc(ret_len);
        if (grp_buf) {
            if (GetTokenInformation(token, TokenGroups, grp_buf, ret_len, &ret_len)) {
                TOKEN_GROUPS * tg = (TOKEN_GROUPS *)grp_buf;
                for (DWORD i = 0; i < tg->GroupCount && *group_count < ID_MAX_GROUPS; i++) {
                    if (tg->Groups[i].Attributes & SE_GROUP_ENABLED) {
                        PSID sid = tg->Groups[i].Sid;
                        unsigned long g = _id_sid_to_rid(sid);
                        if (g != gid) {
                            groups[*group_count].gid = g;
                            groups[*group_count].name[0] = '\0';
                            _id_sid_to_name(sid,
                                           groups[*group_count].name,
                                           ID_NAME_BUF_SIZE);
                            (*group_count)++;
                        }
                    }
                }
            }
            free(grp_buf);
        }
    }

    CloseHandle(token);
    free(user_buf);
    if (pg_buf) {
        free(pg_buf);
    }
    return true;
}

/**
 * @brief Retrieve identity for a named user on Windows.
 *
 * Uses LookupAccountName to look up the user SID and extract the RID as UID.
 * Local groups are enumerated via NetUserGetLocalGroups (requires wide strings).
 *
 * @param username    user name to look up
 * @param ident       output: identity
 * @param groups      output: supplementary groups
 * @param group_count output: number of groups
 * @return true on success, false if user not found
 */
static bool _id_get_named_identity(const char * username,
                                   id_identity * ident,
                                   id_group_entry * groups,
                                   int * group_count)
{
    if (!username || !ident || !groups || !group_count) {
        return false;
    }

    memset(ident, 0, sizeof(*ident));
    *group_count = 0;

    /* Lookup the account SID by name */
    char domain_buf[ID_NAME_BUF_SIZE];
    DWORD domain_len = ID_NAME_BUF_SIZE;
    SID_NAME_USE use;
    PSID sid = NULL;
    DWORD sid_len = 0;

    /* First call to get SID size */
    LookupAccountNameA(NULL, username, NULL, &sid_len, domain_buf, &domain_len, &use);
    if (sid_len == 0) {
        return false;
    }

    sid = (PSID)malloc(sid_len);
    if (!sid) {
        return false;
    }

    domain_len = ID_NAME_BUF_SIZE;
    if (!LookupAccountNameA(NULL, username, sid, &sid_len,
                           domain_buf, &domain_len, &use)) {
        free(sid);
        return false;
    }

    unsigned long uid = _id_sid_to_rid(sid);
    strncpy(ident->user_name, username, ID_NAME_BUF_SIZE - 1);
    ident->user_name[ID_NAME_BUF_SIZE - 1] = '\0';
    ident->uid = uid;

    /* For named users on Windows, use the user SID RID as gid too
     * (no reliable way to get primary group without the token) */
    ident->gid = uid;
    strncpy(ident->group_name, username, ID_NAME_BUF_SIZE - 1);
    ident->group_name[ID_NAME_BUF_SIZE - 1] = '\0';

    /* Convert username to wide string for NetUserGetLocalGroups */
    wchar_t wusername[ID_NAME_BUF_SIZE];
    MultiByteToWideChar(CP_ACP, 0, username, -1, wusername, ID_NAME_BUF_SIZE);

    /* Try to get local groups for this user */
    LOCALGROUP_USERS_INFO_0 * lg = NULL;
    DWORD entries_read = 0;
    DWORD total_entries = 0;

    NET_API_STATUS status = NetUserGetLocalGroups(NULL, wusername, 0,
                                                  LG_INCLUDE_INDIRECT,
                                                  (LPBYTE *)&lg,
                                                  MAX_PREFERRED_LENGTH,
                                                  &entries_read, &total_entries);
    if (status == NERR_Success && lg) {
        for (DWORD i = 0; i < entries_read && *group_count < ID_MAX_GROUPS; i++) {
            char gname[ID_NAME_BUF_SIZE];
            WideCharToMultiByte(CP_ACP, 0, lg[i].lgrui0_name, -1,
                               gname, ID_NAME_BUF_SIZE, NULL, NULL);

            char g_domain[ID_NAME_BUF_SIZE];
            DWORD g_domain_len = ID_NAME_BUF_SIZE;
            SID_NAME_USE g_use;
            PSID g_sid = NULL;
            DWORD g_sid_len = 0;

            LookupAccountNameA(NULL, gname, NULL, &g_sid_len,
                             g_domain, &g_domain_len, &g_use);
            if (g_sid_len > 0) {
                g_sid = (PSID)malloc(g_sid_len);
                if (g_sid) {
                    g_domain_len = ID_NAME_BUF_SIZE;
                    if (LookupAccountNameA(NULL, gname, g_sid, &g_sid_len,
                                          g_domain, &g_domain_len, &g_use)) {
                        groups[*group_count].gid = _id_sid_to_rid(g_sid);
                        strncpy(groups[*group_count].name, gname,
                               ID_NAME_BUF_SIZE - 1);
                        groups[*group_count].name[ID_NAME_BUF_SIZE - 1] = '\0';
                        (*group_count)++;
                    }
                    free(g_sid);
                }
            }
        }
        NetApiBufferFree(lg);
    }

    free(sid);
    return true;
}

#else /* POSIX platforms */

/**
 * @brief Retrieve current user identity and group list on POSIX systems.
 *
 * Uses getuid/geteuid/getgid/getegid for real/effective IDs,
 * getpwuid/getgrgid for name lookups, and getgroups() for supplementary
 * groups.
 *
 * @param real_id    output: real identity
 * @param eff_id     output: effective identity
 * @param groups     output: supplementary groups array
 * @param group_count output: number of supplementary groups
 * @param want_real  whether real IDs are needed (always populates both)
 * @return true on success
 */
static bool _id_get_current_identity(id_identity * real_id,
                                     id_identity * eff_id,
                                     id_group_entry * groups,
                                     int * group_count,
                                     bool want_real)
{
    (void)want_real;

    memset(real_id, 0, sizeof(*real_id));
    memset(eff_id, 0, sizeof(*eff_id));
    *group_count = 0;

    uid_t ruid = getuid();
    uid_t euid = geteuid();
    gid_t rgid = getgid();
    gid_t egid = getegid();

    real_id->uid = (unsigned long)ruid;
    real_id->gid = (unsigned long)rgid;
    eff_id->uid = (unsigned long)euid;
    eff_id->gid = (unsigned long)egid;

    /* Lookup names for real IDs */
    struct passwd * pw = getpwuid(ruid);
    if (pw) {
        strncpy(real_id->user_name, pw->pw_name, ID_NAME_BUF_SIZE - 1);
        real_id->user_name[ID_NAME_BUF_SIZE - 1] = '\0';
    }

    struct group * gr = getgrgid(rgid);
    if (gr) {
        strncpy(real_id->group_name, gr->gr_name, ID_NAME_BUF_SIZE - 1);
        real_id->group_name[ID_NAME_BUF_SIZE - 1] = '\0';
    }

    /* Lookup names for effective IDs */
    if (euid != ruid) {
        pw = getpwuid(euid);
    }
    if (pw) {
        strncpy(eff_id->user_name, pw->pw_name, ID_NAME_BUF_SIZE - 1);
        eff_id->user_name[ID_NAME_BUF_SIZE - 1] = '\0';
    }

    if (egid != rgid) {
        gr = getgrgid(egid);
    }
    if (gr) {
        strncpy(eff_id->group_name, gr->gr_name, ID_NAME_BUF_SIZE - 1);
        eff_id->group_name[ID_NAME_BUF_SIZE - 1] = '\0';
    }

    /* Get supplementary groups */
    gid_t gid_list[ID_MAX_GROUPS];
    int n = getgroups(ID_MAX_GROUPS, gid_list);
    if (n < 0) {
        n = 0;
    }

    /* Ensure the effective gid is included in the groups list */
    bool gid_found = false;
    for (int i = 0; i < n; i++) {
        if (gid_list[i] == egid) {
            gid_found = true;
            break;
        }
    }

    int idx = 0;
    if (!gid_found && idx < ID_MAX_GROUPS) {
        groups[idx].gid = (unsigned long)egid;
        if (gr && egid == rgid) {
            strncpy(groups[idx].name, gr->gr_name, ID_NAME_BUF_SIZE - 1);
        }
        else {
            struct group * g2 = getgrgid(egid);
            if (g2) {
                strncpy(groups[idx].name, g2->gr_name, ID_NAME_BUF_SIZE - 1);
            }
            else {
                groups[idx].name[0] = '\0';
            }
        }
        groups[idx].name[ID_NAME_BUF_SIZE - 1] = '\0';
        idx++;
    }

    for (int i = 0; i < n && idx < ID_MAX_GROUPS; i++) {
        groups[idx].gid = (unsigned long)gid_list[i];
        struct group * g = getgrgid(gid_list[i]);
        if (g) {
            strncpy(groups[idx].name, g->gr_name, ID_NAME_BUF_SIZE - 1);
            groups[idx].name[ID_NAME_BUF_SIZE - 1] = '\0';
        }
        else {
            groups[idx].name[0] = '\0';
        }
        idx++;
    }

    *group_count = idx;
    return true;
}

/**
 * @brief Retrieve identity for a named user on POSIX systems.
 *
 * Uses getpwnam for user lookup and getgrouplist (or manual iteration
 * through /etc/group) for supplementary groups.
 *
 * @param username    user name to look up
 * @param ident       output: identity
 * @param groups      output: supplementary groups
 * @param group_count output: number of groups
 * @return true on success, false if user not found
 */
static bool _id_get_named_identity(const char * username,
                                   id_identity * ident,
                                   id_group_entry * groups,
                                   int * group_count)
{
    if (!username || !ident || !groups || !group_count) {
        return false;
    }

    memset(ident, 0, sizeof(*ident));
    *group_count = 0;

    struct passwd * pw = getpwnam(username);
    if (!pw) {
        return false;
    }

    ident->uid = (unsigned long)pw->pw_uid;
    ident->gid = (unsigned long)pw->pw_gid;
    strncpy(ident->user_name, pw->pw_name, ID_NAME_BUF_SIZE - 1);
    ident->user_name[ID_NAME_BUF_SIZE - 1] = '\0';

    struct group * gr = getgrgid(pw->pw_gid);
    if (gr) {
        strncpy(ident->group_name, gr->gr_name, ID_NAME_BUF_SIZE - 1);
        ident->group_name[ID_NAME_BUF_SIZE - 1] = '\0';
    }

    /* Get supplementary groups using getgrouplist.
     * This function is available on Linux, macOS, FreeBSD, OpenBSD, NetBSD. */
    gid_t gid_list[ID_MAX_GROUPS];
    int ngroups = ID_MAX_GROUPS;

    /* getgrouplist needs the primary gid and the user name */
    gid_t primary_gid = pw->pw_gid;

#if defined(ID_PLATFORM_MACOS) || defined(__APPLE__)
    /* On macOS, getgrouplist takes int* for ngroups */
    int ng = ngroups;
    if (getgrouplist(username, primary_gid, gid_list, &ng) >= 0) {
        ngroups = ng;
    }
    else {
        ngroups = ng; /* may be truncated, use what we got */
    }
#elif defined(ID_PLATFORM_LINUX) || defined(__linux__) || \
      defined(ID_PLATFORM_FREEBSD) || defined(__FreeBSD__) || \
      defined(ID_PLATFORM_OPENBSD) || defined(__OpenBSD__) || \
      defined(ID_PLATFORM_NETBSD) || defined(__NetBSD__)
    /* On Linux/BSD, getgrouplist takes int* for ngroups */
    int ng = ngroups;
    if (getgrouplist(username, primary_gid, gid_list, &ng) >= 0) {
        ngroups = ng;
    }
    else {
        ngroups = ng;
    }
#else
    /* Fallback: iterate through all groups via getgrent */
    ngroups = 0;
    setgrent();
    struct group * g;
    while ((g = getgrent()) != NULL && ngroups < ID_MAX_GROUPS) {
        if (!g->gr_mem) {
            continue;
        }
        for (char ** m = g->gr_mem; *m; m++) {
            if (strcmp(*m, username) == 0) {
                gid_list[ngroups++] = g->gr_gid;
                break;
            }
        }
    }
    endgrent();
    /* Always include the primary group */
    bool found = false;
    for (int i = 0; i < ngroups; i++) {
        if (gid_list[i] == primary_gid) {
            found = true;
            break;
        }
    }
    if (!found && ngroups < ID_MAX_GROUPS) {
        gid_list[ngroups++] = primary_gid;
    }
#endif

    /* Populate the groups array */
    for (int i = 0; i < ngroups && *group_count < ID_MAX_GROUPS; i++) {
        groups[*group_count].gid = (unsigned long)gid_list[i];
        struct group * g2 = getgrgid(gid_list[i]);
        if (g2) {
            strncpy(groups[*group_count].name, g2->gr_name,
                   ID_NAME_BUF_SIZE - 1);
            groups[*group_count].name[ID_NAME_BUF_SIZE - 1] = '\0';
        }
        else {
            groups[*group_count].name[0] = '\0';
        }
        (*group_count)++;
    }

    return true;
}

#endif /* ID_PLATFORM_WINDOWS */

/**
 * @brief Print usage/help information
 */
static void _id_print_help(void)
{
    id_printf(
        "Usage: id [OPTION]... [USER]...\n"
        "Print user and group information for each specified USER,\n"
        "or (when USER omitted) for the current user.\n"
        "\n"
        "  -a             ignore, for compatibility with other versions\n"
        "  -Z, --context  print only the security context of the process\n"
        "  -g, --group    print only the effective group ID\n"
        "  -G, --groups   print all group IDs\n"
        "  -n, --name     print a name instead of a number, for -ugG\n"
        "  -r, --real     print the real ID instead of the effective ID,\n"
        "                  with -ugG\n"
        "  -u, --user     print only the effective user ID\n"
        "  -z, --zero     delimit entries with NUL characters, not whitespace;\n"
        "                  not permitted with the default format\n"
        "      --help     display this help and exit\n"
        "      --version  output version information and exit\n"
        "\n"
        "Without any OPTION, print some useful set of identified information.\n"
        "\n"
        "NOTE: your shell may have its own version of id, which usually supersedes\n"
        "the version described here.  Please refer to your shell's documentation\n"
        "for details about the options it supports.\n"
    );
}

/**
 * @brief Print version information
 */
static void _id_print_version(void)
{
    id_printf("id %s\n", ID_VERSION_STR);
    id_printf("%s", "Copyright (C) 2025-2026 Yezc\n");
    id_printf("%s", "License MIT: <https://mit-license.org/>\n");
    id_printf("%s", "This is free software: you are free to change and redistribute it.\n");
    id_printf("%s", "There is NO WARRANTY, to the extent permitted by law.\n");
}

/**
 * @brief Compare two strings for equality (NULL-safe).
 * @param a  First string (may be NULL)
 * @param b  Second string (may be NULL)
 * @return true if strings are equal, false otherwise
 */
static bool _id_streq(const char * a, const char * b)
{
    if (!a || !b) {
        return (a == b);
    }
    return (strcmp(a, b) == 0);
}
