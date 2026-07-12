// =============================================================================
// Eclipse32 - Eclipse Shell (esh)
// =============================================================================
#include "shell.h"
#include "ins_pkg.h"
#include "../kernel/initramfs/initramfs.h"
#include "../kernel/drivers/vbe/vbe.h"
#include "../kernel/drivers/vga/vga.h"
#include "../kernel/drivers/keyboard/keyboard.h"
#include "../kernel/drivers/speaker/speaker.h"
#include "../kernel/drivers/disk/ata.h"
#include "../kernel/fs/fat32/fat32.h"
#include "../kernel/arch/x86/pit.h"
#include "../kernel/mm/heap.h"
#include "../kernel/exec/e32_loader.h"
#include "../kernel/syscall/syscall.h"
#include "../kernel/kernel.h"
#include "../kernel/net/net.h"
#include "../kernel/net/icmp.h"
#include "../kernel/net/dns.h"
#include "../kernel/net/http.h"
#include "../kernel/drivers/net/rtl8139.h"
#include "../kernel/sched/sched.h"

// =============================================================================
// Config
// =============================================================================
#define SHELL_MAX_LINE      1024
#define SHELL_MAX_ARGS      64
#define SHELL_MAX_HISTORY   128
#define SHELL_MAX_ALIASES   32
#define SHELL_MAX_VARS      64
#define SHELL_MAX_PATH      256
#define SHELL_SCRIPT_DEPTH  16

// =============================================================================
// State
// =============================================================================
typedef struct { char line[SHELL_MAX_LINE]; } history_entry_t;
typedef struct { char name[64]; char value[SHELL_MAX_LINE]; } shell_var_t;
typedef struct { char name[64]; char command[SHELL_MAX_LINE]; } alias_t;

static history_entry_t history[SHELL_MAX_HISTORY];
static int  history_count = 0;
static int  history_pos   = 0;

static shell_var_t shell_vars[SHELL_MAX_VARS];
static int  var_count  = 0;

static alias_t aliases[SHELL_MAX_ALIASES];
static int  alias_count = 0;

static char cwd[SHELL_MAX_PATH] = "/";
static int  last_exit_code      = 0;
static bool shell_running       = true;
static int  script_depth        = 0;

// =============================================================================
// Path resolution
// =============================================================================
static void path_resolve(const char *input, char *out) {
    char tmp[SHELL_MAX_PATH];

    if (input[0] == '/') {
        kstrncpy(tmp, input, SHELL_MAX_PATH - 1);
        tmp[SHELL_MAX_PATH - 1] = 0;
    } else {
        kstrncpy(tmp, cwd, SHELL_MAX_PATH - 1);
        int len = (int)kstrlen(tmp);
        if (len > 0 && tmp[len-1] != '/') { tmp[len++] = '/'; tmp[len] = 0; }
        kstrncpy(tmp + len, input, SHELL_MAX_PATH - 1 - len);
        tmp[SHELL_MAX_PATH - 1] = 0;
    }

    // Normalize: split on '/', handle . and ..
    char buf[SHELL_MAX_PATH];
    kstrncpy(buf, tmp, SHELL_MAX_PATH - 1);

    char *segs[64];
    int nseg = 0;
    char *p = buf;
    while (*p && nseg < 64) {
        if (*p == '/') { p++; continue; }
        segs[nseg++] = p;
        while (*p && *p != '/') p++;
        if (*p == '/') *p++ = 0;
    }

    char *stack[64];
    int top = 0;
    for (int i = 0; i < nseg; i++) {
        if (kstrcmp(segs[i], ".") == 0) continue;
        if (kstrcmp(segs[i], "..") == 0) { if (top > 0) top--; continue; }
        stack[top++] = segs[i];
    }

    out[0] = '/'; out[1] = 0;
    int tlen = 1;
    for (int i = 0; i < top; i++) {
        if (tlen > 1) out[tlen++] = '/';
        for (int j = 0; stack[i][j] && tlen < SHELL_MAX_PATH - 1; j++)
            out[tlen++] = stack[i][j];
        out[tlen] = 0;
    }
}

// =============================================================================
// Terminal output
// =============================================================================
static void (*g_term_cb)(const char*,void*) = 0;
static void *g_term_ud = 0;
static void term_putchar(char c) {
    if (g_term_cb) { char b[2]={c,0}; g_term_cb(b,g_term_ud); return; }
    vga_putchar(c);
}
static void term_puts(const char *s) { while (*s) term_putchar(*s++); }

typedef enum { TC_DEFAULT=0, TC_RED, TC_GREEN, TC_YELLOW, TC_CYAN, TC_WHITE, TC_BLUE } tc_t;

static void term_set_color(tc_t c) {
    uint8_t fg;
    switch (c) {
    case TC_RED:    fg = VGA_COLOR_LIGHT_RED;   break;
    case TC_GREEN:  fg = VGA_COLOR_LIGHT_GREEN; break;
    case TC_YELLOW: fg = VGA_COLOR_YELLOW;      break;
    case TC_CYAN:   fg = VGA_COLOR_LIGHT_CYAN;  break;
    case TC_WHITE:  fg = VGA_COLOR_WHITE;       break;
    case TC_BLUE:   fg = VGA_COLOR_LIGHT_BLUE;  break;
    default:        fg = VGA_COLOR_LIGHT_GREY;  break;
    }
    vga_set_color(fg, VGA_COLOR_BLACK);
}

static void term_reset(void) { vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK); }

static void tprintf(const char *fmt, ...) {
    char buf[SHELL_MAX_LINE];
    va_list args;
    va_start(args, fmt);
    int pos = 0;
    while (*fmt && pos < (int)sizeof(buf) - 1) {
        if (*fmt != '%') { buf[pos++] = *fmt++; continue; }
        fmt++;
        // Parse flags
        bool left_justify = false;
        if (*fmt == '-') { left_justify = true; fmt++; }
        char pad = ' ';
        if (*fmt == '0' && !left_justify) { pad = '0'; fmt++; }
        // Parse width
        int width = 0;
        while (*fmt >= '0' && *fmt <= '9') { width = width*10 + (*fmt-'0'); fmt++; }
        char tmp[32]; int len = 0;
        switch (*fmt) {
        case 'd': {
            int32_t n = va_arg(args, int32_t);
            bool neg = (n < 0); if (neg) n = -n;
            if (n == 0) tmp[len++] = '0';
            else { uint32_t u=(uint32_t)n; while(u&&len<31){tmp[len++]='0'+u%10;u/=10;} }
            for(int i=0;i<len/2;i++){char t=tmp[i];tmp[i]=tmp[len-1-i];tmp[len-1-i]=t;}
            if (neg && pos < (int)sizeof(buf)-1) buf[pos++] = '-';
            int printed = neg ? len+1 : len;
            if (!left_justify) while(printed<width&&pos<(int)sizeof(buf)-1){buf[pos++]=pad;printed++;}
            for(int i=0;i<len&&pos<(int)sizeof(buf)-1;i++) buf[pos++]=tmp[i];
            if (left_justify) while(printed<width&&pos<(int)sizeof(buf)-1){buf[pos++]=' ';printed++;}
            break;
        }
        case 'u': {
            uint32_t u = va_arg(args, uint32_t);
            if (u==0) tmp[len++]='0'; else while(u&&len<31){tmp[len++]='0'+u%10;u/=10;}
            for(int i=0;i<len/2;i++){char t=tmp[i];tmp[i]=tmp[len-1-i];tmp[len-1-i]=t;}
            if (!left_justify) while(len<width&&pos<(int)sizeof(buf)-1){buf[pos++]=pad;width--;}
            for(int i=0;i<len&&pos<(int)sizeof(buf)-1;i++) buf[pos++]=tmp[i];
            if (left_justify) { int p2=len; while(p2<width&&pos<(int)sizeof(buf)-1){buf[pos++]=' ';p2++;} }
            break;
        }
        case 'x': case 'X': {
            uint32_t u = va_arg(args, uint32_t); bool up=(*fmt=='X');
            if (u==0) tmp[len++]='0'; else while(u&&len<31){uint8_t d=u%16;tmp[len++]=d<10?'0'+d:(up?'A':'a')+d-10;u/=16;}
            for(int i=0;i<len/2;i++){char t=tmp[i];tmp[i]=tmp[len-1-i];tmp[len-1-i]=t;}
            if (!left_justify) while(len<width&&pos<(int)sizeof(buf)-1){buf[pos++]=pad;width--;}
            for(int i=0;i<len&&pos<(int)sizeof(buf)-1;i++) buf[pos++]=tmp[i];
            if (left_justify) { int p2=len; while(p2<width&&pos<(int)sizeof(buf)-1){buf[pos++]=' ';p2++;} }
            break;
        }
        case 's': {
            const char *sv=va_arg(args,const char*); if(!sv)sv="(null)";
            int slen=0; { const char *sp=sv; while(*sp){slen++;sp++;} }
            if (!left_justify) { int p2=slen; while(p2<width&&pos<(int)sizeof(buf)-1){buf[pos++]=' ';p2++;} }
            while(*sv&&pos<(int)sizeof(buf)-1) buf[pos++]=*sv++;
            if (left_justify) { int p2=slen; while(p2<width&&pos<(int)sizeof(buf)-1){buf[pos++]=' ';p2++;} }
            break;
        }
        case 'c': if(pos<(int)sizeof(buf)-1) buf[pos++]=(char)va_arg(args,int); break;
        case '%': if(pos<(int)sizeof(buf)-1) buf[pos++]='%'; break;
        default:  if(pos<(int)sizeof(buf)-1) buf[pos++]='%'; if(pos<(int)sizeof(buf)-1) buf[pos++]=*fmt; break;
        }
        fmt++;
    }
    buf[pos] = 0;
    va_end(args);
    term_puts(buf);
}

// =============================================================================
// History
// =============================================================================
static void history_add(const char *line) {
    if (!line[0]) return;
    if (history_count > 0 &&
        kstrcmp(history[(history_count-1)%SHELL_MAX_HISTORY].line, line)==0) return;
    kstrncpy(history[history_count%SHELL_MAX_HISTORY].line, line, SHELL_MAX_LINE-1);
    history_count++;
    history_pos = history_count;
}
static const char *history_prev(void) {
    if (history_pos <= 0) return NULL;
    return history[(--history_pos) % SHELL_MAX_HISTORY].line;
}
static const char *history_next(void) {
    if (history_pos >= history_count) return "";
    history_pos++;
    if (history_pos >= history_count) return "";
    return history[history_pos % SHELL_MAX_HISTORY].line;
}

