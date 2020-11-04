/*ファイルの読み込みのしかた、プッシュのしかたなどが、
調べたもののうまく理解できなかった。
そのため、最低限ルイさんのプログラムをトレースして、
コメントをいれてプログラムを理解することにした。

理解の及ばなかったコメント文は、ルイさんの書いた文そのままになっている*/

#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
  TOKEN_RESERVED, // キーワードまたは句読点
  TOKEN_NUM,      // 整数
  TOKEN_EOF,      // 終末
} TokenKind;

// トークンタイプ
typedef struct Token Token;
struct Token {
  TokenKind kind; // 種類
  Token *next;    // 次のトークン
  int val;        // If kind is TOKEN_NUM, its value
  char *str;      // 文字列のトークン
};

//プログラム読み込み
char *user_input;

//トークン用意
Token *token;

// エラー処理
void error(char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);
}

// エラー場所の報告
void error_at(char *loc, char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);

  int pos = loc - user_input;
  fprintf(stderr, "%s\n", user_input);
  fprintf(stderr, "%*s", pos, ""); // print pos spaces.
  fprintf(stderr, "^ ");
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);
}

// Consumes the current token if it matches `op`.
bool consume(char op) {
  if (token->kind != TOKEN_RESERVED || token->str[0] != op)
    return false;
  token = token->next;
  return true;
}

// Ensure that the current token is `op`.
void expect(char op) {
  if (token->kind != TOKEN_RESERVED || token->str[0] != op)
    error_at(token->str, "expected '%c'", op);
  token = token->next;
}

// Ensure that the current token is TOKEN_NUM.
int expect_number() {
  if (token->kind != TOKEN_NUM)
    error_at(token->str, "expected a number");
  int val = token->val;
  token = token->next;
  return val;
}

bool at_eof() {
  return token->kind == TOKEN_EOF;
}

// 新しいトークンを作成し、それを次のトークンとしてcurに格納
Token *new_token(TokenKind kind, Token *cur, char *str) {
  Token *tok = calloc(1, sizeof(Token));
  tok->kind = kind;
  tok->str = str;
  cur->next = tok;
  return tok;
}

// Tokenize `user_input` and returns new tokens.
Token *tokenize() {
  char *p = user_input;
  Token head;
  head.next = NULL;
  Token *cur = &head;

  while (*p) {
    // 空白を飛ばす
    if (isspace(*p)) {
      p++;
      continue;
    }

    // Punctuator
    if (strchr("+-*/()", *p)) {
      cur = new_token(TOKEN_RESERVED, cur, p++);
      continue;
    }

    // Integer literal
    if (isdigit(*p)) {
      cur = new_token(TOKEN_NUM, cur, p);
      cur->val = strtol(p, &p, 10);
      continue;
    }

    error_at(p, "invalid token");
  }

  new_token(TOKEN_EOF, cur, p);
  return head.next;
}

//
// Parser
//

typedef enum {
  ND_ADD, // +
  ND_SUB, // -
  ND_MUL, // *
  ND_DIV, // /
  ND_NUM, // 整数
} NodeKind;

// AST node type
typedef struct Node Node;
struct Node {
  NodeKind kind; // ノードの種類
  Node *lhs;     // 左側
  Node *rhs;     // 右側
  int val;       // Used if kind == ND_NUM
};

Node *new_node(NodeKind kind) {
  Node *node = calloc(1, sizeof(Node));
  node->kind = kind;
  return node;
}

Node *new_binary(NodeKind kind, Node *lhs, Node *rhs) {
  Node *node = new_node(kind);
  node->lhs = lhs;
  node->rhs = rhs;
  return node;
}

Node *new_num(int val) {
  Node *node = new_node(ND_NUM);
  node->val = val;
  return node;
}

Node *expr();
Node *mul();
Node *primary();

// expr = mul ("+" mul | "-" mul)*
Node *expr() {
  Node *node = mul();

  for (;;) {
    if (consume('+'))
      node = new_binary(ND_ADD, node, mul());
    else if (consume('-'))
      node = new_binary(ND_SUB, node, mul());
    else
      return node;
  }
}

// mul = primary ("*" primary | "/" primary)*
Node *mul() {
  Node *node = primary();

  for (;;) {
    if (consume('*'))
      node = new_binary(ND_MUL, node, primary());
    else if (consume('/'))
      node = new_binary(ND_DIV, node, primary());
    else
      return node;
  }
}

// primary = "(" expr ")" | num
Node *primary() {
  if (consume('(')) {
    Node *node = expr();
    expect(')');
    return node;
  }

  return new_num(expect_number());
}

//
// Code generator
//

void gen(Node *node) {
  if (node->kind == ND_NUM) {
    printf("  push %d\n", node->val);
    return;
  }

  gen(node->lhs);
  gen(node->rhs);

  printf("  pop rdi\n");
  printf("  pop rax\n");

  switch (node->kind) {
  case ND_ADD:
    printf("  add rax, rdi\n");
    break;
  case ND_SUB:
    printf("  sub rax, rdi\n");
    break;
  case ND_MUL:
    printf("  imul rax, rdi\n");
    break;
  case ND_DIV:
    printf("  cqo\n");
    printf("  idiv rdi\n");
    break;
  }

  printf("  push rax\n");
}

int main(int argc, char **argv) {
  if (argc != 2)
    error("%s: invalid number of arguments", argv[0]);

  // トークン化して解析
  user_input = argv[1];
  token = tokenize();
  Node *node = expr();

  // Print out the first half of assembly.
  printf(".intel_syntax noprefix\n");
  printf(".global main\n");
  printf("main:\n");

  // Traverse the AST to emit assembly.
  gen(node);

  // A result must be at the top of the stack, so pop it
  // to RAX to make it a program exit code.
  printf("  pop rax\n");
  printf("  ret\n");
  return 0;
}