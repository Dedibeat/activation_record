%{
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "util.h"
#include "symbol.h"
#include "absyn.h"
#include "y.tab.h"
#include "errormsg.h"

int charPos = 1;
int bracketCount = 0;
int yywrap(void)
{
    charPos = 1;
    return 1;
}

void adjust(void)
{
    EM_tokPos = charPos;
    charPos += yyleng;
}
%}
%x COMM
%%
<INITIAL>[ \t\r]+    { adjust(); }
<INITIAL>\n          { adjust(); EM_newline(); }

<INITIAL>","         { adjust(); return COMMA; }
<INITIAL>":"         { adjust(); return COLON; }
<INITIAL>";"         { adjust(); return SEMICOLON; }
<INITIAL>"("         { adjust(); return LPAREN; }
<INITIAL>")"         { adjust(); return RPAREN; }
<INITIAL>"["         { adjust(); return LBRACK; }
<INITIAL>"]"         { adjust(); return RBRACK; }
<INITIAL>"{"         { adjust(); return LBRACE; }
<INITIAL>"}"         { adjust(); return RBRACE; }
<INITIAL>"."         { adjust(); return DOT; }

<INITIAL>":="        { adjust(); return ASSIGN; }
<INITIAL>"<>"        { adjust(); return NEQ; }
<INITIAL>"<="        { adjust(); return LE; }
<INITIAL>">="        { adjust(); return GE; }

<INITIAL>"+"         { adjust(); return PLUS; }
<INITIAL>"-"         { adjust(); return MINUS; }
<INITIAL>"*"         { adjust(); return TIMES; }
<INITIAL>"/"         { adjust(); return DIVIDE; }

<INITIAL>"="         { adjust(); return EQ; }
<INITIAL>"<"         { adjust(); return LT; }
<INITIAL>">"         { adjust(); return GT; }

<INITIAL>"&"         { adjust(); return AND; }
<INITIAL>"|"         { adjust(); return OR; }

<INITIAL>array       { adjust(); return ARRAY; }
<INITIAL>if          { adjust(); return IF; }
<INITIAL>then        { adjust(); return THEN; }
<INITIAL>else        { adjust(); return ELSE; }
<INITIAL>while       { adjust(); return WHILE; }
<INITIAL>for         { adjust(); return FOR; }
<INITIAL>to          { adjust(); return TO; }
<INITIAL>do          { adjust(); return DO; }
<INITIAL>let         { adjust(); return LET; }
<INITIAL>in          { adjust(); return IN; }
<INITIAL>end         { adjust(); return END; }
<INITIAL>of          { adjust(); return OF; }
<INITIAL>break       { adjust(); return BREAK; }
<INITIAL>nil         { adjust(); return NIL; }
<INITIAL>function    { adjust(); return FUNCTION; }
<INITIAL>var         { adjust(); return VAR; }
<INITIAL>type        { adjust(); return TYPE; }

<INITIAL>"*/" {
    adjust();
    EM_error(EM_tokPos,"closing unopened comment");
}

<INITIAL>"/*" { 
    adjust(); 
    BEGIN(COMM); 
    bracketCount++;
}

<INITIAL>\"([^"\\\n]|\\.)*\n {
    adjust();
    EM_error(EM_tokPos,"unterminated string");
}

<INITIAL>\"([^"\\\n]|\\.)*\" {
    adjust();
    int len = yyleng - 2;
    char *s = (char*)checked_malloc(len + 1);
    int j = 0;
    for (int i = 1; i < yyleng - 1; i++) {
        if (yytext[i] == '\\') {
            i++; 
            switch (yytext[i]) {
                case 'n':  s[j++] = '\n'; break;
                case 't':  s[j++] = '\t'; break;
                case 'r':  s[j++] = '\r'; break;
                case '\\': s[j++] = '\\'; break;
                case '"':  s[j++] = '"'; break;
                default:
                    if (isdigit((unsigned char)yytext[i])
                        && isdigit((unsigned char)yytext[i + 1])
                        && isdigit((unsigned char)yytext[i + 2])) {
                        char digits[4] = {yytext[i], yytext[i + 1], yytext[i + 2], '\0'};
                        int value = atoi(digits);
                        if (value > 255) {
                            EM_error(EM_tokPos,"invalid escape sequence");
                        } else {
                            s[j++] = (char)value;
                        }
                        i += 2;
                    } else {
                        EM_error(EM_tokPos,"invalid escape sequence");
                        s[j++] = yytext[i];
                    }
                    break;
            }
        } else {
            s[j++] = yytext[i];
        }
    }
    s[j] = '\0';
    yylval.sval = String(s);
    return STRING;
}

<INITIAL>[0-9]+ {
    adjust();
    yylval.ival = strtol(yytext, NULL, 10);
    return INT;
}

<INITIAL>[a-zA-Z][a-zA-Z0-9_]* {
    adjust();
    yylval.sval = String(yytext);
    return ID;
}

<INITIAL>. {
    adjust();
    EM_error(EM_tokPos,"illegal token");
}

<COMM>"*/" {
    adjust(); 
    bracketCount--;
    if(bracketCount < 0) 
    {
        EM_error(EM_tokPos,"closing unopened comment bracker");
    }
    if(bracketCount == 0) 
    {
        BEGIN(INITIAL); 
    }
}
<COMM>"/*" { 
    adjust(); 
    BEGIN(COMM); 
    bracketCount++;
}
<COMM>\n {
    adjust();
    EM_newline();
}
<COMM>. {   
    adjust(); 
}

<COMM><<EOF>> {
    EM_error(EM_tokPos, "unclosed comment");
    return 0;
}

<<EOF>> {
    return 0;
}