// =============================================================================
// Variables
// =============================================================================
static void var_set(const char *name, const char *value) {
    for (int i = 0; i < var_count; i++) {
        if (kstrcmp(shell_vars[i].name, name)==0) {
            kstrncpy(shell_vars[i].value, value, SHELL_MAX_LINE-1); return;
        }
    }
    if (var_count < SHELL_MAX_VARS) {
        kstrncpy(shell_vars[var_count].name, name, 63);
        kstrncpy(shell_vars[var_count].value, value, SHELL_MAX_LINE-1);
        var_count++;
    }
}
static const char *var_get(const char *name) {
    for (int i = 0; i < var_count; i++)
        if (kstrcmp(shell_vars[i].name, name)==0) return shell_vars[i].value;
    return env_get(name);
}
static void expand_vars(const char *in, char *out, int max) {
    int o = 0;
    while (*in && o < max-1) {
        if (*in == '$') {
            in++;
            if (*in == '?') {
                char num[12]; int n=last_exit_code, l=0;
                if(!n) num[l++]='0'; else{while(n){num[l++]='0'+n%10;n/=10;}}
                for(int i=0;i<l/2;i++){char t=num[i];num[i]=num[l-1-i];num[l-1-i]=t;}
                for(int i=0;i<l&&o<max-1;i++) out[o++]=num[i];
                in++; continue;
            }
            char vn[64]; int vl=0;
            bool br=(*in=='{'); if(br)in++;
            while(*in&&((*in>='a'&&*in<='z')||(*in>='A'&&*in<='Z')||(*in>='0'&&*in<='9')||*in=='_')&&vl<63)
                vn[vl++]=*in++;
            vn[vl]=0; if(br&&*in=='}')in++;
            const char *v=var_get(vn); if(!v)v="";
            while(*v&&o<max-1) out[o++]=*v++;
        } else if (*in=='\\' && *(in+1)) {
            in++;
            switch(*in){
            case 'n': out[o++]='\n'; break;
            case 't': out[o++]='\t'; break;
            case '\\': out[o++]='\\'; break;
            case '$': out[o++]='$'; break;
            default: out[o++]='\\'; if(o<max-1) out[o++]=*in; break;
            }
            in++;
        } else { out[o++]=*in++; }
    }
    out[o]=0;
}

// =============================================================================
// Aliases
// =============================================================================
static void alias_set(const char *name, const char *cmd) {
    for (int i = 0; i < alias_count; i++) {
        if (kstrcmp(aliases[i].name, name)==0) {
            kstrncpy(aliases[i].command, cmd, SHELL_MAX_LINE-1); return;
        }
    }
    if (alias_count < SHELL_MAX_ALIASES) {
        kstrncpy(aliases[alias_count].name, name, 63);
        kstrncpy(aliases[alias_count].command, cmd, SHELL_MAX_LINE-1);
        alias_count++;
    }
}
static const char *alias_get(const char *name) {
    for (int i = 0; i < alias_count; i++)
        if (kstrcmp(aliases[i].name, name)==0) return aliases[i].command;
    return NULL;
}

// =============================================================================
// readline
// =============================================================================
static void rl_clear(int n) {
    for(int i=0;i<n;i++) term_putchar('\b');
    for(int i=0;i<n;i++) term_putchar(' ');
    for(int i=0;i<n;i++) term_putchar('\b');
}

static void readline(char *buf, int max) {
    int len=0, cursor=0;
    buf[0]=0;
    history_pos=history_count;

    while (1) {
        key_event_t ev = syscall_wait_key();  // GUI-aware: yields instead of hlt
        if (ev.released) continue;

        // ESC key (keycode)
        if (ev.keycode == KEY_ESC) continue;

        if (ev.keycode == KEY_ASCII) {
            char c = ev.ascii;

            if (c=='\n'||c=='\r') { buf[len]=0; term_putchar('\n'); return; }

            if (ev.ctrl && (c==3||c=='c'||c=='C')) {
                buf[0]=0; term_puts("^C\n"); return;
            }
            if (ev.ctrl && (c==12||c=='l'||c=='L')) {
                vga_clear(); term_reset(); buf[0]=0; term_putchar('\n'); return;
            }
            if (ev.ctrl && (c==1||c=='a'||c=='A')) {
                for(int i=0;i<cursor;i++) term_putchar('\b'); cursor=0; continue;
            }
            if (ev.ctrl && (c==5||c=='e'||c=='E')) {
                while(cursor<len){term_putchar(buf[cursor]);cursor++;} continue;
            }
            if (ev.ctrl && (c==11||c=='k'||c=='K')) {
                int old=len; len=cursor; buf[len]=0;
                for(int i=cursor;i<old;i++) term_putchar(' ');
                for(int i=cursor;i<old;i++) term_putchar('\b');
                continue;
            }
            if (ev.ctrl && (c==21||c=='u'||c=='U')) {
                for(int i=0;i<cursor;i++) term_putchar('\b');
                for(int i=0;i<len;i++) term_putchar(' ');
                for(int i=0;i<len;i++) term_putchar('\b');
                len=0; cursor=0; buf[0]=0; continue;
            }
            if (c=='\b'||c==127) {
                if (cursor>0) {
                    for(int i=cursor-1;i<len-1;i++) buf[i]=buf[i+1];
                    len--; cursor--; buf[len]=0;
                    term_putchar('\b');
                    for(int i=cursor;i<len;i++) term_putchar(buf[i]);
                    term_putchar(' ');
                    for(int i=cursor;i<=len;i++) term_putchar('\b');
                }
                continue;
            }
            if (c=='\t') continue;
            if (c>=32 && len<max-1) {
                for(int i=len;i>cursor;i--) buf[i]=buf[i-1];
                buf[cursor]=c; len++; buf[len]=0;
                for(int i=cursor;i<len;i++) term_putchar(buf[i]);
                for(int i=cursor+1;i<len;i++) term_putchar('\b');
                cursor++;
            }
        } else {
            switch (ev.keycode) {
            case KEY_UP: {
                const char *p=history_prev();
                if(p){rl_clear(len);kstrncpy(buf,p,max-1);len=(int)kstrlen(buf);cursor=len;term_puts(buf);}
                break;
            }
            case KEY_DOWN: {
                const char *n=history_next();
                rl_clear(len); kstrncpy(buf,n?n:"",max-1); len=(int)kstrlen(buf); cursor=len; term_puts(buf);
                break;
            }
            case KEY_LEFT:  if(cursor>0){cursor--;term_putchar('\b');} break;
            case KEY_RIGHT: if(cursor<len){term_putchar(buf[cursor]);cursor++;} break;
            case KEY_HOME:  for(int i=0;i<cursor;i++)term_putchar('\b'); cursor=0; break;
            case KEY_END:   while(cursor<len){term_putchar(buf[cursor]);cursor++;} break;
            case KEY_DELETE:
                if(cursor<len){
                    for(int i=cursor;i<len-1;i++) buf[i]=buf[i+1];
                    len--; buf[len]=0;
                    for(int i=cursor;i<len;i++) term_putchar(buf[i]);
                    term_putchar(' ');
                    for(int i=cursor;i<=len;i++) term_putchar('\b');
                }
                break;
            default: break;
            }
        }
    }
}

// =============================================================================
// Tokenizer
// =============================================================================
typedef struct {
    char *args[SHELL_MAX_ARGS];
    int   argc;
    char *redir_in;
    char *redir_out;
    char *redir_out_append;
    bool  background;
} cmd_t;

static int tokenize(char *line, cmd_t *cmd) {
    cmd->argc=0; cmd->redir_in=cmd->redir_out=cmd->redir_out_append=NULL; cmd->background=false;
    char *p=line;
    while(*p) {
        while(*p==' '||*p=='\t') p++;
        if(!*p) break;
        if(*p=='&'){cmd->background=true;p++;continue;}
        if(*p=='<'){
            p++; while(*p==' ')p++;
            cmd->redir_in=p;
            while(*p&&*p!=' '&&*p!='\t'&&*p!='>'&&*p!='<')p++;
            if(*p)*p++=0; continue;
        }
        if(*p=='>'){
            p++; bool app=(*p=='>'); if(app)p++;
            while(*p==' ')p++;
            if(app) cmd->redir_out_append=p; else cmd->redir_out=p;
            while(*p&&*p!=' '&&*p!='\t'&&*p!='>'&&*p!='<')p++;
            if(*p)*p++=0; continue;
        }
        if(*p=='"'||*p=='\''){
            char q=*p++;
            if(cmd->argc<SHELL_MAX_ARGS-1) cmd->args[cmd->argc++]=p;
            while(*p&&*p!=q)p++;
            if(*p)*p++=0; continue;
        }
        if(cmd->argc<SHELL_MAX_ARGS-1) cmd->args[cmd->argc++]=p;
        while(*p&&*p!=' '&&*p!='\t'&&*p!='>'&&*p!='<'&&*p!='&')p++;
        if(*p=='&'){*p=0;cmd->background=true;p++;}
        else if(*p)*p++=0;
    }
    cmd->args[cmd->argc]=NULL;
    return cmd->argc;
}

// =============================================================================
// install, extra utilities, wavplay (PC speaker, 8-bit mono PCM)
// =============================================================================
static void builtin_install_cmd(cmd_t *cmd) {
    if (cmd->argc < 2) {
        tprintf("usage: install <package.ins>\n");
        last_exit_code = 1;
        return;
    }
    char r[SHELL_MAX_PATH];
    path_resolve(cmd->args[1], r);
    int rc = ins_install_from_file(r);
    last_exit_code = (rc == 0) ? 0 : 1;
}

static void builtin_head(cmd_t *cmd) {
    if (cmd->argc < 2) {
        tprintf("usage: head <file> [lines]\n");
        last_exit_code = 1;
        return;
    }
    int maxl = 10;
    if (cmd->argc >= 3) {
        maxl = katoi(cmd->args[2]);
        if (maxl <= 0) maxl = 10;
    }
    char path[SHELL_MAX_PATH];
    path_resolve(cmd->args[1], path);
    int fd = fat32_open(path, FAT32_O_RDONLY);
    if (fd < 0) {
        term_set_color(TC_RED);
        tprintf("head: cannot open\n");
        term_reset();
        last_exit_code = 1;
        return;
    }
    char buf[256];
    int lines = 0;
    int col = 0;
    while (lines < maxl) {
        int n = fat32_read(fd, buf, 1);
        if (n <= 0) break;
        term_putchar(buf[0]);
        if (buf[0] == '\n') lines++;
        col++;
        if (col > 4000) break;
    }
    fat32_close(fd);
    last_exit_code = 0;
}

static void builtin_tail(cmd_t *cmd) {
    if (cmd->argc < 2) {
        tprintf("usage: tail <file> [lines]\n");
        last_exit_code = 1;
        return;
    }
    int want = 10;
    int argfile = 1;
    if (cmd->argc >= 3) {
        want = katoi(cmd->args[2]);
        if (want <= 0) want = 10;
    }
    char path[SHELL_MAX_PATH];
    path_resolve(cmd->args[argfile], path);
    fat32_stat_t st;
    if (fat32_stat(path, &st) != 0 || st.is_dir) {
        term_set_color(TC_RED);
        tprintf("tail: not a file\n");
        term_reset();
        last_exit_code = 1;
        return;
    }
    if (st.size > 65536) {
        tprintf("tail: file too large (max 64K)\n");
        last_exit_code = 1;
        return;
    }
    void *raw = kmalloc((size_t)st.size + 1);
    if (!raw) {
        last_exit_code = 1;
        return;
    }
    int fd = fat32_open(path, FAT32_O_RDONLY);
    if (fd < 0 || (uint32_t)fat32_read(fd, raw, st.size) != st.size) {
        kfree(raw);
        if (fd >= 0) fat32_close(fd);
        last_exit_code = 1;
        return;
    }
    fat32_close(fd);
    ((char *)raw)[st.size] = 0;
    char *p = (char *)raw;
    int nlines = 0;
    for (char *q = p; *q; q++)
        if (*q == '\n') nlines++;
    int skip = nlines - want;
    if (skip < 0) skip = 0;
    char *start = p;
    if (skip > 0) {
        int cur = 0;
        for (char *q = p; *q; q++) {
            if (*q == '\n') {
                cur++;
                start = q + 1;
                if (cur >= skip) break;
            }
        }
    }
    term_puts(start);
    if (st.size > 0 && ((char *)raw)[st.size - 1] != '\n') term_putchar('\n');
    kfree(raw);
    last_exit_code = 0;
}

