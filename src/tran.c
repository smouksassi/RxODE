#include <sys/stat.h> 
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>   /* dj: import intptr_t */
#include "gramgram.h"
#include "d.h"
#include "mkdparse.h"
#include "dparse.h"
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

// from mkdparse_tree.h
typedef void (print_node_fn_t)(int depth, char *token_name, char *token_value, void *client_data);

extern int d_use_file_name;
extern char *d_file_name;


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
extern int d_use_r_headers;
extern int d_rdebug_grammar_level;
extern int d_verbose_level;

unsigned int found_jac = 0, found_print = 0;
int rx_syntax_assign = 0, rx_syntax_star_pow = 0,
  rx_syntax_require_semicolon = 0, rx_syntax_allow_dots = 0;

char s_aux_info[64*MXSYM];


typedef struct symtab {
  char *ss;			/* symbol string: all vars*/
  char *de;             /* symbol string: all Des*/
  int deo[MXSYM];        /* offest of des */
  int vo[MXSYM];	/* offset of symbols */
  int lh[MXSYM];	/* lhs symbols? =9 if a state var*/
  int ini[MXSYM];        /* initial variable assignment =2 if there are two assignments */
  int ini0[MXSYM];        /* state initial variable assignment =2 if there are two assignments */
  int di[MXDER];	/* ith of state vars */
  int nv;			/* nbr of symbols */
  int ix;                       /* ith of curr symbol */
  int id;                       /* ith of curr symbol */
  int fn;			/* curr symbol a fn?*/
  int nd;			/* nbr of dydt */
  int pos;
  int pos_de;
  int ini_i; // #ini
  int statei; // # states
  int li; // # lhs
  int pi; // # param
} symtab;
symtab tb;

typedef struct sbuf {
  char s[MXBUF];	/* curr print buffer */
  int o;			/* offset of print buffer */
} sbuf;
sbuf sb;			/* buffer w/ current parsed & translated line */
        			/* to be stored in a temp file */
sbuf sbt; 

