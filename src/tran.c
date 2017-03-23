#include <sys/stat.h> 
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>   /* dj: import intptr_t */
#include "ode.h"
#include <dparser.h>
#include <R.h>
#include <Rinternals.h>
#include <R_ext/Rdynload.h>
#include <Rmath.h>
#include "tran.g.d_parser.c"
#define max(a,b) (a)>(b) ? (a):(b)
#define MXSYM 5000
#define MXDER 500
#define MXLEN 1200
#define MXBUF 2400
#define SBPTR sb.s+sb.o
#define SBTPTR sbt.s+sbt.o

#define NOASSIGN "'<-' not supported, use '=' instead or set 'options(RxODE.syntax.assign = TRUE)'."
#define NEEDSEMI "Lines need to end with ';' or to match R's handling of line endings set 'options(RxODE.syntax.require.semicolon = FALSE)'."
#define NEEDPOW "'**' not supported, use '^' instead or set 'options(RxODE.syntax.star.pow = TRUE)'."
#define NODOT "'.' in variables and states not supported, use '_' instead or set 'options(RxODE.syntax.allow.dots = TRUE)'."
#define NOINI0 "'%s(0)' for initilization not allowed.  To allow set 'options(RxODE.syntax.allow.ini0 = TRUE)'."
#define NOSTATE "Defined 'df(%s)/dy(%s)', but '%s' is not a state!"
#define NOSTATEVAR "Defined 'df(%s)/dy(%s)', but '%s' is not a state or variable!"

#include <string.h>
#include <stdlib.h>
#include <stddef.h>

#if (__STDC_VERSION__ >= 199901L)
#include <stdint.h>
#endif

char *repl_str(const char *str, const char *from, const char *to) {
  // From http://creativeandcritical.net/str-replace-c by Laird Shaw
  /* Adjust each of the below values to suit your needs. */

  /* Increment positions cache size initially by this number. */
  size_t cache_sz_inc = 16;
  /* Thereafter, each time capacity needs to be increased,
   * multiply the increment by this factor. */
  const size_t cache_sz_inc_factor = 3;
  /* But never increment capacity by more than this number. */
  const size_t cache_sz_inc_max = 1048576;

  char *pret, *ret = NULL;
  const char *pstr2, *pstr = str;
  size_t i, count = 0;
#if (__STDC_VERSION__ >= 199901L)
  uintptr_t *pos_cache_tmp, *pos_cache = NULL;
#else
  ptrdiff_t *pos_cache_tmp, *pos_cache = NULL;
#endif
  size_t cache_sz = 0;
  size_t cpylen, orglen, retlen, tolen, fromlen = strlen(from);

  /* Find all matches and cache their positions. */
  while ((pstr2 = strstr(pstr, from)) != NULL) {
    count++;

    /* Increase the cache size when necessary. */
    if (cache_sz < count) {
      cache_sz += cache_sz_inc;
      pos_cache_tmp = R_chk_realloc(pos_cache, sizeof(*pos_cache) * cache_sz);
      if (pos_cache_tmp == NULL) {
        goto end_repl_str;
      } else pos_cache = pos_cache_tmp;
      cache_sz_inc *= cache_sz_inc_factor;
      if (cache_sz_inc > cache_sz_inc_max) {
        cache_sz_inc = cache_sz_inc_max;
      }
    }

    pos_cache[count-1] = pstr2 - str;
    pstr = pstr2 + fromlen;
  }

  orglen = pstr - str + strlen(pstr);

  /* Allocate memory for the post-replacement string. */
  if (count > 0) {
    tolen = strlen(to);
    retlen = orglen + (tolen - fromlen) * count;
  } else        retlen = orglen;
  ret = R_chk_calloc(1,retlen + 1);
  if (ret == NULL) {
    goto end_repl_str;
  }

  if (count == 0) {
    /* If no matches, then just duplicate the string. */
    strcpy(ret, str);
  } else {
    /* Otherwise, duplicate the string whilst performing
     * the replacements using the position cache. */
    pret = ret;
    memcpy(pret, str, pos_cache[0]);
    pret += pos_cache[0];
    for (i = 0; i < count; i++) {
      memcpy(pret, to, tolen);
      pret += tolen;
      pstr = str + pos_cache[i] + fromlen;
      cpylen = (i == count-1 ? orglen : pos_cache[i+1]) - pos_cache[i] - fromlen;
      memcpy(pret, pstr, cpylen);
      pret += cpylen;
    }
    ret[retlen] = '\0';
  }

 end_repl_str:
  /* Free the cache and return the post-replacement string,
   * which will be NULL in the event of an error. */
  Free(pos_cache);
  return ret;
}

// from mkdparse_tree.h
typedef void (print_node_fn_t)(int depth, char *token_name, char *token_value, void *client_data);

int R_get_option(const char *option, int def){
  SEXP s, t;
  int ret;
  PROTECT(t = s = allocList(3));
  SET_TYPEOF(s, LANGSXP);
  SETCAR(t, install("getOption")); t = CDR(t);
  SETCAR(t, mkString(option)); t = CDR(t);
  if (def){
    SETCAR(t, ScalarLogical(1));
  } else {
    SETCAR(t, ScalarLogical(0));
  }
  ret = INTEGER(eval(s,R_GlobalEnv))[0];
  UNPROTECT(1);
  return ret;
}

// Taken from dparser and changed to use R_alloc
int r_buf_read(const char *pathname, char **buf, int *len) {
  struct stat sb;
  int fd;

  *buf = 0;
  *len = 0;
  fd = open(pathname, O_RDONLY);
  if (fd <= 0) 
    return -1;
  memset(&sb, 0, sizeof(sb));
  fstat(fd, &sb);
  *len = sb.st_size;
  *buf = (char*)R_alloc(*len + 2,sizeof(char));
  // MINGW likes to convert cr lf => lf which messes with the size
  size_t real_size = read(fd, *buf, *len);
  (*buf)[real_size] = 0;
  (*buf)[real_size + 1] = 0;
  *len = real_size;
  close(fd);
  return *len;
}

// Taken from dparser and changed to use R_alloc
char * r_sbuf_read(const char *pathname) {
  char *buf;
  int len;
  if (r_buf_read(pathname, &buf, &len) < 0)
    return NULL;
  return buf;
}


// Taken from dparser and changed to use Calloc
char * rc_dup_str(const char *s, const char *e) {
  int l = e ? e-s : strlen(s);
  char *ss = Calloc(l+1,char);
  memcpy(ss, s, l);
  ss[l] = 0;
  return ss;
}

// Taken from dparser and changed to use R_alloc
char * r_dup_str(const char *s, const char *e) {
  int l = e ? e-s : strlen(s);
  char *ss = (char*)R_alloc(l+1,sizeof(char));
  memcpy(ss, s, l);
  ss[l] = 0;
  return ss;
}

int rx_syntax_error = 0, rx_suppress_syntax_info=0, rx_podo = 0;
static void trans_syntax_error_report_fn(char *err) {
  if (!rx_suppress_syntax_info)
    Rprintf("%s\n",err);
  rx_syntax_error = 1;
}


extern D_ParserTables parser_tables_RxODE;

unsigned int found_jac = 0, found_print = 0;
int rx_syntax_assign = 0, rx_syntax_star_pow = 0,
  rx_syntax_require_semicolon = 0, rx_syntax_allow_dots = 0,
  rx_syntax_allow_ini0 = 1, rx_syntax_allow_ini = 1;

char s_aux_info[64*MXSYM];


typedef struct symtab {
  char ss[64*MXSYM];                     /* symbol string: all vars*/
  char de[64*MXSYM];             /* symbol string: all Des*/
  int deo[MXSYM];        /* offest of des */
  int vo[MXSYM];        /* offset of symbols */
  int lh[MXSYM];        /* lhs symbols? =9 if a state var*/
  int ini[MXSYM];        /* initial variable assignment =2 if there are two assignments */
  int ini0[MXSYM];        /* state initial variable assignment =2 if there are two assignments */
  int di[MXDER];        /* ith of state vars */
  int fdi[MXDER];        /* Functional initilization of state variable */
  int nv;                       /* nbr of symbols */
  int ix;                       /* ith of curr symbol */
  int id;                       /* ith of curr symbol */
  int fn;                       /* curr symbol a fn?*/
  int nd;                       /* nbr of dydt */
  int pos;
  int pos_de;
  int ini_i; // #ini
  int statei; // # states
  int fdn; // # conditional states
  int sensi;
  int li; // # lhs
  int pi; // # param
  // Save Jacobian information
  int df[MXSYM];
  int dy[MXSYM];
  int sdfdy[MXSYM];
  int cdf;
  int ndfdy;
  int maxtheta;
  int maxeta;
} symtab;
symtab tb;

typedef struct sbuf {
  char s[MXBUF];        /* curr print buffer */
  int o;                        /* offset of print buffer */
} sbuf;
sbuf sb;                        /* buffer w/ current parsed & translated line */
                                /* to be stored in a temp file */
sbuf sbt; 

char *extra_buf, *model_prefix, *md5, *out2, *out3;

static FILE *fpIO, *fpIO2;

/* new symbol? if no, find it's ith */
int new_or_ith(const char *s) {
  int i, len, len_s=strlen(s);

  if (tb.fn) return 0;
  if (!strcmp("t", s)) return 0;
  if (!strcmp("time", s)) return 0;
  if (!strcmp("podo", s)) return 0;
  if (!strcmp("tlast", s)) return 0;
  if (!tb.nv) return 1;

  for (i=0; i<tb.nv; i++) {
    len = tb.vo[i+1] - tb.vo[i] - 1;  /* -1 for added ',' */
    if (!strncmp(tb.ss+tb.vo[i], s, max(len, len_s))) { /* note we need take the max in order not to match a sub-string */
      tb.ix = i;
      return 0;
    }
  }
  return 1;
}

int new_de(const char *s){
  int i, len, len_s=strlen(s);
  for (i=0; i<tb.nd; i++) {
    len = tb.deo[i+1] - tb.deo[i] - 1;
    if (!strncmp(tb.de+tb.deo[i], s, max(len, len_s))) { /* note we need take the max in order not to match a sub-string */
      tb.id = i;
      return 0;
    }
  }
  return 1;
}

void wprint_node(int depth, char *name, char *value, void *client_data) {
  int i;
  if (!strcmp("time",value)){
    sprintf(SBPTR, "t");
    sprintf(SBTPTR, "t");
    sb.o += 1;
    sbt.o += 1;
  } else if (!strcmp("podo",value)){
    sprintf(SBPTR, "podo()");
    sprintf(SBTPTR, "podo");
    sb.o  += 6;
    sbt.o += 4;
    rx_podo = 1;
  } else if (!strcmp("tlast",value)){
    sprintf(SBPTR, "tlast()");
    sprintf(SBTPTR, "tlast");
    sb.o  += 7;
    sbt.o += 5;
    
  } else if (!strcmp("identifier",name) && !strcmp("gamma",value)){
    sprintf(SBPTR, "lgammafn");
    sb.o += 8;
    sprintf(SBTPTR, "lgammafn");
    sbt.o += 8;
  } else if (!strcmp("identifier",name) && !strcmp("lfactorial",value)){
    sprintf(SBPTR, "lgamma1p");
    sb.o += 8;
    sprintf(SBTPTR, "lgamma1p");
    sbt.o += 8;
  } else if (!strcmp("identifier",name) && !strcmp("log",value)){
    sprintf(SBPTR, "_safe_log");
    sb.o += 9;
    sprintf(SBTPTR, "log");
    sbt.o += 3;
  } else {
    // Apply fix for dot.syntax
    for (i = 0; i < strlen(value); i++){
      if (value[i] == '.' && !strcmp("identifier_r",name)){
        sprintf(SBPTR, "_DoT_");
        sprintf(SBTPTR, ".");
        if (!rx_syntax_allow_dots){
          trans_syntax_error_report_fn(NODOT);
        }
        sb.o += 5;
        sbt.o++;
      } else {
        sprintf(SBPTR, "%c", value[i]);
        sprintf(SBTPTR, "%c", value[i]);
        sb.o++;
        sbt.o++;
      }
    }
  }
}

