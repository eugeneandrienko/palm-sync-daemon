%option noyywrap
%option 8bit stack yylineno

%{
#include "parser.h"
%}

U            [\x80-\xbf]
U2           [\xc2-\xdf]
U3           [\xe0-\xef]
U4           [\xf0-\xf4]
UTF8SYMBOL   [[:alnum:][:punct:]]|{U2}{U}|{U3}{U}{U}|{U4}{U}{U}{U}
TODO_KEYWORD (TODO|VERIFIED|DONE|CANCELLED)" "
PRIORITY     \[\#(A|B|C)\]" "
TAG          :[[:alnum:]]+:
DATETIME     <[-+: [:alnum:]]+>

%%
^\#\+.+\n { /* Skip OrgMode directives */ }

^\*" " {
    return T_HEADLINE_STAR;
}

{TODO_KEYWORD} {
    yylval.keyword = strdup(yytext);
    yylval.keyword[yyleng - 1] = '\0';
    return T_TODO_KEYWORD;
}

{PRIORITY} {
    yylval.priority = yytext[2];
    return T_PRIORITY;
}

{TAG} {
    yylval.tag = strdup(yytext + 1);
    yylval.tag[yyleng - 2] = '\0';
    return T_TAG;
}

(SCHEDULED|DEADLINE):" " {
    return T_SCHEDULED;
}

{DATETIME} {
    yylval.datetime = strdup(yytext + 1);
    yylval.datetime[yyleng - 2] = '\0';
    return T_DATETIME;
}

{UTF8SYMBOL}+ {
    yylval.word = strdup(yytext);
    return T_WORD;
}

[[:blank:]] { /* Skip spaces */ }

^[\n]+ {
    yylineno++;
    return T_NEWLINE;
}

[\n] {
    yylineno++;
    return T_NEWLINE;
}
%%