static void builtin_stat_cmd(cmd_t *cmd) {
    if (cmd->argc < 2) {
        tprintf("usage: stat <path>\n");
        last_exit_code = 1;
        return;
    }
    char path[SHELL_MAX_PATH];
    path_resolve(cmd->args[1], path);
    fat32_stat_t st;
    if (fat32_stat(path, &st) != 0) {
        term_set_color(TC_RED);
        tprintf("stat: not found\n");
        term_reset();
        last_exit_code = 1;
        return;
    }
    tprintf("File: %s\n", path);
    tprintf("Size: %u  %s\n", st.size, st.is_dir ? "directory" : "file");
    last_exit_code = 0;
}

static void builtin_hexdump(cmd_t *cmd) {
    if (cmd->argc < 2) {
        tprintf("usage: hexdump <file> [max_bytes]\n");
        last_exit_code = 1;
        return;
    }
    uint32_t maxb = 256;
    if (cmd->argc >= 3) maxb = (uint32_t)katoi(cmd->args[2]);
    if (maxb == 0 || maxb > 4096) maxb = 256;
    char path[SHELL_MAX_PATH];
    path_resolve(cmd->args[1], path);
    int fd = fat32_open(path, FAT32_O_RDONLY);
    if (fd < 0) {
        term_set_color(TC_RED);
        tprintf("hexdump: cannot open\n");
        term_reset();
        last_exit_code = 1;
        return;
    }
    uint8_t buf[32];
    uint32_t off = 0;
    while (off < maxb) {
        uint32_t chunk = maxb - off;
        if (chunk > sizeof(buf)) chunk = (uint32_t)sizeof(buf);
        int n = fat32_read(fd, buf, chunk);
        if (n <= 0) break;
        tprintf("%04x: ", off);
        for (int i = 0; i < n; i++) tprintf("%02x ", buf[i]);
        tprintf(" |");
        for (int i = 0; i < n; i++) {
            uint8_t c = buf[i];
            term_putchar((c >= 32 && c < 127) ? (char)c : '.');
        }
        tprintf("|\n");
        off += (uint32_t)n;
    }
    fat32_close(fd);
    last_exit_code = 0;
}

static void builtin_basename(cmd_t *cmd) {
    if (cmd->argc < 2) {
        tprintf("usage: basename <path>\n");
        last_exit_code = 1;
        return;
    }
    const char *p = cmd->args[1];
    char *slash = kstrrchr(p, '/');
    tprintf("%s\n", slash ? slash + 1 : p);
    last_exit_code = 0;
}

static void builtin_dirname(cmd_t *cmd) {
    if (cmd->argc < 2) {
        tprintf("usage: dirname <path>\n");
        last_exit_code = 1;
        return;
    }
    char tmp[SHELL_MAX_PATH];
    kstrncpy(tmp, cmd->args[1], SHELL_MAX_PATH - 1);
    tmp[SHELL_MAX_PATH - 1] = 0;
    char *slash = kstrrchr(tmp, '/');
    if (!slash) {
        tprintf(".\n");
    } else if (slash == tmp) {
        tprintf("/\n");
    } else {
        *slash = 0;
        tprintf("%s\n", tmp);
    }
    last_exit_code = 0;
}

static void builtin_version(cmd_t *cmd) {
    (void)cmd;
    tprintf("Eclipse32 — ring 0 kernel + esh + .INS installer + WAV (speaker)\n");
    last_exit_code = 0;
}

static void builtin_len(cmd_t *cmd) {
    if (cmd->argc < 2) {
        tprintf("usage: len <file>\n");
        last_exit_code = 1;
        return;
    }
    char path[SHELL_MAX_PATH];
    path_resolve(cmd->args[1], path);
    fat32_stat_t st;
    if (fat32_stat(path, &st) != 0 || st.is_dir) {
        last_exit_code = 1;
        return;
    }
    tprintf("%u\n", st.size);
    last_exit_code = 0;
}

static void builtin_arch(cmd_t *cmd) {
    (void)cmd;
    tprintf("i686-pc-eclipse32\n");
    last_exit_code = 0;
}

static void builtin_hostname(cmd_t *cmd) {
    (void)cmd;
    const char *h = env_get("HOSTNAME");
    tprintf("%s\n", h ? h : "eclipse32");
    last_exit_code = 0;
}

static int wav_open_pcm(int fd, uint32_t *pcm_bytes, uint16_t *bits, uint16_t *chans, uint32_t *rate) {
    uint8_t hdr[12];
    if (fat32_read(fd, hdr, 12) != 12) return -1;
    if (kmemcmp(hdr, "RIFF", 4) != 0 || kmemcmp(hdr + 8, "WAVE", 4) != 0) return -1;
    *bits = 8;
    *chans = 1;
    *rate = 8000;
    for (int guard = 0; guard < 256; guard++) {
        uint8_t ch[8];
        if (fat32_read(fd, ch, 8) != 8) return -1;
        uint32_t csz = (uint32_t)ch[4] | ((uint32_t)ch[5] << 8) | ((uint32_t)ch[6] << 16) | ((uint32_t)ch[7] << 24);
        if (kmemcmp(ch, "fmt ", 4) == 0) {
            uint8_t fmt[32];
            if (csz < 16) return -1;
            uint32_t rd = csz > sizeof(fmt) ? (uint32_t)sizeof(fmt) : csz;
            if (fat32_read(fd, fmt, (int)rd) != (int)rd) return -1;
            *chans = (uint16_t)((uint16_t)fmt[2] | ((uint16_t)fmt[3] << 8));
            *rate = (uint32_t)fmt[4] | ((uint32_t)fmt[5] << 8) | ((uint32_t)fmt[6] << 16) | ((uint32_t)fmt[7] << 24);
            *bits = (uint16_t)((uint16_t)fmt[14] | ((uint16_t)fmt[15] << 8));
            if (csz > rd) {
                uint32_t skip = csz - rd;
                if (fat32_seek(fd, (int32_t)skip, FAT32_SEEK_CUR) < 0) return -1;
            }
        } else if (kmemcmp(ch, "data", 4) == 0) {
            *pcm_bytes = csz;
            return 0;
        } else {
            uint32_t skip = (csz + 1u) & ~1u;
            if (fat32_seek(fd, (int32_t)skip, FAT32_SEEK_CUR) < 0) return -1;
        }
    }
    return -1;
}

static void builtin_wavplay(cmd_t *cmd) {
    if (cmd->argc < 2) {
        tprintf("usage: wavplay <file.wav>\n");
        last_exit_code = 1;
        return;
    }
    char path[SHELL_MAX_PATH];
    path_resolve(cmd->args[1], path);
    int fd = fat32_open(path, FAT32_O_RDONLY);
    if (fd < 0) {
        term_set_color(TC_RED);
        tprintf("wavplay: cannot open\n");
        term_reset();
        last_exit_code = 1;
        return;
    }
    uint32_t ds;
    uint16_t bits, chans;
    uint32_t rate;
    if (wav_open_pcm(fd, &ds, &bits, &chans, &rate) != 0) {
        fat32_close(fd);
        tprintf("wavplay: invalid RIFF WAVE file\n");
        last_exit_code = 1;
        return;
    }
    if (bits != 8 || chans != 1) {
        fat32_close(fd);
        tprintf("wavplay: only 8-bit mono PCM supported\n");
        last_exit_code = 1;
        return;
    }
    if (ds > 512 * 1024) ds = 512 * 1024;
    tprintf("[wavplay] rate=%u Hz, %u bytes (PC speaker, ~1 sample/ms)\n", rate, ds);
    uint32_t step = rate / 1000;
    if (step < 1) step = 1;
    uint32_t pos = 0;
    while (pos < ds) {
        uint8_t sample;
        if (fat32_read(fd, &sample, 1) != 1) break;
        if (pos % step == 0) {
            uint32_t hz = 200u + (uint32_t)sample * 8u;
            if (hz > 4000u) hz = 4000u;
            speaker_on(hz);
            pit_sleep_ms(1);
            speaker_off();
        }
        pos++;
    }
    speaker_off();
    fat32_close(fd);
    last_exit_code = 0;
}

// =============================================================================
// Built-ins
// =============================================================================
static void builtin_help(void) {
    term_set_color(TC_CYAN);
    tprintf("Eclipse Shell (esh) - Commands\n");
    term_reset();
    tprintf("------------------------------\n");
    static const char *h[] = {
        "help    ","Show this help",
        "echo    ","echo [-n] <text>",
        "cd      ","cd [dir|'-']",
        "pwd     ","Print working directory",
        "ls      ","ls [-la] [path]",
        "cat     ","cat <file>",
        "touch   ","touch <file>",
        "mkdir   ","mkdir <dir>",
        "rm      ","rm <file>",
        "cp      ","cp <src> <dst>",
        "mv      ","mv <src> <dst>",
        "write   ","write <file> [-a]  (end input with '.')",
        "ed      ","ed <file>  (vi-like editor)",
        "env     ","List environment",
        "export  ","export NAME=value",
        "set     ","set <name> <value>",
        "unset   ","unset <name>",
        "alias   ","alias [name cmd]",
        "unalias ","unalias <name>",
        "history ","Show history",
        "clear   ","Clear screen",
        "uname   ","uname [-a]",
        "uptime  ","Time since boot",
        "free    ","Memory usage",
        "date    ","Current time",
        "sleep   ","sleep <ms>",
        "beep    ","beep [freq_hz] [duration_ms]",
        "wc      ","wc <file>",
        "which   ","which <name>",
        "source  ","source <file.sh>",
        "exit    ","exit [code]",
        "reboot  ","Reboot",
        "halt    ","Halt",
        "install ","install <pkg.ins>  (.INS package)",
        "head    ","head <file> [lines]",
        "tail    ","tail <file> [lines]",
        "stat    ","stat <path>",
        "hexdump ","hexdump <file> [max_b]",
        "basename","basename <path>",
        "dirname ","dirname <path>",
        "version ","Show build tag",
        "len     ","len <file>",
        "arch    ","Machine triple",
        "hostname","Print HOSTNAME",
        "wavplay ","wavplay <file.wav>  (8-bit mono, PC speaker)",
        NULL,NULL
    };
    for(int i=0;h[i];i+=2){
        term_set_color(TC_GREEN); tprintf("  %s",h[i]);
        term_reset(); tprintf("  %s\n",h[i+1]);
    }
    tprintf("\ned keybindings: i=insert  ESC=normal  :w=save  :q=quit  :wq/:q!\n");
    tprintf("  hjkl/arrows=move  dd=del line  yy=yank  p=paste  x=del char\n");
    tprintf("  o/O=new line  r=replace  G=end  g=top  0/$=line start/end\n");
}