char *extra_buf, *model_prefix, *md5, *out2;

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
    if (!strncmp(tb.ss+tb.vo[i], s, max(len, len_s))) {	/* note we need take the max in order not to match a sub-string */
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
  // Took out space; changes parsing of statements like = -1 so that they cannot besc
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
  int nch = d_get_number_of_children(pn), i, k;
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
      sprintf(SBPTR, " pow(");
      sb.o += 5;
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

      
      /* if (!strcmp("decimalint",name)){ */
      /* 	// Make implicit double */
      /* 	sprintf(SBPTR,".0"); */
      /* 	sb.o += 2; */
      /* } */

      tb.fn = (!strcmp("function", name) && i==0) ? 1 : 0;
      D_ParseNode *xpn = d_get_child(pn,i);
      wprint_parsetree(pt, xpn, depth, fn, client_data);
      if (rx_syntax_require_semicolon && !strcmp("end_statement",name) && i == 0){
	if (xpn->start_loc.s ==  xpn->end){
	  trans_syntax_error_report_fn(NEEDSEMI);
	} 
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
	  sprintf(SBPTR,"__PDStateVar__[__CMT_NUM_%s__*(__NROWPD__)+",v);
	  sprintf(SBTPTR,"df(%s)/dy(",v);
	} else {
	  // New statment
	  sb.o = 0;
	  sbt.o = 0;
          sprintf(sb.s,"__PDStateVar__[__CMT_NUM_%s__*(__NROWPD__)+",v);
	  sprintf(sbt.s,"df(%s)/dy(",v);
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
      if (!strcmp("lfactorial_exp",name) && (i == 2 || i == 4 || i == 5)){
	// Take out unneeded expression.
	sb.o--;
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
	sprintf(SBPTR, "__CMT_NUM_%s__]",v);
	sb.o = strlen(sb.s);
	sprintf(SBTPTR, "%s)",v);
	sbt.o = strlen(sbt.s);
	if (strcmp("jac",name) == 0 ||
	    strcmp("dfdy",name) == 0){
	  sprintf(SBPTR ," = ");
	  sb.o += 3;
	  sprintf(SBTPTR ,"=");
          sbt.o += 1;
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
	sprintf(SBPTR, "transit3(t,");
	sb.o += 11;
	sprintf(SBTPTR,"transit(");
	sbt.o += 8;
	rx_podo = 1;
      }
      if (!strcmp("transit3", name) && i == 0){
        sprintf(SBPTR, "transit4(t,");
        sb.o += 11;
        sprintf(SBTPTR,"transit(");
        sbt.o += 8;
        rx_podo = 1;
      }
      if (!strcmp("derivative", name) && i==2) {
        /* sprintf(sb.s, "__DDtStateVar__[%d] = InfusionRate(%d) +", tb.nd, tb.nd); */
        /* sb.o = strlen(sb.s); */
        char *v = (char*)rc_dup_str(xpn->start_loc.s, xpn->end);
	if (new_de(v)){
	  sprintf(sb.s, "__DDtStateVar__[%d] = InfusionRate(%d) + ", tb.nd, tb.nd);
          sb.o = strlen(sb.s);
	  sprintf(sbt.s, "d/dt(%s)=", v);
	  sbt.o = strlen(sbt.s);
	  new_or_ith(v);
	  /* Rprintf("%s; tb.ini = %d; tb.ini0 = %d; tb.lh = %d\n",v,tb.ini[tb.ix],tb.ini0[tb.ix],tb.lh[tb.ix]); */
          if  ((tb.ini[tb.ix] == 1 && tb.ini0[tb.ix] == 0) || (tb.lh[tb.ix] == 1 && tb.ini[tb.ix] == 0)){
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

      if ((!strcmp("assignment", name) || !strcmp("ini", name) || !strcmp("ini0", name)) && i==0) {
        char *v = (char*)rc_dup_str(xpn->start_loc.s, xpn->end);
	if (!strcmp("ini", name) || !strcmp("ini0", name)){
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
	  if (!strcmp("ini",name) & !new_de(v)){
	    sprintf(buf,"Cannot assign state variable %s; For initial condition assigment use '%s(0) ='.\n",v,v);
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
        sbt.o = strlen(v);
        new_or_ith(v);
	if (!strcmp("assignment", name)){
	  tb.lh[tb.ix] = 1;
        } else if (!strcmp("ini", name) || !strcmp("ini0",name)){
	  if (tb.ini[tb.ix] == 0){
	    // If there is only one initialzation call, then assume
	    // this is a parameter with an initial value.
	    tb.ini[tb.ix] = 1;
	    if (!strcmp("ini0",name)){
	      tb.ini0[tb.ix] = 1;
	    }
	  } else {
	    // There is more than one call to this variable, it is a
	    // conditional variabile
            tb.lh[tb.ix] = 1;
	    if (tb.ini0[tb.ix] == 1){
	      sprintf(buf,"Cannot have conditional initial conditions for %s",v);
	      trans_syntax_error_report_fn(buf);
            }
          }
	}
        Free(v);
      }
    }

    if (!strcmp("assignment", name) || !strcmp("ini", name) || !strcmp("derivative", name) || !strcmp("jac",name) || !strcmp("dfdy",name) ||
	!strcmp("ini0",name)){
      fprintf(fpIO, "%s;\n", sb.s);
      fprintf(fpIO2, "%s;\n", sbt.s);
    }

    if (!rx_syntax_assign && (!strcmp("assignment", name) || !strcmp("ini", name) || !strcmp("ini0",name))){
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
void prnt_vars(int scenario, FILE *outpt, int lhs, const char *pre_str, const char *post_str) {
  int i, j, k;
  char buf[64];

  fprintf(outpt, "%s", pre_str);  /* dj: avoid security vulnerability */
  for (i=0, j=0; i<tb.nv; i++) {
    if (lhs && tb.lh[i]>0) continue;
    retieve_var(i, buf);
    switch(scenario) {
    case 0:
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
      fprintf(outpt, " = par_ptr(%d);\n", j++);
      break;
    default: break;
    }
  }
  fprintf(outpt, "%s", post_str);  /* dj: security calls for const format */
}

void print_aux_info(FILE *outpt, char *model){
  int i, k, islhs,pi = 0,li = 0, o=0, o2=0, statei = 0, ini_i = 0;
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
      sprintf(s_aux_info+o, "\tSET_STRING_ELT(params,%d,mkChar(\"%s\"));\n", pi++, buf);
    }
    o = strlen(s_aux_info);
  }
  for (i=0; i<tb.nd; i++) {                     /* name state vars */
    retieve_var(tb.di[i], buf);
    sprintf(s_aux_info+o, "\tSET_STRING_ELT(state,%d,mkChar(\"%s\"));\n", statei++, buf);
    o = strlen(s_aux_info);
  }
  fprintf(outpt,"extern SEXP %smodel_vars(){\n",model_prefix);
  fprintf(outpt,"\tSEXP lst    = PROTECT(allocVector(VECSXP, 8));\n");
  fprintf(outpt,"\tSEXP names  = PROTECT(allocVector(STRSXP, 8));\n");
  fprintf(outpt,"\tSEXP params = PROTECT(allocVector(STRSXP, %d));\n",pi);
  fprintf(outpt,"\tSEXP lhs    = PROTECT(allocVector(STRSXP, %d));\n",li);
  fprintf(outpt,"\tSEXP state  = PROTECT(allocVector(STRSXP, %d));\n",statei);
  fprintf(outpt,"\tSEXP tran   = PROTECT(allocVector(STRSXP, 7));\n");
  fprintf(outpt,"\tSEXP trann  = PROTECT(allocVector(STRSXP, 7));\n");
  fprintf(outpt,"\tSEXP mmd5   = PROTECT(allocVector(STRSXP, 2));\n");
  fprintf(outpt,"\tSEXP mmd5n  = PROTECT(allocVector(STRSXP, 2));\n");
  fprintf(outpt,"\tSEXP model  = PROTECT(allocVector(STRSXP, 3));\n");
  fprintf(outpt,"\tSEXP modeln = PROTECT(allocVector(STRSXP, 3));\n");
  fprintf(outpt,"%s",s_aux_info);
  // Save for outputting in trans
  tb.pi = pi;
  tb.li = li;
  tb.statei = statei;
  fprintf(outpt,"\tSET_STRING_ELT(modeln,0,mkChar(\"model\"));\n");
  fprintf(outpt,"\tSET_STRING_ELT(model,0,mkChar(\"");
  for (i = 0; i < strlen(model); i++){
    if (model[i] == '"'){
      fprintf(outpt,"\\\"");
    } else if (model[i] == '\n'){
      fprintf(outpt,"\\n");
    } else if (model[i] == '\t'){
      fprintf(outpt,"\\t");
    } else if (model[i] >= 32  && model[i] <= 126){ // ASCII only
      fprintf(outpt,"%c",model[i]);
    }
  }
  fprintf(outpt,"\"));\n");
  fprintf(outpt,"\tSET_STRING_ELT(modeln,1,mkChar(\"normModel\"));\n");
  fpIO2 = fopen("out3.txt", "r");
  fprintf(outpt,"\tSET_STRING_ELT(model,1,mkChar(\"");
  err_msg((intptr_t) fpIO2, "Coudln't access out3.txt.\n", -1);
  while(fgets(sLine, MXLEN, fpIO2)) {  /* Prefered RxODE -- for igraph */
    for (i = 0; i < strlen(sLine); i++){
      if (sLine[i] == '"'){
        fprintf(outpt,"\\\"");
      } else if (sLine[i] == '\n'){
        fprintf(outpt,"\\n");
      } else if (sLine[i] == '\t'){
        fprintf(outpt,"\\t");
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
  err_msg((intptr_t) fpIO2, "Coudln't access out2.txt.\n", -1);
  while(fgets(sLine, MXLEN, fpIO2)) {  /* Prefered RxODE -- for igraph */
    for (i = 0; i < strlen(sLine); i++){
      if (sLine[i] == '"'){
        fprintf(outpt,"\\\"");
      } else if (sLine[i] == '\n'){
        fprintf(outpt,"\\n");
      } else if (sLine[i] == '\t'){
        fprintf(outpt,"\\t");
      } else if (sLine[i] >= 32  && sLine[i] <= 126){ // ASCII only
        fprintf(outpt,"%c",sLine[i]);
      }
    }
  }
  fclose(fpIO2);
  fprintf(outpt,"\"));\n");
  fpIO2 = fopen(out2, "r");
  s_aux_info[0] = '\0';
  o    = 0;
  err_msg((intptr_t) fpIO2, "Coudln't access out2.txt.\n", -1);
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
  
  fprintf(outpt,"\tsetAttrib(tran, R_NamesSymbol, trann);\n");
  fprintf(outpt,"\tsetAttrib(mmd5, R_NamesSymbol, mmd5n);\n");
  fprintf(outpt,"\tsetAttrib(model, R_NamesSymbol, modeln);\n");
  fprintf(outpt,"\tsetAttrib(ini, R_NamesSymbol, inin);\n");
  fprintf(outpt,"\tsetAttrib(lst, R_NamesSymbol, names);\n");

  fprintf(outpt,"\tUNPROTECT(13);\n");
  
  fprintf(outpt,"\treturn lst;\n");
  fprintf(outpt,"}\n");
  fprintf(outpt,"extern SEXP __ODE_SOLVER__ (// Parameters\n");
  fprintf(outpt," SEXP sexp_theta,\n");
  fprintf(outpt," SEXP sexp_inits,\n");
  fprintf(outpt," SEXP sexp_lhs,\n");
  fprintf(outpt," // Events\n");
  fprintf(outpt," SEXP sexp_time,\n");
  fprintf(outpt," SEXP sexp_evid,\n");
  fprintf(outpt," SEXP sexp_dose,\n");
  fprintf(outpt," // Covariates\n");
  fprintf(outpt," SEXP sexp_pcov,\n");
  fprintf(outpt," SEXP sexp_cov,\n");
  fprintf(outpt," SEXP sexp_locf,\n");
  fprintf(outpt," // Solver Options\n");
  fprintf(outpt," SEXP sexp_atol,\n");
  fprintf(outpt," SEXP sexp_rtol,\n");
  fprintf(outpt," SEXP sexp_hmin,\n");
  fprintf(outpt," SEXP sexp_hmax,\n");
  fprintf(outpt," SEXP sexp_h0,\n");
  fprintf(outpt," SEXP sexp_mxordn,\n");
  fprintf(outpt," SEXP sexp_mxords,\n");
  fprintf(outpt," SEXP sexp_mx,\n");
  fprintf(outpt," SEXP sexp_stiff,\n");
  fprintf(outpt," SEXP sexp_transit_abs,\n");
  fprintf(outpt," // Object Creation\n");
  fprintf(outpt," SEXP sexp_object,\n");
  fprintf(outpt," SEXP sexp_extra_args){\n");
  fprintf(outpt," typedef SEXP (*RxODE_ode_solver) (SEXP sexp_theta, SEXP sexp_inits, SEXP sexp_lhs, SEXP sexp_time, SEXP sexp_evid, SEXP sexp_dose, SEXP sexp_pcov, SEXP sexp_cov, SEXP sexp_locf, SEXP sexp_atol, SEXP sexp_rtol, SEXP sexp_hmin, SEXP sexp_hmax, SEXP sexp_h0, SEXP sexp_mxordn, SEXP sexp_mxords, SEXP sexp_mx, SEXP sexp_stiff, SEXP sexp_transit_abs, SEXP sexp_object, SEXP sexp_extra_args, void (*fun_dydt)(unsigned int, double, double *, double *), void (*fun_calc_lhs)(double, double *, double *), void (*fun_calc_jac)(unsigned int, double, double *, double *, unsigned int), int fun_jt, int fun_mf, int fun_debug);\n");
  fprintf(outpt,"RxODE_ode_solver ode_solver=(RxODE_ode_solver)R_GetCCallable(\"RxODE\",\"RxODE_ode_solver\");\n");
  fprintf(outpt,"ode_solver(sexp_theta,sexp_inits,sexp_lhs,sexp_time,sexp_evid,sexp_dose,sexp_pcov,sexp_cov,sexp_locf,sexp_atol,sexp_rtol,sexp_hmin,sexp_hmax,sexp_h0,sexp_mxordn,sexp_mxords,sexp_mx,sexp_stiff,sexp_transit_abs,sexp_object,sexp_extra_args, __DYDT__ , __CALC_LHS__ , __CALC_JAC__, __JT__ , __MF__,\n#ifdef __DEBUG__\n1\n#else\n0\n#endif\n);");
  fprintf(outpt,"}\n");
  //fprintf(outpt,"SEXP __PARSED_MD5__()\n{\n\treturn %smodel_vars();\n}\n",model_prefix);
}

void codegen(FILE *outpt, int show_ode) {
  int i, j, k, print_ode=0, print_vars = 0, print_parm = 0, print_jac=0;
  char sLine[MXLEN+1];
  char buf[64];
  FILE *fpIO;

  char *hdft[]=
    {
      "#include <Rmath.h>\n#ifdef __STANDALONE__\n#define Rprintf printf\n#define JAC_Rprintf printf\n#define JAC0_Rprintf if (jac_counter_val() == 0) printf\n#define ODE_Rprintf printf\n#define ODE0_Rprintf if (dadt_counter_val() == 0) printf\n#define LHS_Rprintf printf\n#define R_alloc calloc\n#else\n#include <R.h>\n#include <Rinternals.h>\n#include <Rmath.h>\n#include <R_ext/Rdynload.h>\n#define JAC_Rprintf Rprintf\n#define JAC0_Rprintf if (jac_counter_val() == 0) Rprintf\n#define ODE_Rprintf Rprintf\n#define ODE0_Rprintf if (dadt_counter_val() == 0) Rprintf\n#define LHS_Rprintf Rprintf\n#endif\n#define max(a,b) (((a)>(b))?(a):(b))\n#define min(a,b) (((a)<(b))?(a):(b))\ntypedef void (*RxODE_update_par_ptr)(double t);\ntypedef double (*RxODE_transit3)(double t, double n, double mtt);\ntypedef double (*RxODE_fn) (double x);\ntypedef double (*RxODE_transit4)(double t, double n, double mtt, double bio);\ntypedef double (*RxODE_vec) (int val);\ntypedef long (*RxODE_cnt) ();\ntypedef void (*RxODE_inc) ();\ntypedef double (*RxODE_val) ();\n RxODE_vec par_ptr, InfusionRate;\n RxODE_update_par_ptr update_par_ptr;\n RxODE_cnt dadt_counter_val, jac_counter_val;\n RxODE_inc dadt_counter_inc, jac_counter_inc;\n RxODE_val podo, tlast;\nRxODE_transit4 transit4;\nRxODE_transit3 transit3;\nRxODE_fn factorial;\nvoid __R_INIT__ (DllInfo *info){\n InfusionRate   = (RxODE_vec) R_GetCCallable(\"RxODE\",\"RxODE_InfusionRate\");\n update_par_ptr = (RxODE_update_par_ptr) R_GetCCallable(\"RxODE\",\"RxODE_update_par_ptr\");\n par_ptr = (RxODE_vec) R_GetCCallable(\"RxODE\",\"RxODE_par_ptr\");\n dadt_counter_val = (RxODE_cnt) R_GetCCallable(\"RxODE\",\"RxODE_dadt_counter_val\");\n jac_counter_val  = (RxODE_cnt) R_GetCCallable(\"RxODE\",\"RxODE_jac_counter_val\");\n dadt_counter_inc = (RxODE_inc) R_GetCCallable(\"RxODE\",\"RxODE_dadt_counter_inc\");\n jac_counter_inc  = (RxODE_inc) R_GetCCallable(\"RxODE\",\"RxODE_jac_counter_inc\");\n podo  = (RxODE_val) R_GetCCallable(\"RxODE\",\"RxODE_podo\");\n tlast = (RxODE_val) R_GetCCallable(\"RxODE\",\"RxODE_tlast\");\ntransit3 = (RxODE_transit3) R_GetCCallable(\"RxODE\",\"RxODE_transit3\");\ntransit4 = (RxODE_transit4) R_GetCCallable(\"RxODE\",\"RxODE_transit4\");\nfactorial=(RxODE_fn) R_GetCCallable(\"RxODE\",\"RxODE_factorial\");}\n",
      "\n// prj-specific differential eqns\nvoid ",
      "dydt(unsigned int neq, double t, double *__zzStateVar__, double *__DDtStateVar__)\n{\n",
      "    dadt_counter_inc();\n}\n\n"
    };
  if (show_ode == 1){
    fprintf(outpt, "%s", hdft[0]);
    if (found_jac == 1){
      for (i=0; i<tb.nd; i++) {                   /* name state vars */
        retieve_var(tb.di[i], buf);
        fprintf(outpt, "#define __CMT_NUM_%s__ %d\n", buf, i);
      }
    }
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
    fprintf(outpt, "%s", hdft[1]);
    fprintf(outpt, "%s", model_prefix);
    fprintf(outpt, "%s", hdft[2]);
  } else if (show_ode == 2){
    fprintf(outpt, "// Jacobian derived vars\nvoid %scalc_jac(unsigned int neq, double t, double *__zzStateVar__, double *__PDStateVar__, unsigned int __NROWPD__) {\n\tdouble __DDtStateVar__[%d];\n",model_prefix,tb.nd+1);
  } else {
    fprintf(outpt, "// prj-specific derived vars\nvoid %scalc_lhs(double t, double *__zzStateVar__, double *lhs) {",model_prefix);
  }
  if (found_print){
    fprintf(outpt,"\n\tint __print_ode__ = 0, __print_vars__ = 0,__print_parm__ = 0,__print_jac__ = 0;\n");
  }
  if ((show_ode == 2 && found_jac == 1) || show_ode != 2){
    prnt_vars(0, outpt, 0, "double \n\t", "\n");     /* declare all used vars */
    fprintf(outpt,"\tupdate_par_ptr(t);\n");
    prnt_vars(1, outpt, 1, "", "\n");                   /* pass system pars */
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
    err_msg((intptr_t) fpIO, "Coudln't access out2.txt.\n", -1);
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
      s = strstr(sLine,"ode_print;");
      if (show_ode == 1 && !s) s = strstr(sLine,"full_print;");
      if (show_ode != 1 && s) continue;
      else if (s) {
	fprintf(outpt,"\tRprintf(\"================================================================================\\n\");\n");
	fprintf(outpt,"\tRprintf(\"ODE Count: %%d\\tTime (t): %%f\\n\",dadt_counter_val(),t);\n");
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
      if ((show_ode == 0) && s) continue;
      
      s = strstr(sLine,"JAC_Rprintf");
      if ((show_ode != 2) && s) continue;

      s = strstr(sLine,"jac_print;");
      if (show_ode == 2 && !s) s = strstr(sLine,"full_print;");
      if (show_ode != 2 && s) continue;
      else if (s) {
        fprintf(outpt,"\tRprintf(\"================================================================================\\n\");\n");
        fprintf(outpt,"\tRprintf(\"JAC Count: %%d\\tTime (t): %%f\\n\",jac_counter,t);\n");
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
      if ((show_ode != 2) && s) continue;
      
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
      fprintf(outpt, "\t\tRprintf(\"%s=%%f\\tpar_ptr(%d)=%%f\\n\",%s,par_ptr(%d));\n", buf, j-1, buf,j-1);
    }
    fprintf(outpt,"\t}\n");
  }
  if (print_jac || print_vars || print_ode || print_parm){
    fprintf(outpt,"\tif (__print_jac__ || __print_vars__ || __print_ode__ || __print_parm__){\n");
    fprintf(outpt,"\t\tRprintf(\"================================================================================\\n\\n\\n\");\n\t}\n");
  }
  if (show_ode == 1){
    fprintf(outpt, "%s", hdft[3]);
  } else if (show_ode == 2){
    //fprintf(outpt,"\tfree(__ld_DDtStateVar__);\n");
    fprintf(outpt, "  jac_counter_inc();\n");
    fprintf(outpt, "}\n");
  } else {
    fprintf(outpt, "\n");
    for (i=0, j=0; i<tb.nv; i++) {
      if (tb.lh[i] != 1) continue;
      retieve_var(i, buf);
      fprintf(outpt, "\tlhs[%d]=", j);
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
  tb.ss = (char*)R_alloc(64*MXSYM,sizeof(char));
  tb.de = (char*)R_alloc(64*MXSYM,sizeof(char));
  tb.vo[0]=0;
  tb.deo[0]=0;
  memset(tb.lh,  0, MXSYM);
  memset(tb.ini, 0, MXSYM);
  memset(tb.ini0, 0, MXSYM);
  tb.nv=0;
  tb.nd=0;
  tb.fn=0;
  tb.ix=0;
  tb.id=0;
  tb.pos =0;
  tb.pos_de = 0;
  tb.ini_i = 0;
  found_print = 0;
  found_jac = 0;
}

void trans_internal(char* parse_file, char* c_file){
  char *buf;
  D_ParseNode *pn;
  /* any number greater than sizeof(D_ParseNode_User) will do;
     below 1024 is used */
  D_Parser *p = new_D_Parser(&parser_tables_RxODE, 1024);
  p->save_parse_tree = 1;
  buf = r_sbuf_read(parse_file);
  err_msg((intptr_t) buf, "error: empty buf for FILE_to_parse\n", -2);
  if ((pn=dparse(p, buf, strlen(buf))) && !p->syntax_errors) {
    reset();
    fpIO = fopen( out2, "w" );
    fpIO2 = fopen( "out3.txt", "w" );
    err_msg((intptr_t) fpIO, "error opening out2.txt\n", -2);
    err_msg((intptr_t) fpIO2, "error opening out3.txt\n", -2);
    wprint_parsetree(parser_tables_RxODE, pn, 0, wprint_node, NULL);
    fclose(fpIO);
    fclose(fpIO2);
    fpIO = fopen(c_file, "w");
    err_msg((intptr_t) fpIO, "error opening output c file\n", -2);
    codegen(fpIO, 1);
    codegen(fpIO, 2);
    codegen(fpIO, 0);
    print_aux_info(fpIO,buf);
    fclose(fpIO);
  } else {
    rx_syntax_error = 1;
  }
  free_D_Parser(p);
}

SEXP trans(SEXP parse_file, SEXP c_file, SEXP extra_c, SEXP prefix, SEXP model_md5,
	   SEXP parse_model){
  char *in, *out, *file, *pfile;
  char buf[512], buf2[512];
  char snum[512];
  char *s2;
  char sLine[MXLEN+1];
  int i, j, islhs, pi=0, li=0, ini_i = 0,o2=0,k=0;
  double d;
  rx_syntax_assign = R_get_option("RxODE.syntax.assign",1);
  rx_syntax_star_pow = R_get_option("RxODE.syntax.star.pow",1);
  rx_syntax_require_semicolon = R_get_option("RxODE.syntax.require.semicolon",0);
  rx_syntax_allow_dots = R_get_option("RxODE.syntax.allow.dots",1);
  rx_suppress_syntax_info = R_get_option("RxODE.suppress.syntax.info",0);
  rx_syntax_error = 0;
  d_use_r_headers = 0;
  d_rdebug_grammar_level = 0;
  d_verbose_level = 0;
  rx_podo = 0;
  if (!isString(parse_file) || length(parse_file) != 1){
    error("parse_file is not a single string");
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
    out2 = (char *) R_alloc(9,sizeof(char)); 
    sprintf(out2,"out2.txt"); 
  }
  trans_internal(in, out);
  SEXP lst   = PROTECT(allocVector(VECSXP, 7));
  SEXP names = PROTECT(allocVector(STRSXP, 7));
  
  SEXP tran  = PROTECT(allocVector(STRSXP, 7));
  SEXP trann = PROTECT(allocVector(STRSXP, 7));
  
  SEXP state = PROTECT(allocVector(STRSXP,tb.nd));
  
  SEXP params = PROTECT(allocVector(STRSXP, tb.pi));
  
  SEXP lhs    = PROTECT(allocVector(STRSXP, tb.li));

  SEXP inin   = PROTECT(allocVector(STRSXP, tb.ini_i));
  SEXP ini    = PROTECT(allocVector(REALSXP, tb.ini_i));

  SEXP model  = PROTECT(allocVector(STRSXP,3));
  SEXP modeln = PROTECT(allocVector(STRSXP,3));

  for (i=0; i<tb.nd; i++) {                     /* name state vars */
    retieve_var(tb.di[i], buf);
    SET_STRING_ELT(state,i,mkChar(buf));
  }
  for (i=0; i<tb.nv; i++) {
    islhs = tb.lh[i];
    if (islhs>1) continue;      /* is a state var */
    retieve_var(i, buf);
    if (islhs == 1){
      SET_STRING_ELT(lhs,li++,mkChar(buf));
    } else if (strcmp(buf,"pi")){
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
  
  fpIO2 = fopen(out2, "r");
  err_msg((intptr_t) fpIO2, "Coudln't access out2.txt.\n", -1);
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
  SET_STRING_ELT(modeln,0,mkChar("model"));
  SET_STRING_ELT(model,0,mkChar(pfile));
  
  SET_STRING_ELT(modeln,1,mkChar("normModel"));
  file = r_sbuf_read("out3.txt");
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
  
  setAttrib(ini,   R_NamesSymbol, inin);
  setAttrib(tran,  R_NamesSymbol, trann);
  setAttrib(lst,   R_NamesSymbol, names);
  setAttrib(model, R_NamesSymbol, modeln);
  UNPROTECT(11);
  remove("out3.txt");
  reset();
  if (rx_syntax_error){
    error("Syntax Errors (see above)");
  }
  return lst;
}

extern D_ParserTables parser_tables_dparser_gram;

static int scanner_block_size;

SEXP cDparser(SEXP fileName,
	      SEXP sexp_output_file,
	      SEXP set_op_priority_from_rule ,
	      SEXP right_recursive_BNF ,
	      SEXP states_for_whitespace ,
	      SEXP states_for_all_nterms ,
	      SEXP tokenizer ,
	      SEXP longest_match ,
	      SEXP sexp_grammar_ident ,
	      SEXP scanner_blocks ,
	      SEXP write_line_directives ,
	      SEXP rdebug,
	      SEXP verbose,
	      SEXP sexp_write_extension,
	      SEXP write_header,
	      SEXP token_type,
	      SEXP use_r_header){
  char *grammar_pathname, *grammar_ident, *write_extension, *output_file;
  Grammar *g;
  d_rdebug_grammar_level = INTEGER(rdebug)[0];
  d_verbose_level        = INTEGER(verbose)[0];
  grammar_pathname = r_dup_str(CHAR(STRING_ELT(fileName,0)),0);
  grammar_ident    = r_dup_str(CHAR(STRING_ELT(sexp_grammar_ident,0)),0);
  write_extension  = r_dup_str(CHAR(STRING_ELT(sexp_write_extension,0)),0);
  output_file      = r_dup_str(CHAR(STRING_ELT(sexp_output_file,0)),0);
  g = new_D_Grammar(grammar_pathname);
  /* grammar construction options */
  g->set_op_priority_from_rule = INTEGER(set_op_priority_from_rule)[0];
  g->right_recursive_BNF = INTEGER(right_recursive_BNF)[0];
  g->states_for_whitespace = INTEGER(states_for_whitespace)[0];
  g->states_for_all_nterms = INTEGER(states_for_all_nterms)[0];
  g->tokenizer = INTEGER(tokenizer)[0];
  g->longest_match = INTEGER(longest_match)[0];
  /* grammar writing options */
  strcpy(g->grammar_ident, grammar_ident);
  g->scanner_blocks = INTEGER(scanner_blocks)[0];
  g->scanner_block_size = scanner_block_size;
  g->write_line_directives = INTEGER(write_line_directives)[0];
  g->write_header = INTEGER(write_header)[0];
  g->token_type = INTEGER(token_type)[0];
  strcpy(g->write_extension, write_extension);
  g->write_pathname = output_file;

  d_use_r_headers = INTEGER(use_r_header)[0];
  /* don't print anything to stdout, when the grammar is printed there */
  if (d_rdebug_grammar_level > 0)
    d_verbose_level = 0;

  mkdparse(g, grammar_pathname);

  if (d_rdebug_grammar_level == 0) {
    if (write_c_tables(g) < 0)
      d_fail("unable to write C tables '%s'", grammar_pathname);
  } else
    print_rdebug_grammar(g, grammar_pathname);

  free_D_Grammar(g);
  g = 0;
  d_use_r_headers = 0;
  d_rdebug_grammar_level = 0;
  d_verbose_level = 0;
  return R_NilValue;
}