void wprint_parsetree(D_ParserTables pt, D_ParseNode *pn, int depth, print_node_fn_t fn, void *client_data) {
  char *name = (char*)pt.symbols[pn->symbol].name;
  int nch = d_get_number_of_children(pn), i, k, ii, found, safe_zero = 0;
  char *value = (char*)rc_dup_str(pn->start_loc.s, pn->end);
  char buf[512];
  if ((!strcmp("identifier", name) || !strcmp("identifier_r", name) ||
       !strcmp("identifier_r_no_output",name)) &&
      new_or_ith(value)) {
    /* printf("[%d]->%s\n",tb.nv,value); */
    sprintf(tb.ss+tb.pos, "%s,", value);
    tb.pos += strlen(value)+1;
    tb.vo[++tb.nv] = tb.pos;
  }
  if (!strcmp("(", name) ||
      !strcmp(")", name) ||
      !strcmp(",", name)
      ) {
    sprintf(SBPTR, "%s",name);
    sb.o++;
    sprintf(SBTPTR,"%s",name);
    sbt.o++;
  }
  if (!strcmp("identifier", name) ||
      !strcmp("identifier_r", name) ||
      !strcmp("constant", name) ||
      !strcmp("+", name) ||
      !strcmp("-", name) ||
      !strcmp("*", name) ||
      !strcmp("/", name) ||

      !strcmp("&&", name) ||
      !strcmp("||", name) ||
      !strcmp("!=", name) ||
      !strcmp("==", name) ||
      !strcmp("<=", name) ||
      !strcmp(">=", name) ||
      !strcmp("!", name) ||
      !strcmp("<", name) ||
      !strcmp(">", name) ||

      !strcmp("=", name)
      )
    fn(depth, name, value, client_data);

  // Operator synonyms  
  if (!strcmp("<-",name)){
    sprintf(SBPTR," =");
    sb.o += 2;
    sprintf(SBTPTR,"=");
    sbt.o++;
  }
  
  if (!strcmp("|",name)){
    sprintf(SBPTR," ||");
    sb.o += 3;
    sprintf(SBTPTR,"||");
    sbt.o += 2;
  }

  if (!strcmp("&",name)){
    sprintf(SBPTR," &&");
    sb.o += 3;
    sprintf(SBTPTR,"&&");
    sbt.o += 2;
  }

  if (!strcmp("<>",name) ||
      !strcmp("~=",name) ||
      !strcmp("/=",name) 
      ){
    sprintf(SBPTR," !=");
    sb.o += 3;
    sprintf(SBTPTR,"!=");
    sbt.o += 2;
  }
  Free(value);
  
  depth++;
  if (nch != 0) {
    if (!strcmp("power_expression", name)) {
      sprintf(SBPTR, " R_pow(");
      sb.o += 7;
    }
    for (i = 0; i < nch; i++) {
      if (!rx_syntax_assign  &&
          ((!strcmp("derivative", name) && i == 4) ||
           (!strcmp("jac", name) && i == 6) ||
           (!strcmp("dfdy", name) && i == 6))) {
        D_ParseNode *xpn = d_get_child(pn,i);
        char *v = (char*)rc_dup_str(xpn->start_loc.s, xpn->end);
        if (!strcmp("<-",v)){
          trans_syntax_error_report_fn(NOASSIGN);
        }
        Free(v);
        continue;
      }
      if (!strcmp("derivative", name) && i< 2) continue;
      if (!strcmp("der_rhs", name)    && i< 2) continue;
      if (!strcmp("derivative", name) && i==3) continue;
      if (!strcmp("der_rhs", name)    && i==3) continue;
      if (!strcmp("derivative", name) && i==4) continue;
      
      if (!strcmp("jac", name)     && i< 2)   continue;
      if (!strcmp("jac_rhs", name) && i< 2)   continue;
      if (!strcmp("jac", name)     && i == 3) continue;
      if (!strcmp("jac_rhs", name) && i == 3) continue;
      if (!strcmp("jac", name)     && i == 5) continue;
      if (!strcmp("jac_rhs", name) && i == 5) continue;
      if (!strcmp("jac", name)     && i == 6) continue;

      if (!strcmp("dfdy", name)     && i< 2)   continue;
      if (!strcmp("dfdy_rhs", name) && i< 2)   continue;
      if (!strcmp("dfdy", name)     && i == 3) continue;
      if (!strcmp("dfdy_rhs", name) && i == 3) continue;
      if (!strcmp("dfdy", name)     && i == 5) continue;
      if (!strcmp("dfdy_rhs", name) && i == 5) continue;
      if (!strcmp("dfdy", name)     && i == 6) continue;
      if (!strcmp("ini0", name)     && i == 1) continue;

      if (!strcmp("transit2", name) && i == 1) continue;
      if (!strcmp("transit3", name) && i == 1) continue;

      if (!strcmp("lfactorial",name) && i != 1) continue;
      if (!strcmp("factorial",name) && i != 0) continue;

      if ((!strcmp("theta",name) || !strcmp("eta",name)) && i != 2) continue;
      
      tb.fn = (!strcmp("function", name) && i==0) ? 1 : 0;
      D_ParseNode *xpn = d_get_child(pn,i);
      
      if (!strcmp("theta",name)){
        char *v = (char*)rc_dup_str(xpn->start_loc.s, xpn->end);
        sprintf(buf,"_THETA_%s_",v);
	ii = strtoimax(v,NULL,10);
	if (ii > tb.maxtheta){
	  tb.maxtheta =ii;
	}
	if (new_or_ith(buf)){
          sprintf(tb.ss+tb.pos, "%s,", buf);
          tb.pos += strlen(buf)+1;
          tb.vo[++tb.nv] = tb.pos;
        }
        sprintf(SBPTR,"_THETA_%s_",v);
        sprintf(SBTPTR,"THETA[%s]",v);
        sb.o = strlen(sb.s);
        sbt.o = strlen(sbt.s);
        Free(v);
        continue;
      }

      if (!strcmp("eta",name)){
        char *v = (char*)rc_dup_str(xpn->start_loc.s, xpn->end);
	ii = strtoimax(v,NULL,10);
        if (ii > tb.maxeta){
          tb.maxeta =ii;
        }
        sprintf(buf,"_ETA_%s_",v);
        if (new_or_ith(buf)){
	  sprintf(tb.ss+tb.pos, "%s,", buf);
          tb.pos += strlen(buf)+1;
          tb.vo[++tb.nv] = tb.pos;
        }
        sprintf(SBPTR,"_ETA_%s_",v);
        sprintf(SBTPTR,"ETA[%s]",v);
        sb.o = strlen(sb.s);
        sbt.o = strlen(sbt.s);
        Free(v);
        continue;
      }
      
      wprint_parsetree(pt, xpn, depth, fn, client_data);
      if (rx_syntax_require_semicolon && !strcmp("end_statement",name) && i == 0){
        if (xpn->start_loc.s ==  xpn->end){
          trans_syntax_error_report_fn(NEEDSEMI);
        } 
      }

      if (!strcmp("decimalint",name)){
	sprintf(SBPTR,".0");
        sb.o += 2;
      }

      if (!strcmp("mult_part",name)){
	char *v = (char*)rc_dup_str(xpn->start_loc.s, xpn->end);
	if (i == 0){
	  if (!strcmp("/",v)){
	    sprintf(SBPTR,"_safe_zero(");
            sb.o += 11;
            safe_zero = 1;
	  } else {
	    safe_zero = 0;
	  }
	}
	if (i == 1){
	  if (safe_zero){
	    sprintf(SBPTR,")");
	    sb.o++;
	  }
	  safe_zero = 0;
	}
	Free(v);
      }
      if (!strcmp("print_command",name)){
        found_print = 1;
        char *v = (char*)rc_dup_str(xpn->start_loc.s, xpn->end);
        if  (!strncmp(v,"print",5)){
          fprintf(fpIO,"full_print;\n");
          fprintf(fpIO2,"print;\n");
        } else {
          fprintf(fpIO, "%s;\n", v);
          fprintf(fpIO2,"%s;\n", v);
        }
        /* sprintf(sb.s,"%s",v); */
        /* sb.o = str; */
        Free(v);
      }
      if (!strcmp("printf_statement",name)){
        char *v = (char*)rc_dup_str(xpn->start_loc.s, xpn->end);
        if (i == 0){
          if (!strncmp(v,"ode0",4)){
            sprintf(sb.s,"ODE0_Rprintf(");
            sb.o = 12;
            sprintf(sbt.s,"ode0_printf(");
            sbt.o = 12;
          } else if (!strncmp(v,"jac0",4)) {
            sprintf(sb.s,"JAC0_Rprintf(");
            sb.o = 12;
            sprintf(sbt.s,"jac0_printf(");
            sbt.o = 12;
          } else if (!strncmp(v,"ode",3)){
            sprintf(sb.s,"ODE_Rprintf(");
            sb.o = 11;
            sprintf(sbt.s,"ode_printf(");
            sbt.o = 11;
          } else if (!strncmp(v,"jac",3)){
            sprintf(sb.s,"JAC_Rprintf(");
            sb.o = 11;
            sprintf(sbt.s,"jac_printf(");
            sbt.o = 11;
          } else if (!strncmp(v,"lhs",3)){
            sprintf(sb.s,"LHS_Rprintf(");
            sb.o = 11;
            sprintf(sbt.s,"lhs_printf(");
            sbt.o = 11;
          } else {
            sprintf(sb.s,"Rprintf(");
            sb.o = 7;
            sprintf(sbt.s,"printf(");
            sbt.o = 6;
          }
        }
        if (i == 2){
          sprintf(SBPTR,"%s",v);
          sb.o = strlen(sb.s);
          sprintf(SBTPTR,"%s",v);
          sbt.o = strlen(sbt.s);
        }
        if (i == 4){
          fprintf(fpIO,  "%s;\n", sb.s);
          fprintf(fpIO2, "%s;\n", sbt.s);
        }
        Free(v);
        continue;
      } 

      if ( (!strcmp("jac",name) || !strcmp("jac_rhs",name) ||
            !strcmp("dfdy",name) || !strcmp("dfdy_rhs",name)) && i == 2){
        found_jac = 1;
        char *v = (char*)rc_dup_str(xpn->start_loc.s, xpn->end);
        if (!strcmp("jac_rhs",name) || !strcmp("dfdy_rhs",name)){
          // Continuation statement
          sprintf(SBPTR,"__PDStateVar__[[%s,",v);
          sprintf(SBTPTR,"df(%s)/dy(",v);
        } else {
          // New statment
          sb.o = 0;
          sbt.o = 0;
          sprintf(sb.s,"__PDStateVar__[[%s,",v);
          sprintf(sbt.s,"df(%s)/dy(",v);
	  new_or_ith(v);
	  tb.cdf = tb.ix;
        }
        sb.o = strlen(sb.s);
        sbt.o = strlen(sbt.s);
        Free(v);
        continue;
      }
      if (!strcmp("factorial_exp",name) && i == 0){
        sb.o--;
        sprintf(SBPTR, "exp(lgamma1p(");
        sb.o += 13;
        continue;
      }
      if (!strcmp("lfactorial_exp",name) && i == 0){
        sprintf(SBPTR, "lgamma1p(");
        sb.o += 9;
        sprintf(SBTPTR, "log((");
        sbt.o += 5;
        continue;
      }
      if (!strcmp("lfactorial_exp",name) && i == 2){
        sprintf(SBPTR, ")");
        sb.o++;
        sprintf(SBTPTR, ")!)");
        sbt.o += 3;
        continue;
      }
      if (!strcmp("factorial_exp",name) && i == 3) {
        sb.o--;
        sprintf(SBPTR, ")");
        sb.o++;
        sprintf(SBTPTR, "!");
        sbt.o++;
        continue;
      }      
      if (!strcmp("factorial",name)){
        char *v = (char*)rc_dup_str(xpn->start_loc.s, xpn->end);
        sprintf(SBPTR, "exp(lgamma1p(%s))",v);
        sprintf(SBTPTR, "%s!",v);
        sb.o = strlen(sb.s);
        sbt.o = strlen(sbt.s);
        Free(v);
        continue;
      }
      if (!strcmp("lfactorial",name)){
        char *v = (char*)rc_dup_str(xpn->start_loc.s, xpn->end);
        sprintf(SBPTR, "lgamma1p(%s)",v);
        sprintf(SBTPTR, "log(%s!)",v);
        sb.o = strlen(sb.s);
        sbt.o = strlen(sbt.s);
        Free(v);
        continue;
      }
      if ((!strcmp("jac",name)  || !strcmp("jac_rhs",name) ||
           !strcmp("dfdy",name) || !strcmp("dfdy_rhs",name)) && i == 4){
        char *v = (char*)rc_dup_str(xpn->start_loc.s, xpn->end);
	ii = 0;
	if (strstr(v,"THETA[") != NULL){
	  sprintf(buf,"_THETA_%.*s_",strlen(v)-7,v+6);
	  sprintf(SBTPTR, "%s)",v);
          sbt.o = strlen(sbt.s);
	  sprintf(SBPTR, "%s]]",buf);
          sb.o = strlen(sb.s);
	  ii = 1;
	} else if (strstr(v,"ETA[") != NULL) {
	  sprintf(buf,"_ETA_%.*s_",strlen(v)-5,v+4);
          sprintf(SBTPTR, "%s)",v);
          sbt.o = strlen(sbt.s);
          sprintf(SBPTR, "%s]]",buf);
          sb.o = strlen(sb.s);
          ii = 1;
        } else {
	  sprintf(SBPTR, "%s]]",v);
          sb.o = strlen(sb.s);
          sprintf(SBTPTR, "%s)",v);
          sbt.o = strlen(sbt.s);
        }
        if (!strcmp("jac",name) ||
            strcmp("dfdy",name) == 0){
          sprintf(SBPTR ," = ");
          sb.o += 3;
          sprintf(SBTPTR ,"=");
          sbt.o += 1;
	  if (ii == 1){
	    new_or_ith(buf);
          } else {
	    new_or_ith(v);
          }
	  found = -1;
	  for (ii = 0; ii < tb.ndfdy; ii++){
            if (tb.df[ii] == tb.cdf && tb.dy[ii] == tb.ix){
	      found = ii;
	      break;
	    }
	  }
	  if (found < 0){
            tb.df[tb.ndfdy] = tb.cdf;
	    tb.dy[tb.ndfdy] = tb.ix;
	    tb.ndfdy = tb.ndfdy+1;
	    tb.cdf = -1;
          }
        }
        Free(v);
        continue;
      }
      
      //inits
      if (!strcmp("selection_statement", name) && i==1) {
        sprintf(sb.s, "if (");
        sb.o = strlen(sb.s);
        sprintf(sbt.s, "if (");
        sbt.o = strlen(sbt.s);
        continue;
      }
      if (!strcmp("selection_statement", name) && i==3) {
        sprintf(SBPTR, " {");
        sb.o += 2;
        sprintf(SBTPTR, "{");
        sbt.o += 1;
        fprintf(fpIO,  "%s\n", sb.s);
        fprintf(fpIO2, "%s\n", sbt.s);
        continue;
      }
      if (!strcmp("selection_statement__8", name) && i==0) {
        fprintf(fpIO,  "}\nelse {\n");
        fprintf(fpIO2, "}\nelse {\n");
        continue;
      }

      if (!strcmp("power_expression", name) && i==0) {
        sprintf(SBPTR, ",");
        sb.o++;
        sprintf(SBTPTR, "^");
        sbt.o++;
      }
      if (!rx_syntax_star_pow && i == 1 &&!strcmp("power_expression", name)){
        char *v = (char*)rc_dup_str(xpn->start_loc.s, xpn->end);
        if (!strcmp("**",v)){
          trans_syntax_error_report_fn(NEEDPOW);
        }
        Free(v);
      }
      if (!strcmp("transit2", name) && i == 0){
        sprintf(SBPTR, "_transit3(t,");
        sb.o += 12;
        sprintf(SBTPTR,"transit(");
        sbt.o += 8;
        rx_podo = 1;
      }
      if (!strcmp("transit3", name) && i == 0){
        sprintf(SBPTR, "_transit4(t,");
        sb.o += 12;
        sprintf(SBTPTR,"transit(");
        sbt.o += 8;
        rx_podo = 1;
      }
      if (!strcmp("derivative", name) && i==2) {
        /* sprintf(sb.s, "__DDtStateVar__[%d] = InfusionRate(%d) +", tb.nd, tb.nd); */
        /* sb.o = strlen(sb.s); */
        char *v = (char*)rc_dup_str(xpn->start_loc.s, xpn->end);
        if (new_de(v)){
          sprintf(sb.s, "__DDtStateVar__[%d] = _InfusionRate(%d) + ", tb.nd, tb.nd);
          sb.o = strlen(sb.s);
          sprintf(sbt.s, "d/dt(%s)=", v);
          sbt.o = strlen(sbt.s);
	  new_or_ith(v);
          /* Rprintf("%s; tb.ini = %d; tb.ini0 = %d; tb.lh = %d\n",v,tb.ini[tb.ix],tb.ini0[tb.ix],tb.lh[tb.ix]); */
          if  ((tb.ini[tb.ix] == 1 && tb.ini0[tb.ix] == 0) || tb.lh[tb.ix] == 1){
            sprintf(buf,"Cannot assign state variable %s; For initial condition assigment use '%s(0) = #'.\n",v,v);
            trans_syntax_error_report_fn(buf);
          }
          tb.lh[tb.ix] = 9;
          tb.di[tb.nd] = tb.ix;
          sprintf(tb.de+tb.pos_de, "%s,", v);
          tb.pos_de += strlen(v)+1;
          tb.deo[++tb.nd] = tb.pos_de;
        } else {
	  new_or_ith(v);
	  /* printf("de[%d]->%s[%d]\n",tb.id,v,tb.ix); */
          sprintf(sb.s, "__DDtStateVar__[%d] = ", tb.id);
          sb.o = strlen(sb.s);
          sprintf(sbt.s, "d/dt(%s)=", v);
          sbt.o = strlen(sbt.s);
        }
        Free(v);
        continue;
      }
      if (!strcmp("der_rhs", name)) {
        char *v = (char*)rc_dup_str(xpn->start_loc.s, xpn->end);
        if (new_de(v)){
          sprintf(buf,"Tried to use d/dt(%s) before it was defined",v);
          trans_syntax_error_report_fn(buf);
        } else {
          sprintf(SBPTR, "__DDtStateVar__[%d]", tb.id);
          sb.o = strlen(sb.s);
          sprintf(SBTPTR, "d/dt(%s)", v);
          sbt.o = strlen(sbt.s);
        }
        Free(v);
        continue;
      }

      if (!strcmp("ini0f", name) && rx_syntax_allow_ini && i == 0){
	sprintf(sb.s,"(__0f__)");
	sb.o = 8;
	sbt.o = 0;
	char *v = (char*)rc_dup_str(xpn->start_loc.s, xpn->end);
	sprintf(SBPTR,"%s",v);
	sb.o = strlen(sb.s);
	sprintf(SBTPTR,"%s(0)",v);
	sbt.o = strlen(sbt.s);
      }

      if ((!strcmp("assignment", name) || !strcmp("ini", name) || !strcmp("ini0", name)) && i==0) {
        char *v = (char*)rc_dup_str(xpn->start_loc.s, xpn->end);
        if ((rx_syntax_allow_ini && !strcmp("ini", name)) || !strcmp("ini0", name)){
          sprintf(sb.s,"(__0__)");
          sb.o = 7;
          for (k = 0; k < strlen(v); k++){
            if (v[k] == '.'){
              if (rx_syntax_allow_dots){
                sprintf(SBPTR,"_DoT_");
                sb.o +=5;
              } else {
                trans_syntax_error_report_fn(NODOT);
              }
            } else {
              sprintf(SBPTR,"%c",v[k]);
              sb.o++;
            }
          }
          if (!strcmp("ini",name) && !new_de(v)){
            sprintf(buf,"Cannot assign state variable %s; For initial condition assigment use '%s(0) ='.\n",v,v);
            trans_syntax_error_report_fn(buf);
          }
          if (!rx_syntax_allow_ini0 && !strcmp("ini0",name)){
            sprintf(buf,NOINI0,v);
            trans_syntax_error_report_fn(buf);
          }
        } else {
          sb.o = 0;
          for (k = 0; k < strlen(v); k++){
            if (v[k] == '.'){
              if (rx_syntax_allow_dots){              
                sprintf(SBPTR,"_DoT_");
                sb.o +=5;
              } else {
                trans_syntax_error_report_fn(NODOT);
              }
            } else {
              sprintf(SBPTR,"%c",v[k]);
              sb.o++;
            }
          }
          if (!new_de(v)){
            sprintf(buf,"Cannot assign state variable %s; For initial condition assigment use '%s(0) ='.\n",v,v);
            trans_syntax_error_report_fn(buf);
          }
        }
        sprintf(sbt.s, "%s", v);
	if (!strcmp("ini0",name)){
	  sprintf(sbt.s,"%s(0)",v);
	}
	sbt.o = strlen(sbt.s);
	new_or_ith(v);
	
        if (!strcmp("assignment", name) || (!rx_syntax_allow_ini && !strcmp("ini", name))){
          tb.lh[tb.ix] = 1;
        } else if (!strcmp("ini", name) || !strcmp("ini0",name)){
          if (tb.ini[tb.ix] == 0){
            // If there is only one initialzation call, then assume
            // this is a parameter with an initial value.
            tb.ini[tb.ix] = 1;
            if (!strcmp("ini0",name)){
	      tb.ini0[tb.ix] = 1;
            } else {
	      tb.ini0[tb.ix] = 0;
              if (strstr(v,"rx_") != NULL){
                tb.lh[tb.ix] = 1;
              }
            }
          } else {
            // There is more than one call to this variable, it is a
            // conditional variabile
            tb.lh[tb.ix] = 1;
            if (!strcmp("ini0", name) && tb.ini0[tb.ix] == 1){
              sprintf(buf,"Cannot have conditional initial conditions for %s",v);
              trans_syntax_error_report_fn(buf);
            }
	    tb.ini0[tb.ix] = 0;
          }
        }
        Free(v);
      }
    }

    if (!strcmp("assignment", name) || !strcmp("ini", name) || !strcmp("derivative", name) || !strcmp("jac",name) || !strcmp("dfdy",name) ||
        !strcmp("ini0",name) || !strcmp("ini0f",name)){
      fprintf(fpIO, "%s;\n", sb.s);
      fprintf(fpIO2, "%s;\n", sbt.s);
    }

    if (!rx_syntax_assign && (!strcmp("assignment", name) || !strcmp("ini", name) || !strcmp("ini0",name) || !strcmp("ini0f",name))){
      if (!strcmp("ini0",name)){
        i = 2;
      } else {
        i = 1;
      }
      D_ParseNode *xpn = d_get_child(pn,i);
      char *v = (char*)rc_dup_str(xpn->start_loc.s, xpn->end);
      if (!strcmp("<-",v)){
        trans_syntax_error_report_fn(NOASSIGN);
      }
      Free(v);
    }
    
    if (!strcmp("selection_statement", name)){
      fprintf(fpIO, "}\n");
      fprintf(fpIO2, "}\n");
    }
    
    if (!strcmp("power_expression", name)) {
      sprintf(SBPTR, ")");
      sb.o++;
    }

  }

}

