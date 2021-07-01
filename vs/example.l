%top{
#include <iostream>
#include <cstdio>

using namespace std;
%}

%class{
%}


%%

.       	{ cout << "[" << str() << "]"; }

%%

int main(void)
{
  FILE* fi = fopen ("prova.txt","r") ;

  Lexer lexer(fi);

  while( !feof(fi) )
	  lexer.lex();

  fclose(fi) ;

  cout << endl ;

  return 0;
}