static void builtin_echo(cmd_t *cmd) {
    bool nl=true; int start=1;
    if(cmd->argc>1&&kstrcmp(cmd->args[1],"-n")==0){nl=false;start=2;}
    for(int i=start;i<cmd->argc;i++){
        if(i>start) term_putchar(' ');
        char exp[SHELL_MAX_LINE]; expand_vars(cmd->args[i],exp,sizeof(exp));
        term_puts(exp);
    }
    if(nl) term_putchar('\n');
}

static void builtin_cd(cmd_t *cmd) {
    const char *target;
    if(cmd->argc<2){target=env_get("HOME");if(!target)target="/";}
    else if(kstrcmp(cmd->args[1],"-")==0){
        target=env_get("OLDPWD");
        if(!target){tprintf("cd: OLDPWD not set\n");last_exit_code=1;return;}
        tprintf("%s\n",target);
    } else target=cmd->args[1];

    char np[SHELL_MAX_PATH]; path_resolve(target,np);
    fat32_stat_t st;
    if(fat32_stat(np,&st)!=0||!st.is_dir){
        term_set_color(TC_RED); tprintf("cd: %s: No such directory\n",np); term_reset();
        last_exit_code=1; return;
    }
    env_set("OLDPWD",cwd);
    kstrncpy(cwd,np,SHELL_MAX_PATH-1);
    env_set("PWD",cwd);
    last_exit_code=0;
}

static void builtin_ls(cmd_t *cmd) {
    const char *ap=NULL; bool lf=false,sh=false;
    for(int i=1;i<cmd->argc;i++){
        if(cmd->args[i][0]=='-'){
            if(kstrchr(cmd->args[i],'l'))lf=true;
            if(kstrchr(cmd->args[i],'a'))sh=true;
        } else ap=cmd->args[i];
    }
    char r[SHELL_MAX_PATH];
    if(ap) path_resolve(ap,r); else kstrncpy(r,cwd,SHELL_MAX_PATH-1);

    fat32_dir_entry_t entries[128];
    int count=fat32_readdir(r,entries,128);
    if(count<0){
        if(!fs_is_mounted()){ term_set_color(TC_YELLOW); tprintf("ls: filesystem not mounted\n"); term_reset(); }
        else { term_set_color(TC_RED); tprintf("ls: '%s': No such directory\n",r); term_reset(); }
        last_exit_code=1; return;
    }
    if(count==0){tprintf("(empty)\n");last_exit_code=0;return;}
    if(lf){term_set_color(TC_CYAN);tprintf("total %d\n",count);term_reset();}

    int col=0;
    for(int i=0;i<count;i++){
        if(!sh&&entries[i].is_hidden) continue;
        if(lf){
            if(entries[i].is_dir){term_set_color(TC_CYAN);tprintf("drwxr-xr-x ");}
            else{term_reset();tprintf("-rw-r--r-- ");}
            term_reset(); tprintf("%8u  ",entries[i].size);
            if(entries[i].is_dir){term_set_color(TC_CYAN);tprintf("%s/\n",entries[i].name);term_reset();}
            else{
                char *dot=kstrrchr(entries[i].name,'.');
                if(dot&&(kstrcmp(dot,".sh")==0||kstrcmp(dot,".bin")==0||kstrcmp(dot,".e32")==0))term_set_color(TC_GREEN);
                else term_reset();
                tprintf("%s\n",entries[i].name); term_reset();
            }
        } else {
            if(entries[i].is_dir){
                term_set_color(TC_CYAN); tprintf("%-17s/",entries[i].name);
            } else {
                char *dot=kstrrchr(entries[i].name,'.');
                if(dot&&(kstrcmp(dot,".sh")==0||kstrcmp(dot,".e32")==0))term_set_color(TC_GREEN); else term_reset();
                tprintf("%-18s",entries[i].name);
            }
            term_reset(); col++;
            if(col%4==0) tprintf("\n");
        }
    }
    if(!lf&&col%4!=0) tprintf("\n");
    last_exit_code=0;
}

static void builtin_cat(cmd_t *cmd) {
    if(cmd->argc<2){
        char line[256];
        while(1){readline(line,sizeof(line));if(!line[0])break;tprintf("%s\n",line);}
        return;
    }
    for(int a=1;a<cmd->argc;a++){
        char r[SHELL_MAX_PATH]; path_resolve(cmd->args[a],r);
        int fd=fat32_open(r,FAT32_O_RDONLY);
        if(fd<0){term_set_color(TC_RED);tprintf("cat: %s: No such file\n",cmd->args[a]);term_reset();last_exit_code=1;return;}
        char buf[512]; int n;
        while((n=fat32_read(fd,buf,sizeof(buf)))>0)
            for(int i=0;i<n;i++) term_putchar(buf[i]);
        fat32_close(fd);
    }
    last_exit_code=0;
}

static void builtin_rm(cmd_t *cmd) {
    if(cmd->argc<2){tprintf("usage: rm <file>\n");last_exit_code=1;return;}
    char r[SHELL_MAX_PATH]; path_resolve(cmd->args[1],r);
    if(fat32_delete(r)!=0){term_set_color(TC_RED);tprintf("rm: cannot remove '%s'\n",cmd->args[1]);term_reset();last_exit_code=1;}
    else last_exit_code=0;
}

static void builtin_cp(cmd_t *cmd) {
    if(cmd->argc<3){tprintf("usage: cp <src> <dst>\n");last_exit_code=1;return;}
    char src[SHELL_MAX_PATH],dst[SHELL_MAX_PATH];
    path_resolve(cmd->args[1],src); path_resolve(cmd->args[2],dst);
    int fi=fat32_open(src,FAT32_O_RDONLY);
    if(fi<0){term_set_color(TC_RED);tprintf("cp: cannot open '%s'\n",cmd->args[1]);term_reset();last_exit_code=1;return;}
    int fo=fat32_open(dst,FAT32_O_RDWR|FAT32_O_CREAT|FAT32_O_TRUNC);
    if(fo<0){fat32_close(fi);term_set_color(TC_RED);tprintf("cp: cannot create '%s'\n",cmd->args[2]);term_reset();last_exit_code=1;return;}
    char buf[512]; int n;
    while((n=fat32_read(fi,buf,sizeof(buf)))>0) fat32_write(fo,buf,(uint32_t)n);
    fat32_close(fi); fat32_close(fo); last_exit_code=0;
}

static void builtin_mv(cmd_t *cmd) {
    if(cmd->argc<3){tprintf("usage: mv <src> <dst>\n");last_exit_code=1;return;}
    char src[SHELL_MAX_PATH],dst[SHELL_MAX_PATH];
    path_resolve(cmd->args[1],src); path_resolve(cmd->args[2],dst);
    if(fat32_rename(src,dst)!=0){term_set_color(TC_RED);tprintf("mv: cannot rename '%s'\n",cmd->args[1]);term_reset();last_exit_code=1;}
    else last_exit_code=0;
}

static void builtin_write(cmd_t *cmd) {
    if(cmd->argc<2){tprintf("usage: write <file> [-a]\n");last_exit_code=1;return;}
    char r[SHELL_MAX_PATH]; path_resolve(cmd->args[1],r);
    bool app=(cmd->argc>2&&kstrcmp(cmd->args[2],"-a")==0);
    int fl=FAT32_O_RDWR|FAT32_O_CREAT|(app?FAT32_O_APPEND:FAT32_O_TRUNC);
    int fd=fat32_open(r,fl);
    if(fd<0){term_set_color(TC_RED);tprintf("write: cannot open '%s'\n",cmd->args[1]);term_reset();last_exit_code=1;return;}
    tprintf("Enter text. Type '.' alone to finish:\n");
    char line[SHELL_MAX_LINE];
    while(1){
        readline(line,sizeof(line));
        if(kstrcmp(line,".")==0) break;
        fat32_write(fd,line,(uint32_t)kstrlen(line));
        fat32_write(fd,"\n",1);
    }
    fat32_close(fd); last_exit_code=0;
}

// =============================================================================
// ed - vi-like editor
// =============================================================================
#define ED_MAX_LINES  512
#define ED_LINE_LEN   256

static char  ed_buf[ED_MAX_LINES][ED_LINE_LEN];
static int   ed_lines    = 0;
static int   ed_row      = 0;
static int   ed_col      = 0;
static bool  ed_modified = false;
static char  ed_file[SHELL_MAX_PATH];
static char  ed_yank[ED_LINE_LEN];
static bool  ed_yank_ok  = false;
static bool  ed_insert   = false;
static bool  ed_pd       = false;  // pending 'd' for dd
static bool  ed_py       = false;  // pending 'y' for yy

static void ed_status(const char *msg) {
    uint8_t col = (uint8_t)((VGA_COLOR_BLACK<<4)|VGA_COLOR_LIGHT_GREY);
    for(int i=0;i<80;i++) vga_put_at((uint16_t)i,24,' ',col);
    for(int i=0;msg[i]&&i<79;i++) vga_put_at((uint16_t)i,24,msg[i],col);
}

static void ed_redraw(void) {
    int vr=23;
    int start=ed_row-vr/2;
    if(start<0) start=0;
    if(start+vr>ed_lines) start=ed_lines-vr;
    if(start<0) start=0;

    vga_set_color(VGA_COLOR_LIGHT_GREY,VGA_COLOR_BLACK);

    for(int i=0;i<vr;i++){
        int ln=start+i;
        uint8_t color=(ln==ed_row)?
            (uint8_t)((VGA_COLOR_BLACK<<4)|VGA_COLOR_CYAN):
            (uint8_t)((VGA_COLOR_BLACK<<4)|VGA_COLOR_LIGHT_GREY);
        for(int c=0;c<80;c++) vga_put_at((uint16_t)c,(uint16_t)i,' ',color);
        if(ln<ed_lines){
            char num[8]; int nl=0; int n=ln+1;
            if(!n)num[nl++]='0'; else{while(n){num[nl++]='0'+n%10;n/=10;}}
            for(int a=0;a<nl/2;a++){char t=num[a];num[a]=num[nl-1-a];num[nl-1-a]=t;}
            int c=0;
            for(int sp=nl;sp<4;sp++) vga_put_at((uint16_t)c++,(uint16_t)i,' ',color);
            for(int j=0;j<nl;j++) vga_put_at((uint16_t)c++,(uint16_t)i,num[j],color);
            vga_put_at((uint16_t)c++,(uint16_t)i,' ',color);
            for(int j=0;ed_buf[ln][j]&&c<80;j++) vga_put_at((uint16_t)c++,(uint16_t)i,ed_buf[ln][j],color);
        } else {
            vga_put_at(0,(uint16_t)i,'~',(uint8_t)((VGA_COLOR_BLACK<<4)|VGA_COLOR_DARK_GREY));
        }
    }

    // Build status string
    char st[82]; int sp=0;
    const char *mode=ed_insert?"[INSERT]":"[NORMAL]";
    for(int i=0;mode[i]&&sp<9;i++) st[sp++]=mode[i];
    st[sp++]=' ';
    for(int i=0;ed_file[i]&&sp<55;i++) st[sp++]=ed_file[i];
    if(ed_modified){st[sp++]=' ';st[sp++]='[';st[sp++]='+';st[sp++]=']';}
    st[sp++]=' ';
    char rc[16]; int rl=0;
    int rv=ed_row+1; if(!rv)rc[rl++]='0'; else{int t=rv;while(t){rc[rl++]='0'+t%10;t/=10;} for(int a=0;a<rl/2;a++){char t2=rc[a];rc[a]=rc[rl-1-a];rc[rl-1-a]=t2;}}
    for(int i=0;i<rl&&sp<78;i++) st[sp++]=rc[i];
    st[sp++]=':';
    rl=0; int cv=ed_col+1; if(!cv)rc[rl++]='0'; else{int t=cv;while(t){rc[rl++]='0'+t%10;t/=10;} for(int a=0;a<rl/2;a++){char t2=rc[a];rc[a]=rc[rl-1-a];rc[rl-1-a]=t2;}}
    for(int i=0;i<rl&&sp<79;i++) st[sp++]=rc[i];
    st[sp]=0;
    ed_status(st);

    int dr=ed_row-start;
    int dc=ed_col+5;
    if(dc>79)dc=79; if(dr<0)dr=0; if(dr>=vr)dr=vr-1;
    vga_cursor_move((uint16_t)dc,(uint16_t)dr);
}