void retieve_var(int i, char *buf) {
  int len;

  len = tb.vo[i+1] - tb.vo[i] - 1;
  strncpy(buf, tb.ss+tb.vo[i], len);
  buf[len] = 0;
}

void err_msg(int chk, const char *msg, int code)
{
  if(!chk) {
    error("%s",msg);
  }
}

/* when prnt_vars() is called, user defines the behavior in "case" */
void prnt_vars(int scenario, FILE *outpt, int lhs, const char *pre_str, const char *post_str, int show_ode) {
  int i, j, k;
  char buf[64], buf1[64],buf2[64];
  fprintf(outpt, "%s", pre_str);  /* dj: avoid security vulnerability */
  if (scenario == 0){
    // show_ode = 1 dydt
    // show_ode = 2 Jacobian
    // show_ode = 3 Ini statement
    // show_ode = 0 LHS
    if (show_ode == 2 || show_ode == 0){
      //__DDtStateVar_#__
      for (i = 0; i < tb.nd; i++){
	fprintf(outpt,"\t__DDtStateVar_%d__,\n",i);
      }
    }
    // Now get Jacobain information  __PDStateVar_df_dy__ if needed
    if (show_ode != 3){
      for (i = 0; i < tb.ndfdy; i++){
        retieve_var(tb.df[i], buf1);
        retieve_var(tb.dy[i], buf2);
        // This is for dydt/ LHS/ or jacobian for df(state)/dy(parameter)
        if (show_ode == 1 || show_ode == 0 || tb.sdfdy[i] == 1){
          fprintf(outpt,"\t__PDStateVar_%s_SeP_%s__,\n",buf1,buf2);
        }
      }
    }
  }
  for (i=0, j=0; i<tb.nv; i++) {
    if (lhs && tb.lh[i]>0) continue;
    retieve_var(i, buf);
    switch(scenario) {
    case 0:   // Case 0 is for declaring the variables
      fprintf(outpt,"\t");
      for (k = 0; k < strlen(buf); k++){
        if (buf[k] == '.'){
          fprintf(outpt,"_DoT_");
          if (!rx_syntax_allow_dots){
            trans_syntax_error_report_fn(NODOT);
          }
        } else {
          fprintf(outpt,"%c",buf[k]);
        }
      }
      if (i <tb.nv-1)
        fprintf(outpt, ",\n");
      else
        fprintf(outpt, ";\n");
      break;
    case 1:
      // Case 1 is for declaring the par_ptr.
      fprintf(outpt,"\t");
      for (k = 0; k < strlen(buf); k++){
        if (buf[k] == '.'){
          fprintf(outpt,"_DoT_");
          if (!rx_syntax_allow_dots){
            trans_syntax_error_report_fn(NODOT);
          }
        } else {
          fprintf(outpt,"%c",buf[k]);
        }
      }
      fprintf(outpt, " = _par_ptr(%d);\n", j++);
      break;
    default: break;
    }
  }
  fprintf(outpt, "%s", post_str);  /* dj: security calls for const format */
}

