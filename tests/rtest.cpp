
// To use lazy optional ?? in strings, trigraphs should be disabled or we
// simply use ?\?
// Or disable trigraphs by enabling the GNU standard:
// c++ -std=gnu++11 -Wall test.cpp pattern.cpp lexer.cpp

#include <reflex/lexer.h>

// #define REFLEX_RTEST_INTERACTIVE // for interactive mode testing

void banner(const char *title)
{
  int i;
  printf("\n\n/");
  for (i = 0; i < 78; i++)
    putchar('*');
  printf("\\\n *%76s*\n * %-75s*\n *%76s*\n\\", "", title, "");
  for (i = 0; i < 78; i++)
    putchar('*');
  printf("/\n\n");
}

static void error(const char *text)
{
  std::cout << "FAILED: " << text << std::endl;
  exit(EXIT_FAILURE);
}

using namespace reflex;

class WrappedLexer : public Lexer {
 public:
  WrappedLexer() : Lexer(), source(0)
  { }
 private:
  virtual bool wrap()
  {
    switch (source++)
    {
      case 0: in = "Hello World!";
              return true;
      case 1: in = "How now brown cow.";
              return true;
      case 2: in = "An apple a day.";
              return true;
    }
    return false;
  }
  int source;
};

struct Test {
  const char *pattern;
  const char *popts;
  const char *mopts;
  const char *cstring;
  size_t accepts[32];
};