static int ed_save(void) {
    int fd=fat32_open(ed_file,FAT32_O_RDWR|FAT32_O_CREAT|FAT32_O_TRUNC);
    if(fd<0) return -1;
    for(int i=0;i<ed_lines;i++){
        fat32_write(fd,ed_buf[i],(uint32_t)kstrlen(ed_buf[i]));
        fat32_write(fd,"\n",1);
    }
    fat32_close(fd); ed_modified=false; return 0;
}

static void ed_ins(char c) {
    if(ed_lines==0){ed_lines=1;ed_buf[0][0]=0;}
    char *line=ed_buf[ed_row]; int len=(int)kstrlen(line);
    if(len>=ED_LINE_LEN-1) return;
    for(int i=len;i>=ed_col;i--) line[i+1]=line[i];
    line[ed_col++]=c; ed_modified=true;
}

static void ed_delch(void) {
    char *line=ed_buf[ed_row]; int len=(int)kstrlen(line);
    if(ed_col>=len) return;
    for(int i=ed_col;i<len;i++) line[i]=line[i+1];
    ed_modified=true;
}

static void ed_delline(void) {
    if(ed_lines==0) return;
    kstrncpy(ed_yank,ed_buf[ed_row],ED_LINE_LEN-1); ed_yank_ok=true;
    for(int i=ed_row;i<ed_lines-1;i++) kstrncpy(ed_buf[i],ed_buf[i+1],ED_LINE_LEN-1);
    ed_lines--;
    if(ed_lines==0){ed_lines=1;ed_buf[0][0]=0;}
    if(ed_row>=ed_lines) ed_row=ed_lines-1;
    ed_col=0; ed_modified=true;
}

static void ed_split(void) {
    if(ed_lines>=ED_MAX_LINES-1) return;
    char rest[ED_LINE_LEN]; kstrncpy(rest,ed_buf[ed_row]+ed_col,ED_LINE_LEN-1);
    ed_buf[ed_row][ed_col]=0;
    for(int i=ed_lines;i>ed_row+1;i--) kstrncpy(ed_buf[i],ed_buf[i-1],ED_LINE_LEN-1);
    ed_lines++; ed_row++;
    kstrncpy(ed_buf[ed_row],rest,ED_LINE_LEN-1);
    ed_col=0; ed_modified=true;
}

static void ed_newbelow(void) {
    if(ed_lines>=ED_MAX_LINES-1) return;
    for(int i=ed_lines;i>ed_row+1;i--) kstrncpy(ed_buf[i],ed_buf[i-1],ED_LINE_LEN-1);
    ed_lines++; ed_row++; ed_buf[ed_row][0]=0; ed_col=0; ed_modified=true;
}

static void ed_clamp(void) {
    int l=(int)kstrlen(ed_buf[ed_row]);
    if(ed_col>l) ed_col=l;
    if(ed_col<0) ed_col=0;
}

static void ed_colon(bool *quit) {
    char cb[64]; int cl=1; cb[0]=':'; cb[1]=0;
    ed_status(":");
    while(1){
        key_event_t ev=keyboard_wait_event();
        if(ev.released) continue;
        if(ev.keycode==KEY_ESC){cl=0;break;}
        if(ev.keycode==KEY_ASCII){
            char c=ev.ascii;
            if(c==27){cl=0;break;}
            if(c=='\n'||c=='\r') break;
            if((c=='\b'||c==127)&&cl>1) cl--;
            else if(cl<63) cb[cl++]=c;
            cb[cl]=0; ed_status(cb);
        }
    }
    cb[cl]=0; if(cl<=1) return;
    const char *arg=cb+1;
    if(kstrcmp(arg,"w")==0){if(ed_save()==0)ed_status("Saved.");else ed_status("Save failed!");}
    else if(kstrcmp(arg,"q")==0){if(ed_modified)ed_status("Unsaved! Use :q! or :wq");else *quit=true;}
    else if(kstrcmp(arg,"wq")==0||kstrcmp(arg,"x")==0){ed_save();*quit=true;}
    else if(kstrcmp(arg,"q!")==0)*quit=true;
    else{
        int t=0; bool dg=false;
        for(int i=0;arg[i]>='0'&&arg[i]<='9';i++){t=t*10+(arg[i]-'0');dg=true;}
        if(dg){ed_row=t-1;if(ed_row<0)ed_row=0;if(ed_row>=ed_lines)ed_row=ed_lines-1;ed_col=0;}
        else ed_status("Unknown command");
    }
}

static void builtin_ed(cmd_t *cmd) {
    if(cmd->argc<2){tprintf("usage: ed <file>\n");last_exit_code=1;return;}
    path_resolve(cmd->args[1],ed_file);
    ed_lines=0;ed_row=0;ed_col=0;ed_modified=false;ed_insert=false;
    ed_pd=false;ed_py=false;ed_yank[0]=0;ed_yank_ok=false;

    int fd=fat32_open(ed_file,FAT32_O_RDONLY);
    if(fd>=0){
        char fb[512]; int n,lp=0; ed_buf[0][0]=0;
        while((n=fat32_read(fd,fb,sizeof(fb)))>0&&ed_lines<ED_MAX_LINES){
            for(int i=0;i<n;i++){
                if(fb[i]=='\n'){
                    ed_buf[ed_lines][lp]=0; ed_lines++;
                    if(ed_lines<ED_MAX_LINES) ed_buf[ed_lines][0]=0;
                    lp=0;
                } else if(lp<ED_LINE_LEN-1) ed_buf[ed_lines][lp++]=fb[i];
            }
        }
        if(lp>0&&ed_lines<ED_MAX_LINES){ed_buf[ed_lines][lp]=0;ed_lines++;}
        fat32_close(fd);
    }
    if(ed_lines==0){ed_buf[0][0]=0;ed_lines=1;}

    vga_clear();
    ed_redraw();

    bool running=true;
    while(running){
        key_event_t ev=keyboard_wait_event();
        if(ev.released) continue;

        // ESC always exits insert mode
        if(ev.keycode==KEY_ESC){
            if(ed_insert){ed_insert=false;if(ed_col>0)ed_col--;ed_pd=false;ed_py=false;}
            ed_redraw(); continue;
        }

        if(ed_insert){
            if(ev.keycode==KEY_ASCII){
                char c=ev.ascii;
                if(c==27){ed_insert=false;if(ed_col>0)ed_col--;}
                else if(c=='\b'||c==127){
                    if(ed_col>0){ed_col--;ed_delch();}
                    else if(ed_row>0){
                        int pl=(int)kstrlen(ed_buf[ed_row-1]);
                        kstrcat(ed_buf[ed_row-1],ed_buf[ed_row]);
                        for(int i=ed_row;i<ed_lines-1;i++) kstrncpy(ed_buf[i],ed_buf[i+1],ED_LINE_LEN-1);
                        ed_lines--; ed_row--; ed_col=pl; ed_modified=true;
                    }
                }
                else if(c=='\n'||c=='\r') ed_split();
                else if(c>=32) ed_ins(c);
            } else {
                switch(ev.keycode){
                case KEY_UP:    if(ed_row>0){ed_row--;ed_clamp();} break;
                case KEY_DOWN:  if(ed_row<ed_lines-1){ed_row++;ed_clamp();} break;
                case KEY_LEFT:  if(ed_col>0)ed_col--; break;
                case KEY_RIGHT: {int l=(int)kstrlen(ed_buf[ed_row]);if(ed_col<l)ed_col++;} break;
                case KEY_HOME:  ed_col=0; break;
                case KEY_END:   ed_col=(int)kstrlen(ed_buf[ed_row]); break;
                default: break;
                }
            }
        } else {
            if(ev.keycode==KEY_ASCII){
                char c=ev.ascii;
                if(c==':'){ed_pd=false;ed_py=false;ed_colon(&running);if(running)ed_redraw();continue;}

                if(c=='d'){
                    if(ed_pd){ed_delline();ed_pd=false;ed_py=false;}
                    else{ed_pd=true;ed_py=false;}
                    ed_redraw();continue;
                }
                if(c=='y'){
                    if(ed_py){kstrncpy(ed_yank,ed_buf[ed_row],ED_LINE_LEN-1);ed_yank_ok=true;ed_py=false;}
                    else{ed_py=true;ed_pd=false;}
                    ed_redraw();continue;
                }
                ed_pd=false;ed_py=false;

                switch(c){
                case 'i': ed_insert=true; break;
                case 'I': ed_col=0;ed_insert=true; break;
                case 'a': {int l=(int)kstrlen(ed_buf[ed_row]);if(ed_col<l)ed_col++;ed_insert=true;} break;
                case 'A': ed_col=(int)kstrlen(ed_buf[ed_row]);ed_insert=true; break;
                case 'o': ed_newbelow();ed_insert=true; break;
                case 'O': {
                    if(ed_lines<ED_MAX_LINES-1){
                        for(int i=ed_lines;i>ed_row;i--) kstrncpy(ed_buf[i],ed_buf[i-1],ED_LINE_LEN-1);
                        ed_lines++;ed_buf[ed_row][0]=0;ed_col=0;ed_modified=true;ed_insert=true;
                    }
                    break;
                }
                case 'x': ed_delch();ed_modified=true; break;
                case 'D': ed_buf[ed_row][ed_col]=0;ed_modified=true; break;
                case 'p': {
                    if(ed_yank_ok&&ed_lines<ED_MAX_LINES-1){
                        for(int i=ed_lines;i>ed_row+1;i--) kstrncpy(ed_buf[i],ed_buf[i-1],ED_LINE_LEN-1);
                        ed_lines++;ed_row++;kstrncpy(ed_buf[ed_row],ed_yank,ED_LINE_LEN-1);ed_col=0;ed_modified=true;
                    }
                    break;
                }
                case 'G': ed_row=ed_lines-1;ed_col=0; break;
                case 'g': ed_row=0;ed_col=0; break;
                case 'j': if(ed_row<ed_lines-1){ed_row++;ed_clamp();} break;
                case 'k': if(ed_row>0){ed_row--;ed_clamp();} break;
                case 'h': if(ed_col>0)ed_col--; break;
                case 'l': {int l=(int)kstrlen(ed_buf[ed_row]);if(ed_col<l)ed_col++;} break;
                case '0': ed_col=0; break;
                case '$': {int l=(int)kstrlen(ed_buf[ed_row]);ed_col=l>0?l-1:0;} break;
                case 'r': {
                    key_event_t re; do{re=keyboard_wait_event();}while(re.released||re.keycode!=KEY_ASCII);
                    if(re.ascii>=32&&ed_col<(int)kstrlen(ed_buf[ed_row])){ed_buf[ed_row][ed_col]=re.ascii;ed_modified=true;}
                    break;
                }
                default: break;
                }
            } else {
                switch(ev.keycode){
                case KEY_UP:    if(ed_row>0){ed_row--;ed_clamp();} break;
                case KEY_DOWN:  if(ed_row<ed_lines-1){ed_row++;ed_clamp();} break;
                case KEY_LEFT:  if(ed_col>0)ed_col--; break;
                case KEY_RIGHT: {int l=(int)kstrlen(ed_buf[ed_row]);if(ed_col<l)ed_col++;} break;
                case KEY_HOME:  ed_col=0; break;
                case KEY_END:   ed_col=(int)kstrlen(ed_buf[ed_row]); break;
                case KEY_DELETE: ed_delch();ed_modified=true; break;
                default: break;
                }
            }
        }
        ed_redraw();
    }

    vga_clear(); vga_set_color(VGA_COLOR_LIGHT_GREY,VGA_COLOR_BLACK);
    last_exit_code=0;
}