void print_aux_info(FILE *outpt, char *model, char *orig_model){
  int i, j, k, islhs,pi = 0,li = 0, o=0, o2=0, statei = 0, ini_i = 0, sensi=0, fdi=0,
    in_str=0;
  char *s2;
  char sLine[MXLEN+1];
  char buf[512], buf2[512];
  for (i=0; i<tb.nv; i++) {
    islhs = tb.lh[i];
    if (islhs>1) continue;      /* is a state var */
    retieve_var(i, buf);
    if (islhs == 1){
      sprintf(s_aux_info+o, "\tSET_STRING_ELT(lhs,%d,mkChar(\"%s\"));\n", li++, buf);
    } else if (strcmp(buf,"pi")){
      for (j = 1; j <= tb.maxtheta;j++){
        sprintf(buf2,"_THETA_%d_",j);
        if (!strcmp(buf,buf2)){
          sprintf(buf,"THETA[%d]",j);
        }
      }
      for (j = 1; j <= tb.maxeta;j++){
        sprintf(buf2,"_ETA_%d_",j);
        if (!strcmp(buf,buf2)){
          sprintf(buf,"ETA[%d]",j);
        }
      }
      sprintf(s_aux_info+o, "\tSET_STRING_ELT(params,%d,mkChar(\"%s\"));\n", pi++, buf);
    }
    o = strlen(s_aux_info);
  }
  for (i=0; i<tb.nd; i++) {                     /* name state vars */
    retieve_var(tb.di[i], buf);
    if (strstr(buf, "rx__sens_")){
      sprintf(s_aux_info+o, "\tSET_STRING_ELT(sens,%d,mkChar(\"%s\"));\n", sensi++, buf);
      o = strlen(s_aux_info);
      sprintf(s_aux_info+o, "\tSET_STRING_ELT(state,%d,mkChar(\"%s\"));\n", statei++, buf);
    } else {
      sprintf(s_aux_info+o, "\tSET_STRING_ELT(state,%d,mkChar(\"%s\"));\n", statei++, buf);
    }
    if (tb.fdi[i]){
      o = strlen(s_aux_info);
      sprintf(s_aux_info+o, "\tSET_STRING_ELT(fn_ini,%d,mkChar(\"%s\"));\n", fdi++, buf);
    }
    o = strlen(s_aux_info);
  }
  for (i=0; i<tb.ndfdy; i++) {                     /* name state vars */
    retieve_var(tb.df[i], buf);
    sprintf(s_aux_info+o, "\tSET_STRING_ELT(dfdy,%d,mkChar(\"df(%s)/dy(", i, buf);
    o = strlen(s_aux_info);
    retieve_var(tb.dy[i], buf);
    for (j = 1; j <= tb.maxtheta;j++){
      sprintf(buf2,"_THETA_%d_",j);
      if (!strcmp(buf,buf2)){
        sprintf(buf,"THETA[%d]",j);
      }
    }
    for (j = 1; j <= tb.maxeta;j++){
      sprintf(buf2,"_ETA_%d_",j);
      if (!strcmp(buf,buf2)){
        sprintf(buf,"ETA[%d]",j);
      }
    }
    sprintf(s_aux_info+o, "%s)\"));\n",buf);
    o = strlen(s_aux_info);
  }
  fprintf(outpt,"extern SEXP %smodel_vars(){\n",model_prefix);
  fprintf(outpt,"\tSEXP lst    = PROTECT(allocVector(VECSXP, 11));\n");
  fprintf(outpt,"\tSEXP names  = PROTECT(allocVector(STRSXP, 11));\n");
  fprintf(outpt,"\tSEXP params = PROTECT(allocVector(STRSXP, %d));\n",pi);
  fprintf(outpt,"\tSEXP lhs    = PROTECT(allocVector(STRSXP, %d));\n",li);
  fprintf(outpt,"\tSEXP state  = PROTECT(allocVector(STRSXP, %d));\n",statei);
  fprintf(outpt,"\tSEXP sens   = PROTECT(allocVector(STRSXP, %d));\n",sensi);
  fprintf(outpt,"\tSEXP fn_ini = PROTECT(allocVector(STRSXP, %d));\n",fdi);
  fprintf(outpt,"\tSEXP dfdy   = PROTECT(allocVector(STRSXP, %d));\n",tb.ndfdy);
  fprintf(outpt,"\tSEXP tran   = PROTECT(allocVector(STRSXP, 12));\n");
  fprintf(outpt,"\tSEXP trann  = PROTECT(allocVector(STRSXP, 12));\n");
  fprintf(outpt,"\tSEXP mmd5   = PROTECT(allocVector(STRSXP, 2));\n");
  fprintf(outpt,"\tSEXP mmd5n  = PROTECT(allocVector(STRSXP, 2));\n");
  fprintf(outpt,"\tSEXP model  = PROTECT(allocVector(STRSXP, 4));\n");
  fprintf(outpt,"\tSEXP modeln = PROTECT(allocVector(STRSXP, 4));\n");
  fprintf(outpt,"%s",s_aux_info);
  // Save for outputting in trans
  tb.pi = pi;
  tb.li = li;
  tb.statei = statei;
  tb.sensi  = sensi;
  fprintf(outpt,"\tSET_STRING_ELT(modeln,0,mkChar(\"model\"));\n");
  fprintf(outpt,"\tSET_STRING_ELT(model,0,mkChar(\"");
  for (i = 0; i < strlen(orig_model); i++){
    if (orig_model[i] == '"'){
      fprintf(outpt,"\\\"");
    } else if (orig_model[i] == ' '){
      fprintf(outpt," ");
    } else if (orig_model[i] == '\n'){
      fprintf(outpt,"\\n");
    } else if (orig_model[i] == '\t'){
      fprintf(outpt,"\\t");
    } else if (orig_model[i] == '\\'){
      fprintf(outpt,"\\\\");
    } else if (orig_model[i] >= 32  && orig_model[i] <= 126){ // ASCII only
      fprintf(outpt,"%c",orig_model[i]);
    }
  }
  fprintf(outpt,"\"));\n");
  fprintf(outpt,"\tSET_STRING_ELT(modeln,1,mkChar(\"normModel\"));\n");
  fpIO2 = fopen(out3 , "r");
  fprintf(outpt,"\tSET_STRING_ELT(model,1,mkChar(\"");
  err_msg((intptr_t) fpIO2, "Error parsing. (Couldn't access out3.txt).\n", -1);
  in_str=0;
  while(fgets(sLine, MXLEN, fpIO2)) {  /* Prefered RxODE -- for igraph */
    for (i = 0; i < strlen(sLine); i++){
      if (sLine[i] == '"'){
	if (in_str==1){
          in_str=0;
        } else {
          in_str=1;
        }
        fprintf(outpt,"\\\"");
      } else if (sLine[i] == '\''){
	if (in_str==1){
          in_str=0;
        } else {
          in_str=1;
        }
        fprintf(outpt,"'");
      } else if (sLine[i] == ' '){
	if (in_str==1){
	  fprintf(outpt," ");
	}
      } else if (sLine[i] == '\n'){
        fprintf(outpt,"\\n");
      } else if (sLine[i] == '\t'){
        fprintf(outpt,"\\t");
      } else if (sLine[i] == '\\'){
	fprintf(outpt,"\\\\");
      } else if (sLine[i] >= 33  && sLine[i] <= 126){ // ASCII only
        fprintf(outpt,"%c",sLine[i]);
      }
    }
  }
  fclose(fpIO2);
  fprintf(outpt,"\"));\n");

  fpIO2 = fopen(out2, "r");
  fprintf(outpt,"\tSET_STRING_ELT(modeln,2,mkChar(\"parseModel\"));\n");
  fprintf(outpt,"\tSET_STRING_ELT(model,2,mkChar(\"");
  err_msg((intptr_t) fpIO2, "Error parsing. (Couldn't access out2.txt).\n", -1);
  while(fgets(sLine, MXLEN, fpIO2)) {  /* Prefered RxODE -- for igraph */
    for (i = 0; i < strlen(sLine); i++){
      if (sLine[i] == '"'){
        fprintf(outpt,"\\\"");
      } else if (sLine[i] == '\n'){
        fprintf(outpt,"\\n");
      } else if (sLine[i] == '\t'){
        fprintf(outpt,"\\t");
      } else if (sLine[i] == '\\'){
        fprintf(outpt,"\\\\");
      } else if (sLine[i] >= 32  && sLine[i] <= 126){ // ASCII only
        fprintf(outpt,"%c",sLine[i]);
      }
    }
  }
  fclose(fpIO2);
  fprintf(outpt,"\"));\n");
  fprintf(outpt,"\tSET_STRING_ELT(modeln,3,mkChar(\"expandModel\"));\n");
  fprintf(outpt,"\tSET_STRING_ELT(model,3,mkChar(\"");
  for (i = 0; i < strlen(model); i++){
    if (model[i] == '"'){
      fprintf(outpt,"\\\"");
    } else if (model[i] == '\n'){
      fprintf(outpt,"\\n");
    } else if (model[i] == '\t'){
      fprintf(outpt,"\\t");
    } else if (model[i] == '\\'){
      fprintf(outpt,"\\\\");
    } else if (model[i] >= 32  && model[i] <= 126){ // ASCII only
      fprintf(outpt,"%c",model[i]);
    }
  }
  fprintf(outpt,"\"));\n");
  fpIO2 = fopen(out2, "r");
  s_aux_info[0] = '\0';
  o    = 0;
  err_msg((intptr_t) fpIO2, "Error parsing. (Couldn't access out2.txt).\n", -1);
  while(fgets(sLine, MXLEN, fpIO2)) { 
    s2 = strstr(sLine,"(__0__)");
    if (s2){
      // See if this is a reclaimed initilization variable.
      for (i=0; i<tb.nv; i++) {
        if (tb.ini[i] == 1 && tb.lh[i] != 1){
          //(__0__)V2 =
          retieve_var(i, buf);
          sprintf(buf2,"(__0__)");
          o2 = 7;
          for (k = 0; k < strlen(buf); k++){
            if (buf[k] == '.'){
              sprintf(buf2+o2,"_DoT_");
              if (!rx_syntax_allow_dots){
                trans_syntax_error_report_fn(NODOT);
              }
              o2+=5;
            } else {
              sprintf(buf2+o2,"%c",buf[k]);
              o2++;
            }
          }
          sprintf(buf2+o2,"=");
          s2 = strstr(sLine,buf2);
          if (s2){
            sprintf(s_aux_info+o,"\tSET_STRING_ELT(inin,%d,mkChar(\"%s\"));\n",ini_i, buf);
            o = strlen(s_aux_info);
            sprintf(s_aux_info+o,"\tREAL(ini)[%d] = %.*s;\n",(int)(ini_i++), strlen(sLine)-strlen(buf2)-2,sLine + strlen(buf2));
            o = strlen(s_aux_info);
            continue;
          }
        }
      }
      continue;
    }
  }
  fclose(fpIO2);
  // putin constants
  for (i=0; i<tb.nv; i++) {
    if (tb.ini[i] == 0 && tb.lh[i] != 1) {
      retieve_var(i, buf);
      // Put in constants
      if  (!strcmp("pi",buf)){
        sprintf(s_aux_info+o,"\tSET_STRING_ELT(inin,%d,mkChar(\"pi\"));\n",ini_i);
        o = strlen(s_aux_info);
        // Use well more digits than double supports
        sprintf(s_aux_info+o,"\tREAL(ini)[%d] = M_PI;\n",ini_i++);
        o = strlen(s_aux_info);
      }
    }
  }
  tb.ini_i = ini_i;
  fprintf(outpt,"\tSEXP ini    = PROTECT(allocVector(REALSXP,%d));\n",ini_i);
  fprintf(outpt,"\tSEXP inin   = PROTECT(allocVector(STRSXP, %d));\n",ini_i);
  fprintf(outpt,"%s",s_aux_info);
  // Vector Names
  fprintf(outpt,"\tSET_STRING_ELT(names,0,mkChar(\"params\"));\n");
  fprintf(outpt,"\tSET_VECTOR_ELT(lst,  0,params);\n");

  fprintf(outpt,"\tSET_STRING_ELT(names,1,mkChar(\"lhs\"));\n");
  fprintf(outpt,"\tSET_VECTOR_ELT(lst,  1,lhs);\n");
  
  fprintf(outpt,"\tSET_STRING_ELT(names,2,mkChar(\"state\"));\n");
  fprintf(outpt,"\tSET_VECTOR_ELT(lst,  2,state);\n");
  
  fprintf(outpt,"\tSET_STRING_ELT(names,3,mkChar(\"trans\"));\n");
  fprintf(outpt,"\tSET_VECTOR_ELT(lst,  3,tran);\n");
  
  fprintf(outpt,"\tSET_STRING_ELT(names,5,mkChar(\"model\"));\n");
  fprintf(outpt,"\tSET_VECTOR_ELT(lst,  5,model);\n");
  
  fprintf(outpt,"\tSET_STRING_ELT(names,4,mkChar(\"ini\"));\n");
  fprintf(outpt,"\tSET_VECTOR_ELT(lst,  4,ini);\n");

  fprintf(outpt,"\tSET_STRING_ELT(names,6,mkChar(\"md5\"));\n");
  fprintf(outpt,"\tSET_VECTOR_ELT(lst,  6,mmd5);\n");

  fprintf(outpt,"\tSET_STRING_ELT(names,7,mkChar(\"podo\"));\n");
  fprintf(outpt,"\tSET_VECTOR_ELT(lst,  7,ScalarLogical(%d));\n",rx_podo);

  fprintf(outpt,"\tSET_STRING_ELT(names,8,mkChar(\"dfdy\"));\n");
  fprintf(outpt,"\tSET_VECTOR_ELT(lst,  8,dfdy);\n");

  fprintf(outpt,"\tSET_STRING_ELT(names,9,mkChar(\"sens\"));\n");
  fprintf(outpt,"\tSET_VECTOR_ELT(lst,  9,sens);\n");
  
  fprintf(outpt,"\tSET_STRING_ELT(names,10,mkChar(\"fn.ini\"));\n");
  fprintf(outpt,"\tSET_VECTOR_ELT(lst,  10,fn_ini);\n");
  
  // md5 values
  fprintf(outpt,"\tSET_STRING_ELT(mmd5n,0,mkChar(\"file_md5\"));\n");
  fprintf(outpt,"\tSET_STRING_ELT(mmd5,0,mkChar(\"%s\"));\n",md5);
  fprintf(outpt,"\tSET_STRING_ELT(mmd5n,1,mkChar(\"parsed_md5\"));\n");
  fprintf(outpt,"\tSET_STRING_ELT(mmd5,1,mkChar(__PARSED_MD5_STR__));\n");
  
  // now trans output
  fprintf(outpt,"\tSET_STRING_ELT(trann,0,mkChar(\"jac\"));\n");
  if (found_jac == 1){
    fprintf(outpt,"\tSET_STRING_ELT(tran,0,mkChar(\"fulluser\"));\n"); // Full User Matrix
  } else {
    fprintf(outpt,"\tSET_STRING_ELT(tran,0,mkChar(\"fullint\"));\n"); // Full Internal Matrix
  }
  fprintf(outpt,"\tSET_STRING_ELT(trann,1,mkChar(\"prefix\"));\n");
  fprintf(outpt,"\tSET_STRING_ELT(tran, 1,mkChar(\"%s\"));\n",model_prefix);

  fprintf(outpt,"\tSET_STRING_ELT(trann,2,mkChar(\"dydt\"));\n");
  fprintf(outpt,"\tSET_STRING_ELT(tran, 2,mkChar(\"%sdydt\"));\n",model_prefix);

  fprintf(outpt,"\tSET_STRING_ELT(trann,3,mkChar(\"calc_jac\"));\n");
  fprintf(outpt,"\tSET_STRING_ELT(tran, 3,mkChar(\"%scalc_jac\"));\n",model_prefix);

  fprintf(outpt,"\tSET_STRING_ELT(trann,4,mkChar(\"calc_lhs\"));\n");
  fprintf(outpt,"\tSET_STRING_ELT(tran, 4,mkChar(\"%scalc_lhs\"));\n",model_prefix);

  fprintf(outpt,"\tSET_STRING_ELT(trann,5,mkChar(\"model_vars\"));\n");
  fprintf(outpt,"\tSET_STRING_ELT(tran, 5,mkChar(\"%smodel_vars\"));\n",model_prefix);
  
  fprintf(outpt,"\tSET_STRING_ELT(trann,6,mkChar(\"ode_solver\"));\n");
  fprintf(outpt,"\tSET_STRING_ELT(tran, 6,mkChar(\"%sode_solver\"));\n",model_prefix);
  
  fprintf(outpt,"\tSET_STRING_ELT(trann,7,mkChar(\"ode_solver_sexp\"));\n");
  fprintf(outpt,"\tSET_STRING_ELT(tran, 7,mkChar(\"%sode_solver_sexp\"));\n",model_prefix);

  fprintf(outpt,"\tSET_STRING_ELT(trann,8,mkChar(\"ode_solver_0_6\"));\n");
  fprintf(outpt,"\tSET_STRING_ELT(tran, 8,mkChar(\"%sode_solver_0_6\"));\n",model_prefix);

  fprintf(outpt,"\tSET_STRING_ELT(trann,9,mkChar(\"ode_solver_focei_eta\"));\n");
  fprintf(outpt,"\tSET_STRING_ELT(tran, 9,mkChar(\"%sode_solver_focei_eta\"));\n",model_prefix);
  
  fprintf(outpt,"\tSET_STRING_ELT(trann,10,mkChar(\"ode_solver_ptr\"));\n");
  fprintf(outpt,"\tSET_STRING_ELT(tran, 10,mkChar(\"%sode_solver_ptr\"));\n",model_prefix);

  fprintf(outpt,"\tSET_STRING_ELT(trann,11,mkChar(\"inis\"));\n");
  fprintf(outpt,"\tSET_STRING_ELT(tran, 11,mkChar(\"%sinis\"));\n",model_prefix);
  
  fprintf(outpt,"\tsetAttrib(tran, R_NamesSymbol, trann);\n");
  fprintf(outpt,"\tsetAttrib(mmd5, R_NamesSymbol, mmd5n);\n");
  fprintf(outpt,"\tsetAttrib(model, R_NamesSymbol, modeln);\n");
  fprintf(outpt,"\tsetAttrib(ini, R_NamesSymbol, inin);\n");
  fprintf(outpt,"\tsetAttrib(lst, R_NamesSymbol, names);\n");

  fprintf(outpt,"\tUNPROTECT(16);\n");
  
  fprintf(outpt,"\treturn lst;\n");
  fprintf(outpt,"}\n");
  fprintf(outpt, __HD_SOLVE__);
}