Test tests[] = {
  { "ab", "", "", "ab", { 1 } },
  { "ab", "", "", "abab", { 1, 1 } },
  { "ab|xy", "", "", "abxy", { 1, 2 } },
  { "a(p|q)z", "", "", "apzaqz", { 1, 1 } },
  // DFA edge compaction test
  { "[a-cg-ik]z|d|[e-g]|j|y|[x-z]|.|\\n", "", "", "azz", { 1, 6 } },
  // POSIX character classes
  {
    "[[:ASCII:]]-"
    "[[:space:]]-"
    "[[:xdigit:]]-"
    "[[:cntrl:]]-"
    "[[:print:]]-"
    "[[:alnum:]]-"
    "[[:alpha:]]-"
    "[[:blank:]]-"
    "[[:digit:]]-"
    "[[:graph:]]-"
    "[[:lower:]]-"
    "[[:punct:]]-"
    "[[:upper:]]-"
    "[[:word:]]", "", "", "\x7E-\r-F-\x01-&-0-A-\t-0-#-l-.-U-_", { 1 } },
  {
    "\\p{ASCII}-"
    "\\p{Space}-"
    "\\p{XDigit}-"
    "\\p{Cntrl}-"
    "\\p{Print}-"
    "\\p{Alnum}-"
    "\\p{Alpha}-"
    "\\p{Blank}-"
    "\\p{Digit}-"
    "\\p{Graph}-"
    "\\p{Lower}-"
    "\\p{Punct}-"
    "\\p{Upper}-"
    "\\p{Word}", "", "", "\x7E-\r-F-\x01-&-0-A-\t-0-#-l-.-U-_", { 1 } },
  { "[\\s]-"
    "[\\cA-\\cZ\\x1b-\\x1f\\x7f]-"
    "[\\d]-"
    "[\\l]-"
    "[\\u]-"
    "[\\w]", "", "", "\r-\x01-0-l-U-_", { 1 } },
  // Pattern option e
  { "%(%x41%xFF%)", "e=%", "", "(A\xFF)", { 1 } },
  // Pattern option q
  { "\"(^|$)\\\"\\.+\"", "q", "", "(^|$)\"\\.+", { 1 } },
  { "(?q:\"(^|$)\\\"\\.+\")", "", "", "(^|$)\"\\.+", { 1 } },
  { "\\Q(^|$)\"\\.+\\E", "", "", "(^|$)\"\\.+", { 1 } },
  // Pattern option i
  { "(?i:abc)", "", "", "abcABC", { 1, 1 } },
  { "(?i)abc|xyz", "", "", "abcABCxyzXYZ", { 1, 1, 2, 2 } },
  { "(?i:abc)|xyz", "", "", "abcABCxyz", { 1, 1, 2 } },
  { "(?i:abc)|(?i:xyz)", "", "", "abcABCxyzXYZ", { 1, 1, 2, 2 } },
  { "(?i)abc|(?-i:xyz)|(?-i:XYZ)", "", "", "abcABCxyzXYZ", { 1, 1, 2, 3 } },
  { "(?i:abc(?-i:xyz))|ABCXYZ", "", "", "abcxyzABCxyzABCXYZ", { 1, 1, 2 } },
  // Pattern option x
  { "(?x) a\tb\n c | ( xy ) z ?", "", "", "abcxy", { 1, 2 } },
  { "(?x: a b\n c)", "", "", "abc", { 1 } },
  { "(?x) a b c\n|\n# COMMENT\n x y z", "", "", "abcxyz", { 1, 2 } },
  { "(?# test option (?x:... )(?x: a b c)|x y z", "", "", "abcx y z", { 1, 2 } },
  // Pattern option s
  { "(?s).", "", "", "a\n", { 1, 1 } },
  { "(?s:.)", "", "", "a\n", { 1, 1 } },
  { "(?s).", "", "", "a\n", { 1, 1 } },
  // Anchors \A, \z, ^, and $
  { "\\Aa\\z", "", "", "a", { 1 } },
  { "^a$", "", "", "a", { 1 } },
  { "(?m)^a$|\\n", "m", "", "a\na", { 1, 2, 1 } },
  { "(?m)^a|a$|a|\\n", "m", "", "aa\naaa", { 1, 2, 4, 1, 3, 2 } },
  { "(?m)\\Aa\\z|\\Aa|a\\z|^a$|^a|a$|a|^ab$|^ab|ab$|ab|\\n", "m", "", "a\na\naa\naaa\nab\nabab\nababab\na", { 2, 12, 4, 12, 5, 6, 12, 5, 7, 6, 12, 8, 12, 9, 10, 12, 9, 11, 10, 12, 3 } },
  // Optional X?
  { "a?z", "", "", "azz", { 1, 1 } },
  // Closure X*
  { "a*z", "", "", "azaazz", { 1, 1, 1 } },
  // Positive closure X+
  { "a+z", "", "", "azaaz", { 1, 1 } },
  // Combi ? * +
  { "a?b+|a", "", "", "baba", { 1, 1, 2 } },
  { "a*b+|a", "", "", "baabaa", { 1, 1, 2, 2 } },
  // Iterations {n,m}
  { "ab{2}", "", "", "abbabb", { 1, 1 } },
  { "ab{2,3}", "", "", "abbabbb", { 1, 1 } },
  { "ab{2,}", "", "", "abbabbbabbbb", { 1, 1, 1 } },
  { "ab{0,}", "", "", "a", { 1 } },
  { "(ab{0,2}c){2}", "", "", "abbcacabcabc", { 1, 1 } },
  // Lazy optional X?
  { "(a|b)?\?a", "", "", "aaba", { 1, 1, 1 } },
  { "a(a|b)?\?(?=a|ab)|ac", "", "", "aababac", { 1, 1, 1, 2 } },
  { "(a|b)?\?(a|b)?\?aa", "", "", "baaaabbaa", { 1, 1, 1 } },
  { "(a|b)?\?(a|b)?\?(a|b)?\?aaa", "", "", "baaaaaa", { 1, 1 } },
  { "a?\?b?a", "", "", "aba", { 1, 1 } }, // 'a' 'ba'
  { "a?\?b?b", "", "", "abb", { 1 } }, // 'abb'
  // Lazy closure X*
  { "a*?a", "", "", "aaaa", { 1, 1, 1, 1 } },
  { "a*?|a|b", "", "", "aab", { 2, 2, 3 } },
  { "(a|bb)*?abb", "", "", "abbbbabb", { 1, 1 } },
  { "ab*?|b", "", "", "ab", { 1, 2 } },
  { "(ab)*?|b", "", "", "b", { 2 } },
  { "a(ab)*?|b", "", "", "ab", { 1, 2 } },
  { "(a|b)*?a|c?", "", "", "bbaaac", { 1, 1, 1, 2 } },
  { "a(a|b)*?a", "", "", "aaaba", { 1, 1 } },
  { "a(a|b)*?a?\?|b", "", "", "aaaba", { 1, 1, 1, 2, 1 } },
  { "a(a|b)*?a?", "", "", "aa", { 1 } },
  { "a(a|b)*?a|a", "", "", "aaaba", { 1, 1 } },
  { "a(a|b)*?a|a?", "", "", "aaaba", { 1, 1 } },
  { "a(a|b)*?a|a?\?", "", "", "aaaba", { 1, 1 } },
  { "a(a|b)*?a|aa?", "", "", "aaaba", { 1, 1 } },
  { "a(a|b)*?a|aa?\?", "", "", "aaaba", { 1, 1 } },
  { "ab(ab|cd)*?ab|ab", "", "", "abababcdabab", { 1, 1, 2 } },
  { "(a|b)(a|b)*?a|a", "", "", "aaabaa", { 1, 1, 2 } },
  { "(ab|cd)(ab|cd)*?ab|ab", "", "", "abababcdabab", { 1, 1, 2 } },
  { "(ab)(ab)*?a|b", "", "", "abababa", { 1, 2, 1 } },
  { "a?(a|b)*?a", "", "", "aaababba", { 1, 1, 1, 1 } },
  { "(?m)^(a|b)*?a", "m", "", "bba", { 1 } },
  { "(?m)(a|b)*?a$", "m", "", "bba", { 1 } }, // OK: ending anchors & lazy quantifiers
  { "(a|b)*?a\\b", "", "", "bba", { 1 } }, // OK but limited: ending anchors & lazy quantifiers
  { "(?m)^(a|b)*?|b", "m", "", "ab", { 1, 2 } },
  // Lazy positive closure X+
  { "a+?a", "", "", "aaaa", { 1, 1 } },
  { "(a|b)+?", "", "", "ab", { 1, 1 } },
  { "(a|b)+?a", "", "", "bbaaa", { 1, 1 } },
  { "(a|b)+?a|c?", "", "", "bbaaa", { 1, 1 } },
  { "(ab|cd)+?ab|d?", "", "", "cdcdababab", { 1, 1 } },
  { "(ab)+?a|b", "", "", "abababa", { 1, 2, 1 } },
  { "(ab)+?ac", "", "", "ababac", { 1 } },
  { "ABB*?|ab+?|A|a", "", "", "ABab", { 1, 2 } },
  { "(a|b)+?a|a", "", "", "bbaaa", { 1, 1 } },
  { "(?m)^(a|b)+?a", "m", "", "abba", { 1 } }, // TODO can starting anchors invalidate lazy quantifiers?
  { "(?m)(a|b)+?a$", "m", "", "abba", { 1 } }, // OK ending anchors at & lazy quantifiers
  // Lazy iterations {n,m}
  { "(a|b){0,3}?aaa", "", "", "baaaaaa", { 1, 1 } },
  { "(a|b){1,3}?aaa", "", "", "baaaaaaa", { 1, 1 } },
  { "(a|b){1,3}?aaa", "", "", "bbbaaaaaaa", { 1, 1 } },
  { "(ab|cd){0,3}?ababab", "", "", "cdabababababab", { 1, 1 } },
  { "(ab|cd){1,3}?ababab", "", "", "cdababababababab", { 1, 1 } },
  { "(a|b){1,}?a|a", "", "", "bbaaa", { 1, 1 } },
  { "(a|b){2,}?a|aa", "", "", "bbbaaaa", { 1, 1 } },
  // Bracket lists
  { "[a-z]", "", "", "abcxyz", { 1, 1, 1, 1, 1, 1 } },
  { "[a-d-z]", "", "", "abcd-z", { 1, 1, 1, 1, 1, 1 } },
  { "[-z]", "", "", "-z", { 1, 1 } },
  { "[z-]", "", "", "-z", { 1, 1 } },
  { "[--z]", "", "", "-az", { 1, 1, 1 } },
  { "[ --]", "", "", " +-", { 1, 1, 1 } },
  { "[^a-z]", "", "", "A", { 1 } },
  { "[[:alpha:]]", "", "", "abcxyz", { 1, 1, 1, 1, 1, 1 } },
  { "[\\p{Alpha}]", "", "", "abcxyz", { 1, 1, 1, 1, 1, 1 } },
  { "[][]", "", "", "[]", { 1, 1 } },
  // Lookahead
  { "a(?=bc)|ab(?=d)|bc|d", "", "", "abcdabd", { 1, 3, 4, 2, 4 } },
  { "a(a|b)?(?=a)|a", "", "", "aba", { 1, 2 } }, // Ambiguous, undefined in POSIX
  { "zx*(?=xy*)|x?y*", "", "", "zxxy", { 1, 2 } }, // Ambiguous, undefined in POSIX
  // { "[ab]+(?=ab)|-|ab", "", "", "aaab-bbab", { 1, 3, 2, 1, 3 } }, // Ambiguous, undefined in POSIX
  { "(?m)a(?=b?)|bc", "m", "", "aabc", { 1, 1, 2 } },
  { "(?m)a(?=\\nb)|a|^b|\\n", "m", "", "aa\nb\n", { 2, 1, 4, 3, 4 } },
  { "(?m)^a(?=b$)|b|\\n", "m", "", "ab\n", { 1, 2, 3 } },
  { "(?m)a(?=\n)|a|\\n", "m", "", "aa\n", { 2, 1, 3 } },
  { "(?m)^( +(?=a)|b)|a|\\n", "m", "", " a\n  a\nb\n", { 1, 2, 3, 1, 2, 3, 1, 3 } },
  { "abc(?=\\w+|(?^def))|xyzabcdef", "", "", "abcxyzabcdef", { 1, 2 } },
  // Negative patterns and option A (all)
  { "(?^ab)|\\w+| ", "", "", "aa ab abab ababba", { 2, 3, 3, 2, 3, 2 } },
  { "(?^ab)|\\w+| ", "", "A", "aa ab abab ababba", { 2, 3, reflex::Lexer::Const::REDO, 3, 2, 3, 2 } },
  { "\\w+|(?^ab)| ", "", "", "aa ab abab ababba", { 1, 3, 3, 1, 3, 1 } }, // non-reachable warning is given, but works
  { "\\w+|(?^\\s)", "", "", "99 Luftballons", { 1, 1 } },
  { "(\\w+|(?^ab(?=\\w*)))| ", "", "", "aa ab abab ababba", { 1, 2, 2, 2, 1 } },
  { "(?^ab(?=\\w*))|\\w+| ", "", "", "aa ab abab ababba", { 2, 3, 3, 3, 2 } },
  // Word boundaries \<, \>, \b, and \B
  { "\\<a\\>|\\<a|a\\>|a|-", "", "", "a-aaa", { 1, 5, 2, 4, 3 } },
  { "\\<.*\\>", "", "", "abc def", { 1 } },
  { "\\<.*\\>|-", "", "", "abc-", { 1, 2 } },
  { "\\b.*\\b|-", "", "", "abc-", { 1, 2 } },
  { "-|\\<.*\\>", "", "", "-abc-", { 1, 2, 1 } },
  { "-|\\b.*\\b", "", "", "-abc-", { 1, 2, 1 } },
  { "\\<(-|a)(-|a)\\>| ", "", "", "aa aa", { 1, 2, 1 } },
  { "\\b(-|a)(-|a)\\b| ", "", "", "aa aa", { 1, 2, 1 } },
  { "\\B(-|a)(-|a)\\B|b|#", "", "", "baab#--#", { 2, 1, 2, 3, 1, 3 } },
  { "\\<.*ab\\>|[ab]*|-|\\n", "", "", "-aaa-aaba-aab-\n-aaa", { 3, 1, 3, 4, 3, 2 } },
  // Indent and lexer option T (Tab)
  { "(?m)^[ \\t]+|[ \\t]+\\i|[ \\t]*\\j|a|[ \\n]", "m", "", "a\n  a\n  a\n    a\n", { 4, 5, 2, 4, 5, 1, 4, 5, 2, 4, 5, 3, 3 } },
  { "(?m)^[ \\t]+|^[ \\t]*\\i|^[ \\t]*\\j|\\j|a|[ \\n]", "m", "", "a\n  a\n  a\n    a\n", { 5, 6, 2, 5, 6, 1, 5, 6, 2, 5, 6, 4, 4 } },
  { "(?m)^[ \\t]+|[ \\t]*\\i|[ \\t]*\\j|a|[ \\n]", "m", "", "a\n  a\n  a\n    a\na\n", { 4, 5, 2, 4, 5, 1, 4, 5, 2, 4, 5, 3, 3, 4, 5 } },
  { "(?m)^[ \\t]+|[ \\t]*\\i|[ \\t]*\\j|a|[ \\n]", "m", "", "a\n  a\n  a\n    a\n  a\na\n", { 4, 5, 2, 4, 5, 1, 4, 5, 2, 4, 5, 3, 4, 5, 3, 4, 5 } },
  { "(?m)^[ \\t]+|[ \\t]*\\i|[ \\t]*\\j|a|[ \\n]", "m", "T=2", "a\n  a\n\ta\n    a\n\ta\na\n", { 4, 5, 2, 4, 5, 1, 4, 5, 2, 4, 5, 3, 4, 5, 3, 4, 5 } },
  { "(?m)^[ \\t]+|[ \\t]*\\i|[ \\t]*\\j|a|(?^[ \\n])", "m", "", "a\n\n  a\n\n  a\n\n    a\n\n  a\na\n", { 4, 2, 4, 1, 4, 2, 4, 3, 4, 3, 4 } },
  { "(?m)[ \\t]*\\i|^[ \\t]+|[ \\t]*\\j|a|(?^[ \\n])", "m", "", "a\n  a\n  a\n    a\n  a\na\n", { 4, 1, 4, 2, 4, 1, 4, 3, 4, 3, 4 } },
  // { "(?m)[ \\t]*\\ia|^[ \\t]+|[ \\t]*\\ja|[ \\t]*\\j|a|[ \\n]", "m", "", "a\n  a\na\n", { 5, 6, 1, 6, 3, 6 } }, \\ \i and \j must be at pattern ends (like $)
  { "(?m)_*\\i|^_+|_*\\j|\\w|(?^[ \\n])", "m", "", "a\n__a\n__a\n____a\n__a\na\n", { 4, 1, 4, 2, 4, 1, 4, 3, 4, 3, 4 } },
  { "(?m)[ \\t]*\\i|^[ \\t]+|[ \\t]*\\j|a|[ \\n]|(?^^[ \\t]*#\n)", "m", "", "a\n  a\n    #\n  a\n    a\n#\n  a\na\n", { 4, 5, 1, 4, 5, 2, 4, 5, 1, 4, 5, 3, 4, 5, 3, 4, 5 } },
  { "[ \\t]*\\i|^[ \\t]+|[ \\t]*\\j|a|[ \\n]|(?^\\\\\n[ \\t]+)", "m", "", "a\n  a\n  a\\\n      a a\n    a\n  a\na\n", { 4, 5, 1, 4, 5, 2, 4, 4, 5, 4, 5, 1, 4, 5, 3, 4, 5, 3, 4, 5 } },
  // { "(?m)[ \\t]*\\i|^[ \\t]+|[ \\t]*\\j|a|[ \\n]|(?^\\\\\n[ \\t]*)", "m", "", "a\n  a\n  a\\\na\n    a\n  a\na\n", { 4, 5, 1, 4, 5, 2, 4, 4, 5, 1, 4, 5, 2, 3, 4, 5, 3, 4, 5 } }, // TODO line continuation stopping at left margin triggers dedent
  // Unicode or UTF-8 (TODO: requires a flag and changes to the parser so that UTF-8 multibyte chars are parsed as ONE char)
  { "(©)+", "", "", "©", { 1 } },
  { nullptr, nullptr, nullptr, nullptr, { } }
};