// =============================================================================
// Remaining builtins
// =============================================================================
static void builtin_env(cmd_t *cmd){(void)cmd;
    static const char *k[]={"PATH","HOME","USER","SHELL","TERM","OS","VERSION","ARCH","HOSTNAME","PWD","OLDPWD","PS1","IFS","?",NULL};
    for(int i=0;k[i];i++){const char *v=env_get(k[i]);if(v){term_set_color(TC_GREEN);tprintf("%s",k[i]);term_reset();tprintf("=%s\n",v);}}
    for(int i=0;i<var_count;i++){term_set_color(TC_CYAN);tprintf("%s",shell_vars[i].name);term_reset();tprintf("=%s\n",shell_vars[i].value);}
}
static void builtin_history(void){
    int s=history_count>SHELL_MAX_HISTORY?history_count-SHELL_MAX_HISTORY:0;
    for(int i=s;i<history_count;i++){term_set_color(TC_YELLOW);tprintf("%4d  ",i+1);term_reset();tprintf("%s\n",history[i%SHELL_MAX_HISTORY].line);}
}
static void builtin_uname(cmd_t *cmd){
    bool a=(cmd->argc>1&&kstrcmp(cmd->args[1],"-a")==0);
    tprintf(a?"Eclipse32 eclipse 1.1.2 #1 x86 i686\n":"Eclipse32\n");
}
static void builtin_free(void){
    extern uint32_t pmm_free_pages_count_get(void);
    extern uint32_t pmm_total_pages_get(void);
    uint32_t fp=pmm_free_pages_count_get(),tp=pmm_total_pages_get();
    term_set_color(TC_CYAN);tprintf("%-16s %10s %10s %10s\n","","total","used","free");term_reset();
    tprintf("%-16s %10u %10u %10u  (KB)\n","Mem:",tp*4,(tp-fp)*4,fp*4);
}
static void builtin_uptime(void){
    uint32_t ms=pit_ms(),s=ms/1000,m=s/60; s%=60;
    uint32_t h=m/60; m%=60; uint32_t d=h/24; h%=24;
    tprintf("up "); if(d)tprintf("%u day%s, ",d,d>1?"s":"");
    if(h)tprintf("%02u:",h); tprintf("%02u:%02u\n",m,s);
}
static void builtin_date(void){
    uint32_t ms=pit_ms(),s=ms/1000,m=s/60; s%=60; uint32_t h=m/60; m%=60;
    tprintf("Uptime: %02u:%02u:%02u  (%u ms)\n",h,m,s,ms);
}
static void builtin_sleep(cmd_t *cmd){
    if(cmd->argc<2){tprintf("usage: sleep <ms>\n");return;}
    pit_sleep_ms((uint32_t)katoi(cmd->args[1])); last_exit_code=0;
}
static bool is_builtin(const char *n);

static bool parse_ipv4(const char *s, ipv4_addr_t *out) {
    int octet[4] = {0,0,0,0};
    int idx = 0;
    int val = -1;
    for (const char *p = s; ; p++) {
        if (*p >= '0' && *p <= '9') {
            if (val < 0) val = 0;
            val = val * 10 + (*p - '0');
            if (val > 255) return false;
        } else if (*p == '.' || *p == '\0') {
            if (val < 0) return false;
            if (idx >= 4) return false;
            octet[idx++] = val;
            val = -1;
            if (*p == '\0') break;
        } else {
            return false;
        }
    }
    if (idx != 4) return false;
    *out = ipv4_make((uint8_t)octet[0], (uint8_t)octet[1], (uint8_t)octet[2], (uint8_t)octet[3]);
    return true;
}

static volatile bool     ping_reply_seen = false;
static volatile uint32_t ping_reply_rtt  = 0;
static volatile ipv4_addr_t ping_reply_from;

static void ping_on_reply(ipv4_addr_t from, uint16_t identifier, uint16_t seq, uint32_t rtt_ms) {
    (void)identifier; (void)seq;
    ping_reply_seen = true;
    ping_reply_rtt = rtt_ms;
    ping_reply_from = from;
}

static void builtin_ping(cmd_t *cmd) {
    if (cmd->argc < 2) { tprintf("usage: ping <ip>\n"); last_exit_code = 1; return; }
    if (!rtl8139_present()) {
        term_set_color(TC_RED);
        tprintf("ping: no network interface available\n");
        term_reset();
        last_exit_code = 1;
        return;
    }

    ipv4_addr_t dst;
    if (!parse_ipv4(cmd->args[1], &dst)) {
        tprintf("ping: invalid address '%s' (expected a.b.c.d)\n", cmd->args[1]);
        last_exit_code = 1;
        return;
    }

    icmp_set_reply_callback(ping_on_reply);

    int count = 4;
    int sent = 0, received = 0;
    uint16_t identifier = 0xE32;

    for (int seq = 0; seq < count; seq++) {
        ping_reply_seen = false;

        // ipv4_send() returns false while ARP is still resolving the next
        // hop — keep polling/retrying the actual send for a short window
        // before giving up on this sequence number. sched_yield() each
        // iteration so TASK_GUI still gets to render frames/handle input
        // while we wait — without it the desktop freezes for the whole
        // ping duration since this loop owns the CPU as TASK_APP.
        bool sent_ok = false;
        uint32_t send_deadline = pit_ms() + 1000;
        while (pit_ms() < send_deadline) {
            if (icmp_send_ping(dst, identifier, (uint16_t)seq)) { sent_ok = true; break; }
            net_poll();
            sched_yield();
        }
        if (!sent_ok) {
            tprintf("Request timeout (ARP) for icmp_seq %d\n", seq);
            continue;
        }
        sent++;

        uint32_t reply_deadline = pit_ms() + 2000;
        while (pit_ms() < reply_deadline) {
            net_poll();
            if (ping_reply_seen) break;
            sched_yield();
        }

        if (ping_reply_seen) {
            received++;
            ipv4_addr_t f = ping_reply_from;
            tprintf("Reply from %d.%d.%d.%d: icmp_seq=%d time=%ums\n",
                    f.b[0], f.b[1], f.b[2], f.b[3], seq, ping_reply_rtt);
        } else {
            tprintf("Request timeout for icmp_seq %d\n", seq);
        }

        if (seq < count - 1) {
            uint32_t gap_deadline = pit_ms() + 500;
            while (pit_ms() < gap_deadline) {
                net_poll();
                sched_yield();
            }
        }
    }

    icmp_set_reply_callback(NULL);

    int loss = sent > 0 ? ((sent - received) * 100) / sent : 100;
    tprintf("\n%d packets transmitted, %d received, %d%% packet loss\n", sent, received, loss);
    last_exit_code = (received > 0) ? 0 : 1;
}

static void builtin_nslookup(cmd_t *cmd) {
    if (cmd->argc < 2) { tprintf("usage: nslookup <hostname>\n"); last_exit_code = 1; return; }
    if (!rtl8139_present()) {
        term_set_color(TC_RED);
        tprintf("nslookup: no network interface available\n");
        term_reset();
        last_exit_code = 1;
        return;
    }

    const char *hostname = cmd->args[1];

    // dns_query() (via udp_send -> ipv4_send) fails immediately if ARP for
    // the gateway hasn't resolved yet — keep retrying the send for a short
    // window instead of giving up on the very first attempt, same pattern
    // as ping's send-retry loop.
    bool sent_ok = false;
    uint32_t send_deadline = pit_ms() + 1000;
    while (pit_ms() < send_deadline) {
        if (dns_query(hostname)) { sent_ok = true; break; }
        net_poll();
        sched_yield();
    }

    if (!sent_ok) {
        term_set_color(TC_RED);
        tprintf("nslookup: failed to send query (ARP not resolved for DNS server?)\n");
        term_reset();
        last_exit_code = 1;
        return;
    }

    tprintf("Looking up %s ...\n", hostname);

    ipv4_addr_t result;
    bool resolved = false;
    bool got_response = false;

    uint32_t deadline = pit_ms() + 3000;
    while (pit_ms() < deadline) {
        net_poll();
        if (dns_poll_result(&result)) {
            resolved = true;
            got_response = true;
            break;
        }
        sched_yield();
    }

    if (resolved) {
        tprintf("Name:    %s\nAddress: %d.%d.%d.%d\n",
                hostname, result.b[0], result.b[1], result.b[2], result.b[3]);
        last_exit_code = 0;
    } else if (got_response) {
        term_set_color(TC_RED);
        tprintf("nslookup: %s: not found (NXDOMAIN or no A record)\n", hostname);
        term_reset();
        last_exit_code = 1;
    } else {
        term_set_color(TC_RED);
        tprintf("nslookup: request timed out\n");
        term_reset();
        last_exit_code = 1;
    }
}