void codegen(FILE *outpt, int show_ode) {
  int i, j, k, print_ode=0, print_vars = 0, print_parm = 0, print_jac=0, o;
  char sLine[MXLEN+1];
  char buf[64];
  char from[512], to[512], df[512], dy[512], state[512];
  char *s2;
  FILE *fpIO;
  char *hdft[]=
    {
      "\n// prj-specific differential eqns\nvoid ",
      "dydt(unsigned int _neq, double t, double *__zzStateVar__, double *__DDtStateVar__)\n{\n",
      "    _dadt_counter_inc();\n}\n\n"
    };
  if (show_ode == 1){
    fprintf(outpt, __HD_ODE__);
    /* if (found_jac == 1){ */
    /*   for (i=0; i<tb.nd; i++) {                   /\* name state vars *\/ */
    /*     retieve_var(tb.di[i], buf); */
    /*     fprintf(outpt, "#define __CMT_NUM_%s__ %d\n", buf, i); */
    /*   } */
    /* } */
    fprintf(outpt,"\n");
    for (i = 0; i < strlen(extra_buf); i++){
      if (extra_buf[i] == '"'){
        fprintf(outpt,"\"");
      } else if (extra_buf[i] == '\n'){
        fprintf(outpt,"\n");
      } else if (extra_buf[i] == '\t'){
        fprintf(outpt,"\t");
      } else if (extra_buf[i] >= 32  && extra_buf[i] <= 126){ // ASCII only
        fprintf(outpt,"%c",extra_buf[i]);
      }
    }
    fprintf(outpt, "%s", hdft[0]);
    fprintf(outpt, "%s", model_prefix);
    fprintf(outpt, "%s", hdft[1]);
  } else if (show_ode == 2){
    fprintf(outpt, "// Jacobian derived vars\nvoid %scalc_jac(unsigned int _neq, double t, double *__zzStateVar__, double *__PDStateVar__, unsigned int __NROWPD__) {\n",model_prefix);
  } else if (show_ode == 3){
    fprintf(outpt, "// Functional based initial conditions.\nvoid %sinis(SEXP _ini_sexp){\n\tdouble *__zzStateVar__ = REAL(_ini_sexp);\n\tdouble t=0;\n",model_prefix);
  } else {
    fprintf(outpt, "// prj-specific derived vars\nvoid %scalc_lhs(double t, double *__zzStateVar__, double *_lhs) {\n",model_prefix);
  }
  if (found_print){
    fprintf(outpt,"\n\tint __print_ode__ = 0, __print_vars__ = 0,__print_parm__ = 0,__print_jac__ = 0;\n");
  }
  if ((show_ode == 2 && found_jac == 1) || show_ode != 2){
    prnt_vars(0, outpt, 0, "double \n\t", "\n",show_ode);     /* declare all used vars */
    if (show_ode == 3){
      fprintf(outpt,"\t_update_par_ptr(0.0);\n");
    } else {
      fprintf(outpt,"\t_update_par_ptr(t);\n");
    }
    prnt_vars(1, outpt, 1, "", "\n",show_ode);                   /* pass system pars */
    for (i=0; i<tb.nd; i++) {                   /* name state vars */
      retieve_var(tb.di[i], buf);
      fprintf(outpt,"\t");
      for (k = 0; k < strlen(buf); k++){
	if (buf[k] == '.'){
	  fprintf(outpt,"_DoT_");
	  if (!rx_syntax_allow_dots){
	    trans_syntax_error_report_fn(NODOT);
	  }
	} else {
	  fprintf(outpt,"%c",buf[k]);
	}
      }
      fprintf(outpt," = __zzStateVar__[%d];\n", i);
    }
    fprintf(outpt,"\n");
    fpIO = fopen(out2, "r");
    err_msg((intptr_t) fpIO, "Error parsing. (Couldn't access out2.txt).\n", -1);
    while(fgets(sLine, MXLEN, fpIO)) {  /* parsed eqns */
      char *s;
      s = strstr(sLine,"(__0__)");
      if (s){
        // See if this is a reclaimed initilization variable.
        for (i=0; i<tb.nv; i++) {
          if (tb.ini[i] == 1 && tb.lh[i] == 1){
            //(__0__)V2=
            retieve_var(i, buf);
            s = strstr(sLine,buf);
            if (s){
              fprintf(outpt,"\t%s\n",sLine + 7);
              continue;
            }
          }
        }
        continue;
      }
      if (show_ode == 3 && strstr(sLine,"full_print;")){
	continue;
      }
      s = strstr(sLine,"(__0f__)");
      if (s){
	if (show_ode == 3){
	  // FIXME
	  for (i = 0; i < tb.nd; i++){
	    retieve_var(tb.di[i], buf);
	    sprintf(to,"(__0f__)%s=",buf);
	    if (strstr(sLine,to)){
	      if (!tb.fdi[i]){
		tb.fdi[i] = 1;
		tb.fdn++;
	      }
	      fprintf(outpt, "\t//if (ISNA(%s)){ // Always apply; Othersiwse, Since this updates vector, it may cause this to only be updated once.\n\t\t%s\t//}\n",buf,sLine+8);
	    }
	  }
	  continue;
	} else {
	  continue;
	}
      }
      s = strstr(sLine,"ode_print;");
      if (show_ode == 1 && !s) s = strstr(sLine,"full_print;");
      if (show_ode != 1 && s) continue;
      else if (s) {
        fprintf(outpt,"\tRprintf(\"================================================================================\\n\");\n");
        fprintf(outpt,"\tRprintf(\"ODE Count: %%d\\tTime (t): %%f\\n\",_dadt_counter_val(),t);\n");
        fprintf(outpt,"\tRprintf(\"================================================================================\\n\");\n");
        fprintf(outpt,"\t__print_ode__ = 1;\n");
        fprintf(outpt,"\t__print_vars__ = 1;\n");
        fprintf(outpt,"\t__print_parm__ = 1;\n");
        print_ode  = 1;
        print_vars = 1;
        print_parm = 1;
        continue;
      }      
      s = strstr(sLine,"ODE_Rprintf");
      if ((show_ode != 1) && s) continue;
      
      s = strstr(sLine,"ODE0_Rprintf");
      if ((show_ode != 1) && s) continue;
      
      s = strstr(sLine, "__DDtStateVar__");
      if (s){
	if (show_ode == 3){
	  continue;
	}
	if (show_ode!= 1){
	  for (i = 0; i < tb.nd; i++){
	    // Replace __DDtStateVar__[#] -> __DDtStateVar_#__
	    sprintf(to,"__DDtStateVar_%d__",i);
	    sprintf(from,"__DDtStateVar__[%d]",i);
	    s2 = repl_str(sLine,from,to);
	    strcpy(sLine, s2);
	    Free(s2);
	    s2=NULL;
	  }
	}
      }
      
      s = strstr(sLine,"JAC_Rprintf");
      if ((show_ode != 2) && s) continue;

      s = strstr(sLine,"jac_print;");
      if (show_ode == 2 && !s) s = strstr(sLine,"full_print;");
      if (show_ode != 2 && s) continue;
      else if (s) {
        fprintf(outpt,"\tRprintf(\"================================================================================\\n\");\n");
        fprintf(outpt,"\tRprintf(\"JAC Count: %%d\\tTime (t): %%f\\n\",_jac_counter_val(),t);\n");
        fprintf(outpt,"\tRprintf(\"================================================================================\\n\");\n");
        fprintf(outpt,"\t__print_ode__ = 1;\n");
        fprintf(outpt,"\t__print_jac__ = 1;\n");
        fprintf(outpt,"\t__print_vars__ = 1;\n");
        fprintf(outpt,"\t__print_parm__ = 1;\n");
        print_ode  = 1;
        print_vars = 1;
        print_parm = 1;
        print_jac = 1;
        continue;
      }

      s = strstr(sLine,"JAC0_Rprintf");
      if ((show_ode != 2) && s) continue;

      s = strstr(sLine,"LHS_Rprintf");
      if ((show_ode != 0) && s) continue;

      s = strstr(sLine,"lhs_print;");
      if (show_ode == 0 && !s) s = strstr(sLine,"full_print;");
      if (show_ode != 0 && s) continue;
      else if (s) {
        fprintf(outpt,"\tRprintf(\"================================================================================\\n\");\n");
        fprintf(outpt,"\tRprintf(\"LHS Time (t): %%f\\n\",t);\n");
        fprintf(outpt,"\tRprintf(\"================================================================================\\n\");\n");
        //fprintf(outpt,"\t__print_ode__ = 1;\n");
        fprintf(outpt,"\t__print_vars__ = 1;\n");
        fprintf(outpt,"\t__print_parm__ = 1;\n");
        //print_ode  = 1;
        print_vars = 1;
        print_parm = 1;
        continue;
      }
      
      s = strstr(sLine,"__PDStateVar__");
      if (s){
	if (show_ode == 3){
	  continue;
	}
	for (i = 0; i < tb.ndfdy; i++){
          retieve_var(tb.df[i], df);
          retieve_var(tb.dy[i], dy);
	  /* for (j = 1; j <= tb.maxtheta;j++){ */
          /*   sprintf(to,"THETA[%d]",j); */
	  /*   sprintf(from,"_THETA_%d_",j); */
	  /*   s2 = repl_str(dy,from,to); */
          /*   strcpy(sLine, s2); */
          /*   Free(s2); */
          /*   if (!strcmp(buf,buf2)){ */
          /*     sprintf(buf,"THETA[%d]",j); */
          /*   } */
          /* } */
          sprintf(from,"__PDStateVar__[[%s,%s]]",df,dy);
	  if (show_ode == 2 && tb.sdfdy[i] == 0){
	    // __PDStateVar__[__CMT_NUM_y__*(__NROWPD__)+__CMT_NUM_dy__]
	    sprintf(to,"__PDStateVar__[");
	    o = strlen(to);
	    for (j=0; j<tb.nd; j++) {                     /* name state vars */
              retieve_var(tb.di[j], state);
	      if (!strcmp(state, df)){
		sprintf(to+o,"%d*(__NROWPD__)+",j);
		o = strlen(to);
		break;
	      }
	    }
	    for (j=0; j<tb.nd; j++){
	      retieve_var(tb.di[j], state);
              if (!strcmp(state, dy)){
                sprintf(to+o,"%d]",j);
                o = strlen(to);
                break;
              }
	    }
	  } else {
	    sprintf(to,"__PDStateVar_%s_SeP_%s__",df,dy);
	  }
	  s2 = repl_str(sLine,from,to);
          strcpy(sLine, s2);
          Free(s2);
          s2=NULL;
        }
        
      }
      fprintf(outpt, "\t%s", sLine);
    }
    fclose(fpIO);
  }
  if (print_ode && show_ode != 0){
    fprintf(outpt,"\tif (__print_ode__ == 1){\n");
    for (i=0; i<tb.nd; i++) {                   /* name state vars */
      retieve_var(tb.di[i], buf);
      fprintf(outpt, "\t\tRprintf(\"d/dt(%s)[%d]:\\t%%f\\t%s:\\t%%f\\n\", __DDtStateVar__[%d],%s);\n", buf, i,buf,i,buf);
    }
    fprintf(outpt,"\t}\n");
  }
  if (print_jac && show_ode == 2){
    fprintf(outpt,"\tif (__print_jac__ == 1){\n");
    fprintf(outpt,"\tRprintf(\"Fixme\\n\");");
    fprintf(outpt,"\t}\n");
  }
  if (print_vars){
    fprintf(outpt,"\tif (__print_vars__ == 1){\n");
    fprintf(outpt,"\t\tRprintf(\".Left Handed Variables:.........................................................\\n\");\n");      
    for (i=0, j=0; i<tb.nv; i++) {
      if (tb.lh[i] != 1) continue;
      retieve_var(i, buf);
      fprintf(outpt, "\t\tRprintf(\"%s = %%f\\n\", %s);\n", buf, buf);
    }
    fprintf(outpt,"\t}\n");
  }
  if (print_parm){
    fprintf(outpt,"\tif (__print_parm__ == 1){\n");
    fprintf(outpt,"\t\tRprintf(\".User Supplied Variables:.......................................................\\n\");\n");
    for (i=0, j=0; i<tb.nv; i++) {
      if (tb.lh[i]>0) continue;
      j++;
      retieve_var(i, buf);
      fprintf(outpt, "\t\tRprintf(\"%s=%%f\\t_par_ptr(%d)=%%f\\n\",%s,_par_ptr(%d));\n", buf, j-1, buf,j-1);
    }
    fprintf(outpt,"\t}\n");
  }
  if (print_jac || print_vars || print_ode || print_parm){
    fprintf(outpt,"\tif (__print_jac__ || __print_vars__ || __print_ode__ || __print_parm__){\n");
    fprintf(outpt,"\t\tRprintf(\"================================================================================\\n\\n\\n\");\n\t}\n");
  }
  if (show_ode == 1){
    fprintf(outpt, "%s", hdft[2]);
  } else if (show_ode == 2){
    //fprintf(outpt,"\tfree(__ld_DDtStateVar__);\n");
    fprintf(outpt, "  _jac_counter_inc();\n");
    fprintf(outpt, "}\n");
  } else if (show_ode == 3){
    for (i = 0; i < tb.nd; i++){
      retieve_var(tb.di[i], buf);
      fprintf(outpt,"  __zzStateVar__[%d]=",i);
      for (k = 0; k < strlen(buf); k++){
        if (buf[k] == '.'){
          fprintf(outpt,"_DoT_");
          if (!rx_syntax_allow_dots){
            trans_syntax_error_report_fn(NODOT);
          }
        } else {
          fprintf(outpt,"%c",buf[k]);
        }
      }
      fprintf(outpt, ";\n");
    }
    fprintf(outpt, "}\n");
  } else {
    fprintf(outpt, "\n");
    for (i=0, j=0; i<tb.nv; i++) {
      if (tb.lh[i] != 1) continue;
      retieve_var(i, buf);
      fprintf(outpt, "\t_lhs[%d]=", j);
      for (k = 0; k < strlen(buf); k++){
        if (buf[k] == '.'){
          fprintf(outpt,"_DoT_");
          if (!rx_syntax_allow_dots){
            trans_syntax_error_report_fn(NODOT);
          }
        } else {
          fprintf(outpt,"%c",buf[k]);
        }
      }
      fprintf(outpt, ";\n");
      j++;
    }
    fprintf(outpt, "}\n");
  }
}
void reset (){
  // Reset Arrays
  memset(tb.ss,		0, 64*MXSYM);
  memset(tb.de,		0, 64*MXSYM);
  memset(tb.deo,	0, MXSYM);
  memset(tb.vo,		0, MXSYM);
  memset(tb.lh,		0, MXSYM);
  memset(tb.ini,	0, MXSYM);
  memset(tb.di,		0, MXDER);
  memset(tb.fdi,        0, MXDER);
  memset(tb.dy,		0, MXSYM);
  memset(tb.sdfdy,	0, MXSYM);
  // Reset integers
  tb.nv		= 0;
  tb.ix		= 0;
  tb.id		= 0;
  tb.fn		= 0;
  tb.nd		= 0;
  tb.pos	= 0;
  tb.pos_de	= 0;
  tb.ini_i	= 0;
  tb.statei	= 0;
  tb.sensi	= 0;
  tb.li		= 0;
  tb.pi		= 0;
  tb.cdf	= 0;
  tb.ndfdy	= 0;
  tb.maxtheta   = 0;
  tb.maxeta     = 0;
  tb.fdn        = 0;
  // reset globals
  found_print = 0;
  found_jac = 0;
  rx_syntax_error = 0;
  rx_suppress_syntax_info=0;
  rx_podo = 0;
  memset(s_aux_info,         0, 64*MXSYM);
  rx_syntax_assign = 0;
  rx_syntax_star_pow = 0;
  rx_syntax_require_semicolon = 0;
  rx_syntax_allow_dots = 0;
  rx_syntax_allow_ini0 = 1;
  rx_syntax_allow_ini = 1;
  memset(sb.s,         0, MXBUF);
  memset(sbt.s,        0, MXBUF);
  sb.o = 0;
  sbt.o = 0;
}