int main()
{
  banner("PATTERN TESTS");
  for (const Test *test = tests; test->pattern != nullptr; ++test)
  {
    Pattern pattern(test->pattern, test->popts);
    Lexer lexer(test->cstring, test->mopts);
    lexer.patterns.push_back(std::move(pattern));
#ifdef REFLEX_RTEST_INTERACTIVE
    lexer.interactive();
#endif
    printf("Test \"%s\" against \"%s\"\n", test->pattern, test->cstring);
    if (*test->popts)
      printf("With pattern options \"%s\"\n", test->popts);
    if (*test->mopts)
      printf("With lexer options \"%s\"\n", test->mopts);
    for (Pattern::Index i = 1; i <= pattern.size(); ++i)
      if (!pattern.reachable(i))
        printf("WARNING: pattern[%u]=\"%s\" not reachable\n", i, pattern[i].c_str());
    size_t i = 0;
    while (lexer.scan())
    {
      printf("  At %zu,%zu;[%zu,%zu]: \"%s\" matches pattern[%zu]=\"%s\" from %u choice(s)\n", lexer.lineno(), lexer.columno(), lexer.first(), lexer.last(), lexer.text(), lexer.accept(), pattern[lexer.accept()].c_str(), pattern.size());
      if (lexer.accept() != test->accepts[i])
        break;
      ++i;
    }
    if (lexer.accept() != 0 || test->accepts[i] != 0 || !lexer.at_end())
    {
      if (!lexer.at_end())
        printf("ERROR: remaining input rest = '%s'; dumping dump.gv and dump.cpp\n", lexer.rest());
      else
        printf("ERROR: accept = %zu text = '%s'; dumping dump.gv and dump.cpp\n", lexer.accept(), lexer.text());
      std::string options(test->popts);
      options.append(";f=dump.gv,dump.cpp");
      Pattern(test->pattern, options);
      exit(1);
    }
    printf("OK\n\n");
  }
  Pattern pattern0("\\w+|\\W", "f=dump.cpp");
  Pattern pattern1("\\<.*\\>", "f=dump.gv");
  Pattern pattern2(" ");
  Pattern pattern3("[ \\t]+");
  Pattern pattern4("\\b", "f=dump.gv,dump.cpp");
  Pattern pattern5("");
  Pattern pattern6("[[:alpha:]]");
  Pattern pattern7("\\w+");
  Pattern pattern8(Lexer::convert("(?u:\\p{L})"));

  Lexer lexer;
  lexer.patterns.push_back(std::move(pattern0));
  lexer.patterns.push_back(std::move(pattern1));
  lexer.patterns.push_back(std::move(pattern2));
  lexer.patterns.push_back(std::move(pattern3));
  lexer.patterns.push_back(std::move(pattern4));
  lexer.patterns.push_back(std::move(pattern5));
  lexer.patterns.push_back(std::move(pattern6));
  lexer.patterns.push_back(std::move(pattern7));
  lexer.patterns.push_back(std::move(pattern8));
  std::string test;
  //
  banner("TEST FIND");
  //
  lexer.pattern_current=8;
  lexer.input("an apple a day");
  test = "";
  while (lexer.find())
  {
    std::cout << lexer.text() << "/";
    test.append(lexer.text()).append("/");
  }
  std::cout << std::endl;
  if (test != "an/apple/a/day/")
    error("find results");
  //
  lexer.pattern_current=6;
  lexer.reset("N");
  lexer.input("a a");
  test = "";
  while (lexer.find())
  {
    std::cout << lexer.text() << "/";
    test.append(lexer.text()).append("/");
  }
  std::cout << std::endl;
  if (test != "///")
    error("find with nullable results");
  lexer.reset("");
  //
  lexer.pattern_current=7;
  lexer.reset("N");
  lexer.input("a a");
  test = "";
  while (lexer.find())
  {
    std::cout << lexer.text() << "/";
    test.append(lexer.text()).append("/");
  }
  std::cout << std::endl;
  if (test != "///")
    error("find with nullable results");
  lexer.reset("");
  //
  banner("TEST SPLIT");
  //
  lexer.pattern_current=3;
  lexer.input("ab c  d");
  test = "";
  while (lexer.split())
  {
    std::cout << lexer.text() << "/";
    test.append(lexer.text()).append("/");
  }
  std::cout << std::endl;
  if (test != "ab/c//d/")
    error("split results");
  //
  lexer.pattern_current=3;
  lexer.input("ab c  d ");
  test = "";
  while (lexer.split())
  {
    std::cout << lexer.text() << "/";
    test.append(lexer.text()).append("/");
  }
  std::cout << std::endl;
  if (test != "ab/c//d//")
    error("split results");
  //
  lexer.pattern_current=4;
  lexer.input("ab c  d");
  test = "";
  while (lexer.split())
  {
    std::cout << lexer.text() << "/";
    test.append(lexer.text()).append("/");
  }
  std::cout << std::endl;
  if (test != "ab/c/d/")
    error("split results");
  //
  lexer.pattern_current=5;
  lexer.input("ab c  d");
  test = "";
  while (lexer.split())
  {
    std::cout << lexer.text() << "/";
    test.append(lexer.text()).append("/");
  }
  std::cout << std::endl;
  if (test != "/ab/ /c/  /d//")
    error("split results");
  //
  lexer.pattern_current=6;
  lexer.input("ab c  d");
  test = "";
  while (lexer.split())
  {
    std::cout << lexer.text() << "/";
    test.append(lexer.text()).append("/");
  }
  std::cout << std::endl;
  if (test != "/a/b/ /c/ / /d//")
    error("split results");
  //
  lexer.pattern_current=6;
  lexer.input("");
  test = "";
  while (lexer.split())
  {
    std::cout << lexer.text() << "/";
    test.append(lexer.text()).append("/");
  }
  std::cout << std::endl;
  if (test != "/")
    error("split results");
  //
  lexer.pattern_current=7;
  lexer.input("a-b");
  test = "";
  while (lexer.split())
  {
    std::cout << lexer.text() << "/";
    test.append(lexer.text()).append("/");
  }
  std::cout << std::endl;
  if (test != "/-//")
    error("split results");
  //
  lexer.pattern_current=7;
  lexer.input("a");
  test = "";
  while (lexer.split())
  {
    std::cout << lexer.text() << "/";
    test.append(lexer.text()).append("/");
  }
  std::cout << std::endl;
  if (test != "//")
    error("split results");
  //
  lexer.pattern_current=7;
  lexer.input("-");
  test = "";
  while (lexer.split())
  {
    std::cout << lexer.text() << "/";
    test.append(lexer.text()).append("/");
  }
  std::cout << std::endl;
  if (test != "-/")
    error("split results");
  //
  lexer.pattern_current=4;
  lexer.input("ab c  d");
  int n = 2; // split 2
  while (n-- && lexer.split())
    std::cout << lexer.text() << "/";
  std::cout << std::endl << "REST = " << lexer.rest() << std::endl;
  //
  banner("TEST INPUT/UNPUT");
  //
  lexer.pattern_current=2;
  lexer.input("ab c  d");
  while (!lexer.at_end())
    std::cout << (char)lexer.input() << "/";
  std::cout << std::endl;
  //
  lexer.pattern_current=2;
  lexer.input("ab c  d");
  test = "";
  while (true)
  {
    if (lexer.scan())
    {
      std::cout << lexer.text() << "/";
      test.append(lexer.text()).append("/");
    }
    else if (!lexer.at_end())
    {
      std::cout << (char)lexer.input() << "?/";
      test.append("?/");
    }
    else
    {
      break;
    }
  }
  std::cout << std::endl;
  if (test != "ab c  d/")
    error("input");
  //
  lexer.pattern_current=7;
  lexer.input("ab c  d");
  test = "";
  while (true)
  {
    if (lexer.scan())
    {
      std::cout << lexer.text() << "/";
      test.append(lexer.text()).append("/");
    }
    else if (!lexer.at_end())
    {
      std::cout << (char)lexer.input() << "?/";
      test.append("?/");
    }
    else
    {
      break;
    }
  }
  std::cout << std::endl;
  if (test != "a/b/?/c/?/?/d/")
    error("input");
  //
  lexer.pattern_current=7;
  lexer.input("ab c  d");
  lexer.unput('a');
  test = "";
  while (true)
  {
    if (lexer.scan())
    {
      std::cout << lexer.text() << "/";
      test.append(lexer.text()).append("/");
      if (*lexer.text() == 'b')
        lexer.unput('c');
    }
    else if (!lexer.at_end())
    {
      std::cout << (char)lexer.input() << "?/";
    }
    else
    {
      break;
    }
  }
  std::cout << std::endl;
  if (test != "a/a/b/c/c/d/")
    error("unput");
  //
  lexer.pattern_current=9;
  lexer.input("ab c  d");
  lexer.wunput(L'ä');
  test = "";
  while (true)
  {
    if (lexer.scan())
    {
      std::cout << lexer.text() << "/";
      test.append(lexer.text()).append("/");
      if (*lexer.text() == 'b')
        lexer.wunput(L'ç');
    }
    else if (!lexer.at_end())
    {
      std::cout << (char)lexer.winput() << "?/";
    }
    else
    {
      break;
    }
  }
  std::cout << std::endl;
  if (test != "ä/a/b/ç/c/d/")
    error("wunput");
  //
  banner("TEST WRAP");
  //
  WrappedLexer wrapped_lexer;
  wrapped_lexer.patterns.push_back(lexer.patterns[7]);
  test = "";
  while (wrapped_lexer.find())
  {
    std::cout << wrapped_lexer.text() << "/";
    test.append(wrapped_lexer.text()).append("/");
  }
  std::cout << std::endl;
  if (test != "Hello/World/How/now/brown/cow/An/apple/a/day/")
    error("wrap");
  //
  banner("TEST REST");
  //
  lexer.pattern_current=8;
  lexer.input("abc def xyz");
  test = "";
  if (lexer.find())
  {
    std::cout << lexer.text() << "/";
    test.append(lexer.text()).append("/");
  }
  std::cout << std::endl;
  if (test != "abc/" || strcmp(lexer.rest(), " def xyz") != 0)
    error("rest");
  //
  banner("TEST SKIP");
  //
  lexer.pattern_current=8;
  lexer.input("abc  \ndef xyz");
  test = "";
  if (lexer.scan())
  {
    std::cout << lexer.text() << "/";
    test.append(lexer.text()).append("/");
    lexer.skip('\n');
  }
  if (lexer.scan())
  {
    std::cout << lexer.text() << "/";
    test.append(lexer.text()).append("/");
    lexer.skip('\n');
  }
  std::cout << std::endl;
  if (test != "abc/def/")
    error("skip");
  //
  lexer.input("abc  ¶def¶");
  test = "";
  if (lexer.scan())
  {
    std::cout << lexer.text() << "/";
    test.append(lexer.text()).append("/");
    lexer.skip(L'¶');
  }
  if (lexer.scan())
  {
    std::cout << lexer.text() << "/";
    test.append(lexer.text()).append("/");
    lexer.skip(L'¶');
  }
  //
  lexer.input("abc  xxydef xx");
  test = "";
  if (lexer.scan())
  {
    std::cout << lexer.text() << "/";
    test.append(lexer.text()).append("/");
    lexer.skip("xy");
  }
  if (lexer.scan())
  {
    std::cout << lexer.text() << "/";
    test.append(lexer.text()).append("/");
    lexer.skip("xy");
  }
  std::cout << std::endl;
  if (test != "abc/def/")
    error("skip");
  //
#ifdef REFLEX_WITH_SPAN
  banner("TEST SPAN");
  //
  lexer.pattern_current=8;
  lexer.input("##a#b#c##\ndef##\n##ghi\n##xyz");
  test = "";
  while (lexer.find())
  {
    std::cout << lexer.span() << "/";
    test.append(lexer.span()).append("/");
  }
  std::cout << std::endl;
  if (test != "##a#b#c##/def##/##ghi/##xyz/")
    error("span");
  //
  banner("TEST LINE");
  //
  lexer.pattern_current=8;
  lexer.input("##a#b#c##\ndef##\n##ghi\n##xyz");
  test = "";
  while (lexer.find())
  {
    std::cout << lexer.line() << "/";
    test.append(lexer.line()).append("/");
  }
  std::cout << std::endl;
  if (test != "##a#b#c##/##a#b#c##/##a#b#c##/def##/##ghi/##xyz/")
    error("line");
#endif
  //
  banner("TEST MORE");
  //
  lexer.pattern_current=7;
  lexer.input("abc");
  test = "";
  while (lexer.scan())
  {
    std::cout << lexer.text() << "/";
    lexer.more();
    test.append(lexer.text()).append("/");
  }
  std::cout << std::endl;
  if (test != "a/ab/abc/")
    error("more");
  //
  banner("TEST LESS");
  //
  lexer.pattern_current=1;
  lexer.input("abc");
  test = "";
  while (lexer.scan())
  {
    lexer.less(1);
    std::cout << lexer.text() << "/";
    test.append(lexer.text()).append("/");
  }
  std::cout << std::endl;
  if (test != "a/b/c/")
    error("less");
  //
  banner("TEST MATCHES");
  //
  if (Lexer("\\w+", "hello").matches()) // on the fly string matching
    std::cout << "OK";
  else
    error("match results");
  std::cout << std::endl;
  if (Lexer("\\d", "0").matches())
    std::cout << "OK";
  else
    error("match results");
  std::cout << std::endl;
  //
  lexer.pattern_current=1;
  lexer.input("abc");
  if (lexer.matches())
    std::cout << "OK";
  else
    error("match results");
  std::cout << std::endl;
  //
  lexer.pattern_current=2;
  lexer.input("abc");
  if (lexer.matches())
    std::cout << "OK";
  else
    error("match results");
  std::cout << std::endl;
  //
  lexer.pattern_current=6;
  lexer.input("");
  if (lexer.matches())
    std::cout << "OK";
  else
    error("match results");
  std::cout << std::endl;

  //
  lexer.pattern_current=2;
  lexer.input("---");
  if (!lexer.matches())
    std::cout << "OK";
  else
    error("match results");
  std::cout << std::endl;
  //
  banner("DONE");
  return 0;
}