// -----------------------------------------------------------------------------
// get <url> [dest]  — download a file (like wget)
// -----------------------------------------------------------------------------
static void builtin_get(cmd_t *cmd) {
    if (cmd->argc < 2) {
        tprintf("usage: get <url> [dest]\n");
        last_exit_code = 1;
        return;
    }
    if (!rtl8139_present()) {
        term_set_color(TC_RED);
        tprintf("get: no network interface\n");
        term_reset();
        last_exit_code = 1;
        return;
    }

    const char *url = cmd->args[1];

    char dest[SHELL_MAX_PATH];
    if (cmd->argc >= 3) {
        path_resolve(cmd->args[2], dest);
    } else {
        const char *slash = url + kstrlen(url);
        while (slash > url && *slash != '/') slash--;
        if (*slash == '/') slash++;
        if (*slash == '\0') {
            kstrcpy(dest, "/index.html");
        } else {
            dest[0] = '/';
            kstrcpy(dest + 1, slash);
        }
    }

    tprintf("get: %s -> %s\n", url, dest);

    if (http_get_to_file(url, dest, true) == 0) {
        term_set_color(TC_GREEN);
        tprintf("get: saved to %s\n", dest);
        term_reset();
        last_exit_code = 0;
    } else {
        term_set_color(TC_RED);
        tprintf("get: failed\n");
        term_reset();
        last_exit_code = 1;
    }
}

// -----------------------------------------------------------------------------
// getpkg <url>  — download a .ins package and install it
// -----------------------------------------------------------------------------
static void builtin_getpkg(cmd_t *cmd) {
    if (cmd->argc < 2) {
        tprintf("usage: getpkg <url>\n");
        last_exit_code = 1;
        return;
    }
    if (!rtl8139_present()) {
        term_set_color(TC_RED);
        tprintf("getpkg: no network interface\n");
        term_reset();
        last_exit_code = 1;
        return;
    }

    const char *url = cmd->args[1];
    const char *tmp = "/tmp/pkg_download.ins";

    tprintf("getpkg: fetching %s\n", url);
    if (http_get_to_file(url, tmp, true) != 0) {
        term_set_color(TC_RED);
        tprintf("getpkg: download failed\n");
        term_reset();
        last_exit_code = 1;
        return;
    }

    tprintf("getpkg: installing...\n");
    int rc = ins_install_from_file(tmp);
    fat32_delete(tmp);

    if (rc == 0) {
        term_set_color(TC_GREEN);
        tprintf("getpkg: installed OK\n");
        term_reset();
        last_exit_code = 0;
    } else {
        term_set_color(TC_RED);
        tprintf("getpkg: install failed (code %d)\n", rc);
        term_reset();
        last_exit_code = 1;
    }
}

static void builtin_beep(cmd_t *cmd){
    uint32_t hz = 880;
    uint32_t ms = 120;
    if(cmd->argc > 1) hz = (uint32_t)katoi(cmd->args[1]);
    if(cmd->argc > 2) ms = (uint32_t)katoi(cmd->args[2]);
    if(hz < 20) hz = 20;
    if(ms < 10) ms = 10;
    speaker_beep(hz, ms);
    last_exit_code = 0;
}
static void builtin_wc(cmd_t *cmd){
    if(cmd->argc<2){tprintf("usage: wc <file>\n");last_exit_code=1;return;}
    char r[SHELL_MAX_PATH]; path_resolve(cmd->args[1],r);
    int fd=fat32_open(r,FAT32_O_RDONLY);
    if(fd<0){term_set_color(TC_RED);tprintf("wc: %s: No such file\n",cmd->args[1]);term_reset();last_exit_code=1;return;}
    uint32_t lines=0, words=0, bytes=0;
    bool in_word=false;
    char buf[256]; int n;
    while((n=fat32_read(fd,buf,sizeof(buf)))>0){
        for(int i=0;i<n;i++){
            char c=buf[i];
            bytes++;
            if(c=='\n') lines++;
            bool ws=(c==' '||c=='\t'||c=='\n'||c=='\r'||c=='\f'||c=='\v');
            if(ws) in_word=false;
            else if(!in_word){words++;in_word=true;}
        }
    }
    fat32_close(fd);
    tprintf("%u %u %u %s\n", lines, words, bytes, cmd->args[1]);
    last_exit_code=0;
}
static void builtin_which(cmd_t *cmd){
    if(cmd->argc<2){tprintf("usage: which <name>\n");last_exit_code=1;return;}
    const char *name = cmd->args[1];
    if(is_builtin(name)){ tprintf("%s: shell builtin\n", name); last_exit_code=0; return; }
    char p[SHELL_MAX_PATH]; path_resolve(name,p);
    fat32_stat_t st;
    if(fat32_stat(p,&st)==0 && !st.is_dir){ tprintf("%s\n", p); last_exit_code=0; return; }
    term_set_color(TC_RED); tprintf("%s not found\n", name); term_reset();
    last_exit_code=1;
}
static void builtin_clear(void){vga_clear();vga_set_color(VGA_COLOR_LIGHT_GREY,VGA_COLOR_BLACK);}
static void builtin_reboot(void){
    term_set_color(TC_YELLOW);tprintf("Rebooting...\n");term_reset();
    uint8_t g=0x02; while(g&0x02)g=inb(0x64); outb(0x64,0xFE);
    asm volatile("cli; hlt");
}
static void builtin_halt(void){
    term_set_color(TC_RED);tprintf("System halted.\n");term_reset();
    asm volatile("cli; hlt");
}

// =============================================================================
// Script runner
// =============================================================================
static int execute_line(char *line);

int shell_run_script(const char *path){
    if(script_depth>=SHELL_SCRIPT_DEPTH){term_set_color(TC_RED);tprintf("esh: recursion limit\n");term_reset();return 1;}
    char r[SHELL_MAX_PATH]; path_resolve(path,r);
    fat32_stat_t st;
    if(fat32_stat(r,&st)!=0){term_set_color(TC_RED);tprintf("esh: %s: not found\n",path);term_reset();return 1;}
    int fd=fat32_open(r,FAT32_O_RDONLY);
    if(fd<0){term_set_color(TC_RED);tprintf("esh: %s: cannot open\n",path);term_reset();return 1;}
    char *sc=(char*)kmalloc(st.size+2); if(!sc){fat32_close(fd);return 1;}
    int got=fat32_read(fd,sc,st.size); sc[got]=0; fat32_close(fd);
    script_depth++;
    char *l=sc; char lb[SHELL_MAX_LINE];
    while(*l){
        int len=0;
        while(*l&&*l!='\n'&&len<SHELL_MAX_LINE-1) lb[len++]=*l++;
        if(*l=='\n')l++; lb[len]=0;
        char *t=lb; while(*t==' '||*t=='\t')t++;
        if(*t&&*t!='#') execute_line(t);
    }
    kfree(sc); script_depth--; return last_exit_code;
}

// =============================================================================
// Prompt
// =============================================================================
static void print_prompt(void){
    const char *ps1=env_get("PS1"); if(!ps1)ps1="\\u@\\h:\\w$ ";
    while(*ps1){
        if(*ps1=='\\'){
            ps1++;
            switch(*ps1){
            case 'u':{const char *u=env_get("USER");term_set_color(TC_RED);tprintf("%s",u?u:"root");term_reset();}break;
            case 'h':{const char *h=env_get("HOSTNAME");term_set_color(TC_RED);tprintf("%s",h?h:"eclipse");term_reset();}break;
            case 'w': term_set_color(TC_YELLOW);tprintf("%s",cwd[1]==0?"/":cwd);term_reset();break;
            case 'W':{const char *b=kstrrchr(cwd,'/');term_set_color(TC_YELLOW);tprintf("%s",(b&&b[1])?b+1:"/");term_reset();}break;
            case '$': term_set_color(last_exit_code?TC_RED:TC_DEFAULT);tprintf("$");term_reset();break;
            case 'n': tprintf("\n");break;
            default: tprintf("\\%c",*ps1);break;
            }
        } else { char ch[2]={*ps1,0};tprintf("%s",ch); }
        ps1++;
    }
    term_reset();
}

// =============================================================================
// Execution engine
// =============================================================================
static int run_builtin(cmd_t *cmd);

static bool is_builtin(const char *n){
    static const char *b[]={"help","echo","cd","pwd","ls","cat","touch","mkdir","rm","cp","mv",
        "env","export","set","unset","alias","unalias","history","clear","uname","uptime","free",
        "date","sleep","beep","wc","which","source","exit","reboot","halt","true","false","write","ed",
        "install","head","tail","stat","hexdump","basename","dirname","version","len","arch","hostname","wavplay","ping","nslookup","get","getpkg",NULL};
    for(int i=0;b[i];i++) if(kstrcmp(n,b[i])==0) return true;
    return false;
}

static int execute_cmd(cmd_t *cmd){
    if(cmd->argc==0) return 0;
    const char *name=cmd->args[0];
    const char *al=alias_get(name);
    if(al){
        char exp[SHELL_MAX_LINE]; kstrncpy(exp,al,SHELL_MAX_LINE-1);
        for(int i=1;i<cmd->argc;i++){kstrcat(exp," ");kstrcat(exp,cmd->args[i]);}
        return execute_line(exp);
    }
    if(is_builtin(name)) return run_builtin(cmd);
    char sp[SHELL_MAX_PATH]; path_resolve(name,sp);
    char *dot=kstrrchr(name,'.');
    if(dot&&kstrcmp(dot,".sh")==0) return shell_run_script(sp);
    if(dot&&kstrcmp(dot,".e32")==0){
        int rc = e32_exec_file_argv(sp, cmd->argc, cmd->args);
        if (rc < 0) {
            term_set_color(TC_RED); tprintf("esh: failed to execute: %s (err=%d)\n", name, rc); term_reset();
            last_exit_code = 126;
            return 126;
        }
        last_exit_code = rc;
        return rc;
    }
    /* Bare name (no extension) → try /bin/<NAME>.E32 only.
       (Current-directory files must be run with the explicit .E32 extension.) */
    if(!dot) {
        char uname[64];
        int ui = 0;
        while(name[ui] && ui < 63) {
            char c = name[ui];
            if(c >= 'a' && c <= 'z') c -= 32;
            uname[ui++] = c;
        }
        uname[ui] = 0;

        char uname_e32[70];
        kstrncpy(uname_e32, uname, 63);
        kstrcat(uname_e32, ".E32");

        /* fall back to /bin */
        char bin_path[SHELL_MAX_PATH];
        kstrncpy(bin_path, "/bin/", SHELL_MAX_PATH - 1);
        kstrcat(bin_path, uname_e32);
        fat32_stat_t bst;
        if(fat32_stat(bin_path, &bst) == 0 && !bst.is_dir) {
            int rc = e32_exec_file_argv(bin_path, cmd->argc, cmd->args);
            if(rc < 0) {
                term_set_color(TC_RED); tprintf("esh: failed to execute: %s (err=%d)\n", name, rc); term_reset();
                last_exit_code = 126;
                return 126;
            }
            last_exit_code = rc;
            return rc;
        }
    }
    term_set_color(TC_RED);tprintf("esh: command not found: %s\n",name);term_reset();
    last_exit_code=127; return 127;
}