void trans_internal(char *orig_file, char* parse_file, char* c_file){
  char *buf, *infile;
  char buf1[512], buf2[512], bufe[512];
  int i,j,found,islhs;
  D_ParseNode *pn;
  /* any number greater than sizeof(D_ParseNode_User) will do;
     below 1024 is used */
  D_Parser *p = new_D_Parser(&parser_tables_RxODE, 1024);
  p->save_parse_tree = 1;
  buf = r_sbuf_read(parse_file);
  infile = r_sbuf_read(orig_file);

  err_msg((intptr_t) buf, "error: empty buf for FILE_to_parse\n", -2);
  if ((pn=dparse(p, buf, strlen(buf))) && !p->syntax_errors) {
    fpIO = fopen( out2, "w" );
    fpIO2 = fopen( out3, "w" );
    err_msg((intptr_t) fpIO, "error opening out2.txt\n", -2);
    err_msg((intptr_t) fpIO2, "error opening out3.txt\n", -2);
    wprint_parsetree(parser_tables_RxODE, pn, 0, wprint_node, NULL);
    fclose(fpIO);
    fclose(fpIO2);
    // Determine Jacobian vs df/dvar
    for (i=0; i<tb.ndfdy; i++) {                     /* name state vars */
      retieve_var(tb.df[i], buf1);
      found=0;
      for (j=0; j<tb.nd; j++) {                     /* name state vars */
        retieve_var(tb.di[j], buf2);
	if (!strcmp(buf1, buf2)){
	  found=1;
          break;
	}
      }
      if (!found){
	retieve_var(tb.dy[i], buf2);
	sprintf(bufe,NOSTATE,buf1,buf2,buf1);
	trans_syntax_error_report_fn(bufe);
      }
      // Now the dy()
      retieve_var(tb.dy[i], buf1);
      found=0;
      for (j=0; j<tb.nd; j++) {                     /* name state vars */
        retieve_var(tb.di[j], buf2);
        if (!strcmp(buf1, buf2)){
          found=1;
          break;
        }
      }
      if (!found){
	for (j=0; j<tb.nv; j++) {
          islhs = tb.lh[j];
	  retieve_var(j, buf2);
          if (islhs>1) continue; /* is a state var */
          retieve_var(j, buf2);
          if ((islhs != 1 || tb.ini[j] == 1) &&!strcmp(buf1, buf2)){
	    found=1;
	    // This is a df(State)/dy(Parameter)
	    tb.sdfdy[i] = 1;
	    break;
	  }
        }
      }
      if (!found){
        retieve_var(tb.df[i], buf1);
      	retieve_var(tb.dy[i], buf2);
      	sprintf(bufe,NOSTATEVAR,buf1,buf2,buf2);
        trans_syntax_error_report_fn(bufe);
      }
    }
    fpIO = fopen(c_file, "w");
    err_msg((intptr_t) fpIO, "error opening output c file\n", -2);
    codegen(fpIO, 1);
    codegen(fpIO, 2);
    codegen(fpIO, 3);
    codegen(fpIO, 0);
    print_aux_info(fpIO,buf, infile);
    fclose(fpIO);
  } else {
    rx_syntax_error = 1;
  }
  free_D_Parser(p);
}

SEXP trans(SEXP orig_file, SEXP parse_file, SEXP c_file, SEXP extra_c, SEXP prefix, SEXP model_md5,
           SEXP parse_model,SEXP parse_model3){
  char *in, *orig, *out, *file, *pfile;
  char buf[512], buf2[512], df[512], dy[512];
  char snum[512];
  char *s2;
  char sLine[MXLEN+1];
  int i, j, islhs, pi=0, li=0, ini_i = 0,o2=0,k=0, l=0;
  double d;
  reset(); 
  rx_syntax_assign = R_get_option("RxODE.syntax.assign",1);
  rx_syntax_star_pow = R_get_option("RxODE.syntax.star.pow",1);
  rx_syntax_require_semicolon = R_get_option("RxODE.syntax.require.semicolon",0);
  rx_syntax_allow_dots = R_get_option("RxODE.syntax.allow.dots",1);
  rx_suppress_syntax_info = R_get_option("RxODE.suppress.syntax.info",0);
  rx_syntax_allow_ini0 = R_get_option("RxODE.syntax.allow.ini0",1);
  rx_syntax_allow_ini  = R_get_option("RxODE.syntax.allow.ini",1);
  rx_syntax_error = 0;
  set_d_use_r_headers(0);
  set_d_rdebug_grammar_level(0);
  set_d_verbose_level(0);
  rx_podo = 0;
  if (!isString(parse_file) || length(parse_file) != 1){
    error("parse_file is not a single string");
  }
  if (!isString(orig_file) || length(orig_file) != 1){
    error("orig_file is not a single string");
  }
  if (!isString(c_file) || length(c_file) != 1){
    error("c_file is not a single string");
  }
  if (isString(extra_c) && length(extra_c) == 1){
    in = r_dup_str(CHAR(STRING_ELT(extra_c,0)),0);
    extra_buf = r_sbuf_read(in);
    if (!((intptr_t) extra_buf)){ 
      extra_buf = (char *) R_alloc(1,sizeof(char));
      extra_buf[0]='\0';
    }
  } else {
    extra_buf = (char *) R_alloc(1,sizeof(char));
    extra_buf[0] = '\0';
  }

  orig = r_dup_str(CHAR(STRING_ELT(orig_file,0)),0);
  in = r_dup_str(CHAR(STRING_ELT(parse_file,0)),0);
  out = r_dup_str(CHAR(STRING_ELT(c_file,0)),0);
  
  if (isString(prefix) && length(prefix) == 1){
    model_prefix = r_dup_str(CHAR(STRING_ELT(prefix,0)),0);
  } else {
    error("model prefix must be specified");
  }

  if (isString(model_md5) && length(model_md5) == 1){
    md5 = r_dup_str(CHAR(STRING_ELT(model_md5,0)),0);
  } else {
    md5 = R_alloc(1,sizeof(char));
    md5[0] = '\0';
  }
  
  if (isString(parse_model) && length(parse_model) == 1){
    out2 = r_dup_str(CHAR(STRING_ELT(parse_model,0)),0);
  } else {
    error("Parse model must be specified.");
  }


  if (isString(parse_model3) && length(parse_model3) == 1){
    out3 = r_dup_str(CHAR(STRING_ELT(parse_model3,0)),0);
  } else {
    error("Parse model 3 must be specified.");
  }
  trans_internal(orig, in, out);
  SEXP lst   = PROTECT(allocVector(VECSXP, 10));
  SEXP names = PROTECT(allocVector(STRSXP, 10));
  
  SEXP tran  = PROTECT(allocVector(STRSXP, 12));
  SEXP trann = PROTECT(allocVector(STRSXP, 12));
  
  SEXP state  = PROTECT(allocVector(STRSXP,tb.statei));
  SEXP sens   = PROTECT(allocVector(STRSXP,tb.sensi));
  SEXP fn_ini = PROTECT(allocVector(STRSXP, tb.fdn));

  SEXP dfdy = PROTECT(allocVector(STRSXP,tb.ndfdy));
  
  SEXP params = PROTECT(allocVector(STRSXP, tb.pi));
  
  SEXP lhs    = PROTECT(allocVector(STRSXP, tb.li));

  SEXP inin   = PROTECT(allocVector(STRSXP, tb.ini_i));
  SEXP ini    = PROTECT(allocVector(REALSXP, tb.ini_i));

  SEXP model  = PROTECT(allocVector(STRSXP,4));
  SEXP modeln = PROTECT(allocVector(STRSXP,4));

  k=0;j=0;l=0;
  for (i=0; i<tb.nd; i++) {                     /* name state vars */
    retieve_var(tb.di[i], buf);
    if (strstr(buf,"rx__sens_")){
      SET_STRING_ELT(sens,j++,mkChar(buf));
      SET_STRING_ELT(state,k++,mkChar(buf));
    } else {
      SET_STRING_ELT(state,k++,mkChar(buf));
    }
    if (tb.fdi[i]){
      SET_STRING_ELT(state,l++,mkChar(buf));
    }
  }
  for (i=0; i<tb.ndfdy; i++) {                     /* name state vars */
    retieve_var(tb.df[i], df);
    retieve_var(tb.dy[i], dy);
    for (j = 1; j <= tb.maxtheta;j++){
      sprintf(buf,"_THETA_%d_",j);
      if (!strcmp(dy,buf2)){
        sprintf(dy,"THETA[%d]",j);
      }
    }
    for (j = 1; j <= tb.maxeta;j++){
      sprintf(buf,"_ETA_%d_",j);
      if (!strcmp(dy,buf)){
        sprintf(dy,"ETA[%d]",j);
      }
    }
    sprintf(buf,"df(%s)/dy(%s)",df,dy);
    SET_STRING_ELT(dfdy,i,mkChar(buf));
  }
  for (i=0; i<tb.nv; i++) {
    islhs = tb.lh[i];
    if (islhs>1) continue;      /* is a state var */
    retieve_var(i, buf);
    if (islhs == 1){
      SET_STRING_ELT(lhs,li++,mkChar(buf));
    } else if (strcmp(buf,"pi")){
      for (j = 1; j <= tb.maxtheta;j++){
	sprintf(buf2,"_THETA_%d_",j);
	if (!strcmp(buf, buf2)){
	  sprintf(buf,"THETA[%d]",j);
	}
      }
      for (j = 1; j <= tb.maxeta;j++){
        sprintf(buf2,"_ETA_%d_",j);
        if (!strcmp(buf, buf2)){
          sprintf(buf,"ETA[%d]",j);
        }
      }
      SET_STRING_ELT(params,pi++,mkChar(buf));
    }
  }
  SET_STRING_ELT(names,0,mkChar("trans"));
  SET_VECTOR_ELT(lst,  0,tran);
  
  SET_STRING_ELT(names,3,mkChar("params"));
  SET_VECTOR_ELT(lst,  3,params);

  SET_STRING_ELT(names,1,mkChar("lhs"));
  SET_VECTOR_ELT(lst,  1,lhs);

  SET_STRING_ELT(names,2,mkChar("state"));
  SET_VECTOR_ELT(lst,  2,state);
  
  SET_STRING_ELT(names,4,mkChar("ini"));
  SET_VECTOR_ELT(lst,  4,ini);

  SET_STRING_ELT(names,5,mkChar("model"));
  SET_VECTOR_ELT(lst,  5,model);

  SET_STRING_ELT(names,6,mkChar("podo"));
  SET_VECTOR_ELT(lst,  6,ScalarLogical(rx_podo));

  SET_STRING_ELT(names,7,mkChar("dfdy"));
  SET_VECTOR_ELT(lst,  7,dfdy);

  SET_STRING_ELT(names,8,mkChar("sens"));
  SET_VECTOR_ELT(lst,  8,sens);
  
  SET_STRING_ELT(names,8,mkChar("fn.ini"));
  SET_VECTOR_ELT(lst,  8,fn_ini);
  
  SET_STRING_ELT(trann,0,mkChar("jac"));
  if (found_jac == 1){
    SET_STRING_ELT(tran,0,mkChar("fulluser")); // Full User Matrix
  } else {
    SET_STRING_ELT(tran,0,mkChar("fullint")); // Full Internal Matrix
  }
  SET_STRING_ELT(trann,1,mkChar("prefix"));
  SET_STRING_ELT(tran,1,mkChar(buf));

  sprintf(buf,"%sdydt",model_prefix);
  SET_STRING_ELT(trann,2,mkChar("dydt"));
  SET_STRING_ELT(tran,2,mkChar(buf)) ;

  sprintf(buf,"%scalc_jac",model_prefix);
  SET_STRING_ELT(trann,3,mkChar("calc_jac"));
  SET_STRING_ELT(tran, 3,mkChar(buf));

  sprintf(buf,"%scalc_lhs",model_prefix);
  SET_STRING_ELT(trann,4,mkChar("calc_lhs"));
  SET_STRING_ELT(tran, 4,mkChar(buf));

  sprintf(buf,"%smodel_vars",model_prefix);
  SET_STRING_ELT(trann,5,mkChar("model_vars"));
  SET_STRING_ELT(tran, 5,mkChar(buf));

  sprintf(buf,"%sode_solver",model_prefix);
  SET_STRING_ELT(trann,6,mkChar("ode_solver"));
  SET_STRING_ELT(tran, 6,mkChar(buf));

  sprintf(buf,"%sode_solver_sexp",model_prefix);
  SET_STRING_ELT(trann,7,mkChar("ode_solver_sexp"));
  SET_STRING_ELT(tran, 7,mkChar(buf));

  sprintf(buf,"%sode_solver_0_6",model_prefix);
  SET_STRING_ELT(trann,8,mkChar("ode_solver_0_6"));
  SET_STRING_ELT(tran, 8,mkChar(buf));

  sprintf(buf,"%sode_solver_focei_eta",model_prefix);
  SET_STRING_ELT(trann,9,mkChar("ode_solver_focei_eta"));
  SET_STRING_ELT(tran, 9,mkChar(buf));

  sprintf(buf,"%sode_solver_ptr",model_prefix);
  SET_STRING_ELT(trann,10,mkChar("ode_solver_ptr"));
  SET_STRING_ELT(tran, 10,mkChar(buf));

  sprintf(buf,"%sinis",model_prefix);
  SET_STRING_ELT(trann,11,mkChar("inis"));
  SET_STRING_ELT(tran, 11,mkChar(buf));
  
  fpIO2 = fopen(out2, "r");
  err_msg((intptr_t) fpIO2, "Error parsing. (Couldn't access out2.txt).\n", -1);
  while(fgets(sLine, MXLEN, fpIO2)) {
    s2 = strstr(sLine,"(__0__)");
    if (s2){
      // See if this is a reclaimed initilization variable.
      for (i=0; i<tb.nv; i++) {
        if (tb.ini[i] == 1 && tb.lh[i] != 1){
          //(__0__)V2=
          retieve_var(i, buf);
          sprintf(buf2,"(__0__)");
          o2 = 7;
          for (k = 0; k < strlen(buf); k++){
            if (buf[k] == '.'){
              sprintf(buf2+o2,"_DoT_");
              if (!rx_syntax_allow_dots){
                trans_syntax_error_report_fn(NODOT);
              }
              o2+=5;
            } else {
              sprintf(buf2+o2,"%c",buf[k]);
              o2++;
            }
          }
          sprintf(buf2+o2,"=");
          s2 = strstr(sLine,buf2);
          if (s2){
            /* Rprintf("%s[%d]->\n",buf,ini_i++); */
            SET_STRING_ELT(inin,ini_i,mkChar(buf));
            sprintf(snum,"%.*s",(int)(strlen(sLine)-strlen(buf2) - 2), sLine + strlen(buf2));
            sscanf(snum, "%lf", &d);
            REAL(ini)[ini_i++] = d;
            continue;
          }
        }
      }
      continue;
    }
  }
  fclose(fpIO2);
  // putin constants
  for (i=0; i<tb.nv; i++) {
    if (tb.ini[i] == 0 && tb.lh[i] != 1) {
      retieve_var(i, buf);
      // Put in constants
      if  (!strcmp("pi",buf)){
        SET_STRING_ELT(inin,ini_i,mkChar("pi"));
        REAL(ini)[ini_i++] = M_PI;
      }
    }
  }
  file = r_sbuf_read(orig);
  pfile = (char *) R_alloc(strlen(file)+1,sizeof(char));
  j=0;
  for (i = 0; i < strlen(file); i++){
    if (file[i] == '"'  ||
        file[i] == '\n' ||
        file[i] == '\t' ||
        (file[i] >= 32 && file[i] <= 126)){
      sprintf(pfile+(j++),"%c",file[i]);
    }
  }
  SET_STRING_ELT(modeln,0,mkChar("model"));
  SET_STRING_ELT(model,0,mkChar(pfile));
  
  SET_STRING_ELT(modeln,1,mkChar("normModel"));
  file = r_sbuf_read(out3);
  if (file){
    pfile = (char *) R_alloc(strlen(file)+1,sizeof(char));
    j=0;
    for (i = 0; i < strlen(file); i++){
      if (file[i] == '"'  ||
          file[i] == '\n' ||
          file[i] == '\t' ||
          (file[i] >= 33 && file[i] <= 126)){
        sprintf(pfile+(j++),"%c",file[i]);
      }
    }
    SET_STRING_ELT(model,1,mkChar(pfile));
  } else {
    SET_STRING_ELT(model,1,mkChar("Syntax Error"));
  }
  /* printf("parseModel\n"); */
  SET_STRING_ELT(modeln,2,mkChar("parseModel"));
  file = r_sbuf_read(out2);
  if (file){
    pfile = (char *) R_alloc(strlen(file)+1,sizeof(char));
    j=0;
    for (i = 0; i < strlen(file); i++){
      if (file[i] == '"'  ||
          file[i] == '\n' ||
          file[i] == '\t' ||
          (file[i] >= 32 && file[i] <= 126)){
        sprintf(pfile+(j++),"%c",file[i]);
      }
    }
    SET_STRING_ELT(model,2,mkChar(pfile));
  } else {
    SET_STRING_ELT(model,2,mkChar("Syntax Error"));
  }

  file = r_sbuf_read(in);
  pfile = (char *) R_alloc(strlen(file)+1,sizeof(char));
  j=0;
  for (i = 0; i < strlen(file); i++){
    if (file[i] == '"'  ||
        file[i] == '\n' ||
        file[i] == '\t' ||
        (file[i] >= 32 && file[i] <= 126)){
      sprintf(pfile+(j++),"%c",file[i]);
    }
  }
  SET_STRING_ELT(modeln,3,mkChar("expandModel"));
  SET_STRING_ELT(model,3,mkChar(pfile));
  
  setAttrib(ini,   R_NamesSymbol, inin);
  setAttrib(tran,  R_NamesSymbol, trann);
  setAttrib(lst,   R_NamesSymbol, names);
  setAttrib(model, R_NamesSymbol, modeln);
  UNPROTECT(14);
  remove(out3);
  if (rx_syntax_error){
    error("Syntax Errors (see above)");
  }
  return lst;
}