static int run_builtin(cmd_t *cmd){
    const char *n=cmd->args[0];
    if(kstrcmp(n,"help")==0)     builtin_help();
    else if(kstrcmp(n,"echo")==0)    builtin_echo(cmd);
    else if(kstrcmp(n,"cd")==0)      builtin_cd(cmd);
    else if(kstrcmp(n,"pwd")==0)     tprintf("%s\n",cwd);
    else if(kstrcmp(n,"ls")==0)      builtin_ls(cmd);
    else if(kstrcmp(n,"cat")==0)     builtin_cat(cmd);
    else if(kstrcmp(n,"rm")==0)      {builtin_rm(cmd);return last_exit_code;}
    else if(kstrcmp(n,"cp")==0)      {builtin_cp(cmd);return last_exit_code;}
    else if(kstrcmp(n,"mv")==0)      {builtin_mv(cmd);return last_exit_code;}
    else if(kstrcmp(n,"write")==0)   {builtin_write(cmd);return last_exit_code;}
    else if(kstrcmp(n,"ed")==0)      {builtin_ed(cmd);return last_exit_code;}
    else if(kstrcmp(n,"clear")==0)   builtin_clear();
    else if(kstrcmp(n,"history")==0) builtin_history();
    else if(kstrcmp(n,"env")==0)     builtin_env(cmd);
    else if(kstrcmp(n,"uname")==0)   builtin_uname(cmd);
    else if(kstrcmp(n,"uptime")==0)  builtin_uptime();
    else if(kstrcmp(n,"free")==0)    builtin_free();
    else if(kstrcmp(n,"date")==0)    builtin_date();
    else if(kstrcmp(n,"sleep")==0)   builtin_sleep(cmd);
    else if(kstrcmp(n,"beep")==0)    builtin_beep(cmd);
    else if(kstrcmp(n,"wc")==0)      builtin_wc(cmd);
    else if(kstrcmp(n,"which")==0)   builtin_which(cmd);
    else if(kstrcmp(n,"install")==0)  builtin_install_cmd(cmd);
    else if(kstrcmp(n,"ping")==0)     {builtin_ping(cmd);return last_exit_code;}
    else if(kstrcmp(n,"nslookup")==0) {builtin_nslookup(cmd);return last_exit_code;}
    else if(kstrcmp(n,"get")==0)      {builtin_get(cmd);return last_exit_code;}
    else if(kstrcmp(n,"getpkg")==0)   {builtin_getpkg(cmd);return last_exit_code;}
    else if(kstrcmp(n,"head")==0)     builtin_head(cmd);
    else if(kstrcmp(n,"tail")==0)     builtin_tail(cmd);
    else if(kstrcmp(n,"stat")==0)     builtin_stat_cmd(cmd);
    else if(kstrcmp(n,"hexdump")==0)  builtin_hexdump(cmd);
    else if(kstrcmp(n,"basename")==0) builtin_basename(cmd);
    else if(kstrcmp(n,"dirname")==0)  builtin_dirname(cmd);
    else if(kstrcmp(n,"version")==0) builtin_version(cmd);
    else if(kstrcmp(n,"len")==0)      builtin_len(cmd);
    else if(kstrcmp(n,"arch")==0)     builtin_arch(cmd);
    else if(kstrcmp(n,"hostname")==0) builtin_hostname(cmd);
    else if(kstrcmp(n,"wavplay")==0)  builtin_wavplay(cmd);
    else if(kstrcmp(n,"reboot")==0)  builtin_reboot();
    else if(kstrcmp(n,"halt")==0)    builtin_halt();
    else if(kstrcmp(n,"exit")==0){
        last_exit_code=cmd->argc>1?katoi(cmd->args[1]):0;
        shell_running=false; return last_exit_code;
    }
    else if(kstrcmp(n,"source")==0){
        if(cmd->argc>1)shell_run_script(cmd->args[1]);
        else tprintf("usage: source <script.sh>\n");
    }
    else if(kstrcmp(n,"export")==0){
        for(int i=1;i<cmd->argc;i++){
            char *eq=kstrchr(cmd->args[i],'=');
            if(eq){*eq=0;env_set(cmd->args[i],eq+1);*eq='=';}
            else{const char *v=var_get(cmd->args[i]);if(v)env_set(cmd->args[i],v);}
        }
    }
    else if(kstrcmp(n,"set")==0){
        if(cmd->argc<3)tprintf("usage: set <name> <value>\n");
        else var_set(cmd->args[1],cmd->args[2]);
    }
    else if(kstrcmp(n,"unset")==0){ if(cmd->argc>1)env_unset(cmd->args[1]); }
    else if(kstrcmp(n,"alias")==0){
        if(cmd->argc==1){
            for(int i=0;i<alias_count;i++){term_set_color(TC_GREEN);tprintf("alias %s=",aliases[i].name);term_reset();tprintf("'%s'\n",aliases[i].command);}
        } else {
            char *eq=kstrchr(cmd->args[1],'=');
            if(eq&&cmd->argc==2){*eq=0;alias_set(cmd->args[1],eq+1);*eq='=';}
            else if(cmd->argc>=3) alias_set(cmd->args[1],cmd->args[2]);
        }
    }
    else if(kstrcmp(n,"unalias")==0){
        if(cmd->argc>1)
            for(int i=0;i<alias_count;i++)
                if(kstrcmp(aliases[i].name,cmd->args[1])==0){for(int j=i;j<alias_count-1;j++)aliases[j]=aliases[j+1];alias_count--;break;}
    }
    else if(kstrcmp(n,"true")==0) {last_exit_code=0;return 0;}
    else if(kstrcmp(n,"false")==0){last_exit_code=1;return 1;}
    else if(kstrcmp(n,"touch")==0){
        if(cmd->argc>1){
            char r[SHELL_MAX_PATH];path_resolve(cmd->args[1],r);
            int fd=fat32_open(r,FAT32_O_RDWR|FAT32_O_CREAT);
            if(fd>=0)fat32_close(fd);
            else{term_set_color(TC_RED);tprintf("touch: cannot create '%s'\n",cmd->args[1]);term_reset();last_exit_code=1;return 1;}
        }
    }
    else if(kstrcmp(n,"mkdir")==0){
        if(cmd->argc>1){
            char r[SHELL_MAX_PATH];path_resolve(cmd->args[1],r);
            if(fat32_mkdir(r)!=0){term_set_color(TC_RED);tprintf("mkdir: cannot create '%s'\n",cmd->args[1]);term_reset();last_exit_code=1;return 1;}
        }
    }
    last_exit_code=0; return 0;
}

static int execute_line(char *line){
    while(*line==' '||*line=='\t') line++;
    if(!*line||*line=='#') return 0;

    char exp[SHELL_MAX_LINE];
    expand_vars(line,exp,sizeof(exp));

    // &&
    for(int i=0;exp[i];i++){
        if(exp[i]=='&'&&exp[i+1]=='&'){
            exp[i]=0; execute_line(exp);
            if(last_exit_code==0) execute_line(exp+i+2);
            return last_exit_code;
        }
    }
    // ||
    for(int i=0;exp[i];i++){
        if(exp[i]=='|'&&exp[i+1]=='|'){
            exp[i]=0; execute_line(exp);
            if(last_exit_code!=0) execute_line(exp+i+2);
            return last_exit_code;
        }
    }
    // |
    for(int i=0;exp[i];i++){
        if(exp[i]=='|'&&exp[i+1]!='|'){
            exp[i]=0; execute_line(exp); execute_line(exp+i+2); return last_exit_code;
        }
    }

    char cb[SHELL_MAX_LINE]; kstrncpy(cb,exp,SHELL_MAX_LINE-1);
    cmd_t cmd; if(tokenize(cb,&cmd)==0) return 0;

    // Output redirection
    if(cmd.redir_out||cmd.redir_out_append){
        const char *rp=cmd.redir_out?cmd.redir_out:cmd.redir_out_append;
        char r[SHELL_MAX_PATH]; path_resolve(rp,r);
        int fl=FAT32_O_RDWR|FAT32_O_CREAT|(cmd.redir_out?FAT32_O_TRUNC:FAT32_O_APPEND);
        int rfd=fat32_open(r,fl);
        if(rfd<0){term_set_color(TC_RED);tprintf("shell: cannot open '%s' for writing\n",rp);term_reset();last_exit_code=1;return 1;}
        if(cmd.argc>0&&kstrcmp(cmd.args[0],"echo")==0){
            int st=1; bool nl=true;
            if(cmd.argc>1&&kstrcmp(cmd.args[1],"-n")==0){nl=false;st=2;}
            for(int i=st;i<cmd.argc;i++){
                if(i>st)fat32_write(rfd," ",1);
                char ea[SHELL_MAX_LINE]; expand_vars(cmd.args[i],ea,sizeof(ea));
                fat32_write(rfd,ea,(uint32_t)kstrlen(ea));
            }
            if(nl)fat32_write(rfd,"\n",1);
            last_exit_code=0;
        } else { execute_cmd(&cmd); }
        fat32_close(rfd); return last_exit_code;
    }

    return execute_cmd(&cmd);
}

// =============================================================================
// MOTD + main
// =============================================================================
static void print_motd(void){
    term_set_color(TC_CYAN);
    tprintf("  Welcome to Eclipse32\n");
    tprintf("  ====================\n");
    term_reset();
    term_set_color(TC_WHITE);
    tprintf("  Version 1.1.2  |  x86-32  |  esh 1.0\n");
    term_reset();
    tprintf("\n");
    term_set_color(TC_GREEN);
    tprintf("  Type 'help' for a list of commands.\n");
    term_reset();
    tprintf("\n");
}

void shell_main(void){
    alias_set("ll","ls -l");
    alias_set("la","ls -a");
    alias_set("cls","clear");
    alias_set("..","cd ..");
    alias_set("...","cd ../..");
    alias_set("q","exit");
    var_count=0;
    print_motd();

    char line[SHELL_MAX_LINE];
    while(shell_running){
        print_prompt(); tprintf(" ");
        readline(line,sizeof(line));
        int len=(int)kstrlen(line);
        while(len>0&&(line[len-1]==' '||line[len-1]=='\t'||line[len-1]=='\n')) line[--len]=0;
        if(!len) continue;
        history_add(line);
        execute_line(line);
        // Update $?
        char cs[12]; int n=last_exit_code,cl=0;
        if(!n)cs[cl++]='0'; else{while(n&&cl<10){cs[cl++]='0'+n%10;n/=10;} for(int i=0;i<cl/2;i++){char t=cs[i];cs[i]=cs[cl-1-i];cs[cl-1-i]=t;}}
        cs[cl]=0; env_set("?",cs);
    }
    tprintf("\n");
    term_set_color(TC_CYAN); tprintf("Eclipse32 shell exited.\n"); term_reset();
}

// ---- GUI Terminal hook -----------------------------------------------
// Output sink used when running a command from the GUI terminal
void shell_exec_line(const char *line, void (*cb)(const char *, void *), void *ud) {
    int slot = sched_current(); // our own slot -- shell_exec_line always runs
                                 // inside the app task the scheduler already
                                 // switched into (see term_app_runner)
    g_term_cb = cb;
    g_term_ud = ud;
    syscall_set_output_cb(slot, cb, ud);   // E32 stdout → same terminal window

    char buf[256];
    kstrncpy(buf, line, 255); buf[255] = 0;

    cmd_t cmd;
    if (tokenize(buf, &cmd) > 0)
        execute_cmd(&cmd);

    g_term_cb = 0;
    g_term_ud = 0;
    syscall_set_output_cb(slot, NULL, NULL);
}
