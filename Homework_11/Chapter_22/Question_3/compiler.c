// LARRY LA - CS 4080 - HW 11
/*
Ch.22 Q3: Added `const` as a single-assignment variable keyword.
See lines 360-368, 469-497, 703-736.
Assignments to const variables now fail at compile time.

Example:
Input:
const a = 10;
a = 20;
Output:
[line 2] Error at '=': Can't assign to const variable.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "compiler.h"
#include "object.h"
#include "scanner.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

typedef struct {
  Token current;
  Token previous;
  bool hadError;
  bool panicMode;
} Parser;

typedef enum {
  PREC_NONE,
  PREC_ASSIGNMENT,  // =
  PREC_TERNARY,     // ?:
  PREC_OR,          // or
  PREC_AND,         // and
  PREC_EQUALITY,    // == !=
  PREC_COMPARISON,  // < > <= >=
  PREC_TERM,        // + -
  PREC_FACTOR,      // * /
  PREC_UNARY,       // ! -
  PREC_CALL,        // . ()
  PREC_PRIMARY
} Precedence;

typedef void (*ParseFn)(bool canAssign);

typedef struct {
  ParseFn prefix;
  ParseFn infix;
  Precedence precedence;
} ParseRule;

#define MAX_LOCAL_COUNT (UINT16_MAX + 1)
#define INITIAL_LOCAL_CAPACITY 16
#define INITIAL_GLOBAL_DECL_CAPACITY 16
#define INITIAL_LOCAL_BUCKET_COUNT 256

typedef struct {
  Token name;
  int depth;
  bool isConst;
  uint32_t hash;
  int hashBucket;
  int nextInHash;
  int prevInHash;
} Local;

typedef struct {
  Token name;
  bool isConst;
} GlobalDeclaration;

typedef struct {
  Local* locals;
  int localCount;
  int localCapacity;
  int scopeDepth;
  int* localBuckets;
  int localBucketCount;
  GlobalDeclaration* globals;
  int globalCount;
  int globalCapacity;
} Compiler;

Parser parser;
Compiler* current = NULL;
Chunk* compilingChunk;

static Chunk* currentChunk(void) {
  return compilingChunk;
}

static void expression(void);
static void block(void);
static void statement(void);
static void declaration(void);
static ParseRule* getRule(TokenType type);
static void parsePrecedence(Precedence precedence);
static void beginScope(void);
static void endScope(void);
static void addLocal(Token name, bool isConst);
static void declareVariable(bool isConst);
static void markInitialized(void);
static bool identifiersEqual(Token* a, Token* b);
static int resolveLocal(Compiler* compiler, Token* name);
static void ensureLocalCapacity(Compiler* compiler);
static void ensureGlobalDeclarationCapacity(Compiler* compiler);
static void ensureLocalBucketCapacity(Compiler* compiler, int neededCount);
static uint32_t hashToken(Token* token);
static void addGlobalDeclaration(Token name, bool isConst);
static bool resolveGlobalIsConst(Token* name);
static void removeLocalFromBuckets(int localIndex);

static void errorAt(Token* token, const char* message) {
  if (parser.panicMode) return;
  parser.panicMode = true;

  fprintf(stderr, "[line %d] Error", token->line);

  if (token->type == TOKEN_EOF) {
    fprintf(stderr, " at end");
  } else if (token->type == TOKEN_ERROR) {
    // Nothing.
  } else {
    fprintf(stderr, " at '%.*s'", token->length, token->start);
  }

  fprintf(stderr, ": %s\n", message);
  parser.hadError = true;
}

static void error(const char* message) {
  errorAt(&parser.previous, message);
}

static void errorAtCurrent(const char* message) {
  errorAt(&parser.current, message);
}

static void advance(void) {
  parser.previous = parser.current;

  for (;;) {
    parser.current = scanToken();
    if (parser.current.type != TOKEN_ERROR) break;

    errorAtCurrent(parser.current.start);
  }
}

static void consume(TokenType type, const char* message) {
  if (parser.current.type == type) {
    advance();
    return;
  }

  errorAtCurrent(message);
}

static bool check(TokenType type) {
  return parser.current.type == type;
}

static bool match(TokenType type) {
  if (!check(type)) return false;
  advance();
  return true;
}

static void* checkedRealloc(void* pointer, size_t size) {
  void* result = realloc(pointer, size);
  if (result == NULL) {
    fprintf(stderr, "Out of memory.\n");
    exit(74);
  }

  return result;
}

static void emitByte(uint8_t byte) {
  writeChunk(currentChunk(), byte, parser.previous.line);
}

static void emitBytes(uint8_t byte1, uint8_t byte2) {
  emitByte(byte1);
  emitByte(byte2);
}

static void emitShort(uint16_t value) {
  emitByte((uint8_t)(value >> 8));
  emitByte((uint8_t)value);
}

static void emitReturn(void) {
  emitByte(OP_RETURN);
}

static uint8_t makeConstant(Value value) {
  int constant = addConstant(currentChunk(), value);
  if (constant > UINT8_MAX) {
    error("Too many constants in one chunk.");
    return 0;
  }

  return (uint8_t)constant;
}

static void emitConstant(Value value) {
  emitBytes(OP_CONSTANT, makeConstant(value));
}

static uint32_t hashToken(Token* token) {
  uint32_t hash = 2166136261u;
  for (int i = 0; i < token->length; i++) {
    hash ^= (uint8_t)token->start[i];
    hash *= 16777619;
  }

  return hash;
}

static void ensureLocalCapacity(Compiler* compiler) {
  if (compiler->localCount < compiler->localCapacity) return;

  int oldCapacity = compiler->localCapacity;
  compiler->localCapacity = oldCapacity < INITIAL_LOCAL_CAPACITY
      ? INITIAL_LOCAL_CAPACITY
      : oldCapacity * 2;
  compiler->locals = checkedRealloc(compiler->locals,
      sizeof(Local) * (size_t)compiler->localCapacity);
}

static void ensureGlobalDeclarationCapacity(Compiler* compiler) {
  if (compiler->globalCount < compiler->globalCapacity) return;

  int oldCapacity = compiler->globalCapacity;
  compiler->globalCapacity = oldCapacity < INITIAL_GLOBAL_DECL_CAPACITY
      ? INITIAL_GLOBAL_DECL_CAPACITY
      : oldCapacity * 2;
  compiler->globals = checkedRealloc(compiler->globals,
      sizeof(GlobalDeclaration) * (size_t)compiler->globalCapacity);
}

static void ensureLocalBucketCapacity(Compiler* compiler, int neededCount) {
  if (neededCount < compiler->localBucketCount * 2) return;

  int oldBucketCount = compiler->localBucketCount;
  int newBucketCount = oldBucketCount * 2;
  int* newBuckets = checkedRealloc(NULL, sizeof(int) * (size_t)newBucketCount);
  for (int i = 0; i < newBucketCount; i++) {
    newBuckets[i] = -1;
  }

  for (int i = 0; i < compiler->localCount; i++) {
    Local* local = &compiler->locals[i];
    int bucket = (int)(local->hash & (uint32_t)(newBucketCount - 1));
    local->hashBucket = bucket;
    local->prevInHash = -1;
    local->nextInHash = newBuckets[bucket];
    if (local->nextInHash != -1) {
      compiler->locals[local->nextInHash].prevInHash = i;
    }
    newBuckets[bucket] = i;
  }

  free(compiler->localBuckets);
  compiler->localBuckets = newBuckets;
  compiler->localBucketCount = newBucketCount;
}

static void initCompiler(Compiler* compiler) {
  compiler->locals = NULL;
  compiler->localCount = 0;
  compiler->localCapacity = 0;
  compiler->scopeDepth = 0;
  compiler->localBucketCount = INITIAL_LOCAL_BUCKET_COUNT;
  compiler->localBuckets = checkedRealloc(NULL,
      sizeof(int) * (size_t)compiler->localBucketCount);
  for (int i = 0; i < compiler->localBucketCount; i++) {
    compiler->localBuckets[i] = -1;
  }
  compiler->globals = NULL;
  compiler->globalCount = 0;
  compiler->globalCapacity = 0;
  current = compiler;
}

static void freeCompiler(Compiler* compiler) {
  free(compiler->locals);
  free(compiler->localBuckets);
  free(compiler->globals);
  compiler->locals = NULL;
  compiler->localBuckets = NULL;
  compiler->globals = NULL;
  compiler->localCount = 0;
  compiler->localCapacity = 0;
  compiler->localBucketCount = 0;
  compiler->globalCount = 0;
  compiler->globalCapacity = 0;
}

static void endCompiler(void) {
  emitReturn();

#ifdef DEBUG_PRINT_CODE
  if (!parser.hadError) {
    disassembleChunk(currentChunk(), "code");
  }
#endif
}

static void beginScope(void) {
  current->scopeDepth++;
}

static void endScope(void) {
  current->scopeDepth--;

  while (current->localCount > 0 &&
         current->locals[current->localCount - 1].depth >
             current->scopeDepth) {
    emitByte(OP_POP);
    removeLocalFromBuckets(current->localCount - 1);
    current->localCount--;
  }
}

static uint8_t identifierConstant(Token* name) {
  ObjString* string = copyString(name->start, name->length);
  Value value = OBJ_VAL(string);

  ValueArray* constants = &currentChunk()->constants;
  for (int i = 0; i < constants->count; i++) {
    Value constant = constants->values[i];
    if (IS_STRING(constant) && AS_STRING(constant) == string) {
      return (uint8_t)i;
    }
  }

  return makeConstant(value);
}

static bool identifiersEqual(Token* a, Token* b) {
  if (a->length != b->length) return false;
  return memcmp(a->start, b->start, a->length) == 0;
}

static void addGlobalDeclaration(Token name, bool isConst) {
  ensureGlobalDeclarationCapacity(current);
  GlobalDeclaration* declaration = &current->globals[current->globalCount++];
  declaration->name = name;
  declaration->isConst = isConst;
}

static bool resolveGlobalIsConst(Token* name) {
  for (int i = current->globalCount - 1; i >= 0; i--) {
    GlobalDeclaration* declaration = &current->globals[i];
    if (identifiersEqual(name, &declaration->name)) {
      return declaration->isConst;
    }
  }

  return false;
}

static void removeLocalFromBuckets(int localIndex) {
  Local* local = &current->locals[localIndex];
  if (local->prevInHash != -1) {
    current->locals[local->prevInHash].nextInHash = local->nextInHash;
  } else {
    current->localBuckets[local->hashBucket] = local->nextInHash;
  }

  if (local->nextInHash != -1) {
    current->locals[local->nextInHash].prevInHash = local->prevInHash;
  }
}

static void addLocal(Token name, bool isConst) {
  if (current->localCount == MAX_LOCAL_COUNT) {
    error("Too many local variables in function.");
    return;
  }

  ensureLocalCapacity(current);
  ensureLocalBucketCapacity(current, current->localCount + 1);

  int localIndex = current->localCount++;
  Local* local = &current->locals[localIndex];
  local->name = name;
  local->depth = -1;
  local->isConst = isConst;
  local->hash = hashToken(&name);

  int bucket = (int)(local->hash & (uint32_t)(current->localBucketCount - 1));
  local->hashBucket = bucket;
  local->prevInHash = -1;
  local->nextInHash = current->localBuckets[bucket];
  if (local->nextInHash != -1) {
    current->locals[local->nextInHash].prevInHash = localIndex;
  }
  current->localBuckets[bucket] = localIndex;
}

static void declareVariable(bool isConst) {
  Token* name = &parser.previous;
  if (current->scopeDepth == 0) {
    addGlobalDeclaration(*name, isConst);
    return;
  }

  for (int i = current->localCount - 1; i >= 0; i--) {
    Local* local = &current->locals[i];
    if (local->depth != -1 && local->depth < current->scopeDepth) break;

    if (identifiersEqual(name, &local->name)) {
      error("Already a variable with this name in this scope.");
    }
  }

  addLocal(*name, isConst);
}

static int resolveLocal(Compiler* compiler, Token* name) {
  uint32_t hash = hashToken(name);
  int bucket = (int)(hash & (uint32_t)(compiler->localBucketCount - 1));
  for (int i = compiler->localBuckets[bucket]; i != -1;
       i = compiler->locals[i].nextInHash) {
    Local* local = &compiler->locals[i];
    if (local->hash == hash && identifiersEqual(name, &local->name)) {
        if (local->depth == -1) {
          error("Can't read local variable in its own initializer.");
        }
        return i;
    }
  }

  return -1;
}

static uint8_t parseVariable(const char* errorMessage, bool isConst) {
  consume(TOKEN_IDENTIFIER, errorMessage);
  declareVariable(isConst);
  if (current->scopeDepth > 0) return 0;
  return identifierConstant(&parser.previous);
}

static void markInitialized(void) {
  current->locals[current->localCount - 1].depth = current->scopeDepth;
}

static void defineVariable(uint8_t global) {
  if (current->scopeDepth > 0) {
    markInitialized();
    return;
  }

  emitBytes(OP_DEFINE_GLOBAL, global);
}

static void namedVariable(Token name, bool canAssign) {
  bool isLocal = false;
  bool isConst = false;
  uint8_t getOp, setOp;
  int arg = resolveLocal(current, &name);
  if (arg != -1) {
    isLocal = true;
    isConst = current->locals[arg].isConst;
    getOp = OP_GET_LOCAL;
    setOp = OP_SET_LOCAL;
  } else {
    isConst = resolveGlobalIsConst(&name);
    arg = identifierConstant(&name);
    getOp = OP_GET_GLOBAL;
    setOp = OP_SET_GLOBAL;
  }

  if (canAssign && match(TOKEN_EQUAL)) {
    if (isConst) {
      error("Can't assign to const variable.");
    }
    expression();
    if (isLocal) {
      emitByte(setOp);
      emitShort((uint16_t)arg);
    } else {
      emitBytes(setOp, (uint8_t)arg);
    }
  } else {
    if (isLocal) {
      emitByte(getOp);
      emitShort((uint16_t)arg);
    } else {
      emitBytes(getOp, (uint8_t)arg);
    }
  }
}

static void number(bool canAssign) {
  (void)canAssign;
  double value = strtod(parser.previous.start, NULL);
  emitConstant(NUMBER_VAL(value));
}

static void variable(bool canAssign) {
  namedVariable(parser.previous, canAssign);
}

static void string(bool canAssign) {
  (void)canAssign;
  emitConstant(OBJ_VAL(copyString(parser.previous.start + 1,
                                  parser.previous.length - 2)));
}

static void grouping(bool canAssign) {
  (void)canAssign;
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void unary(bool canAssign) {
  (void)canAssign;
  TokenType operatorType = parser.previous.type;

  // Compile the operand.
  parsePrecedence(PREC_UNARY);

  // Emit the operator instruction.
  switch (operatorType) {
    case TOKEN_BANG: emitByte(OP_NOT); break;
    case TOKEN_MINUS: emitByte(OP_NEGATE); break;
    default: return; // Unreachable.
  }
}

static void literal(bool canAssign) {
  (void)canAssign;
  switch (parser.previous.type) {
    case TOKEN_FALSE: emitByte(OP_FALSE); break;
    case TOKEN_NIL: emitByte(OP_NIL); break;
    case TOKEN_TRUE: emitByte(OP_TRUE); break;
    default: return; // Unreachable.
  }
}

static void ternary(bool canAssign) {
  (void)canAssign;
  // Parse the then branch.
  parsePrecedence(PREC_ASSIGNMENT);
  consume(TOKEN_COLON, "Expect ':' after then branch of conditional operator.");

  // Parse the else branch. This allows right-associative ternaries.
  parsePrecedence(PREC_TERNARY);
}

static void binary(bool canAssign) {
  (void)canAssign;
  TokenType operatorType = parser.previous.type;
  ParseRule* rule = getRule(operatorType);
  parsePrecedence((Precedence)(rule->precedence + 1));

  switch (operatorType) {
    case TOKEN_BANG_EQUAL:    emitBytes(OP_EQUAL, OP_NOT); break;
    case TOKEN_EQUAL_EQUAL:   emitByte(OP_EQUAL); break;
    case TOKEN_GREATER:       emitByte(OP_GREATER); break;
    case TOKEN_GREATER_EQUAL: emitBytes(OP_LESS, OP_NOT); break;
    case TOKEN_LESS:          emitByte(OP_LESS); break;
    case TOKEN_LESS_EQUAL:    emitBytes(OP_GREATER, OP_NOT); break;
    case TOKEN_PLUS:  emitByte(OP_ADD); break;
    case TOKEN_MINUS: emitByte(OP_SUBTRACT); break;
    case TOKEN_STAR:  emitByte(OP_MULTIPLY); break;
    case TOKEN_SLASH: emitByte(OP_DIVIDE); break;
    default: return; // Unreachable.
  }
}

ParseRule rules[] = {
  [TOKEN_LEFT_PAREN]    = {grouping, NULL,   PREC_NONE},
  [TOKEN_RIGHT_PAREN]   = {NULL,     NULL,   PREC_NONE},
  [TOKEN_LEFT_BRACE]    = {NULL,     NULL,   PREC_NONE},
  [TOKEN_RIGHT_BRACE]   = {NULL,     NULL,   PREC_NONE},
  [TOKEN_COMMA]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_DOT]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_MINUS]         = {unary,    binary, PREC_TERM},
  [TOKEN_PLUS]          = {NULL,     binary, PREC_TERM},
  [TOKEN_QUESTION]      = {NULL,     ternary, PREC_TERNARY},
  [TOKEN_COLON]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_SEMICOLON]     = {NULL,     NULL,   PREC_NONE},
  [TOKEN_SLASH]         = {NULL,     binary, PREC_FACTOR},
  [TOKEN_STAR]          = {NULL,     binary, PREC_FACTOR},
  [TOKEN_BANG]          = {unary,    NULL,   PREC_NONE},
  [TOKEN_BANG_EQUAL]    = {NULL,     binary, PREC_EQUALITY},
  [TOKEN_EQUAL]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_EQUAL_EQUAL]   = {NULL,     binary, PREC_EQUALITY},
  [TOKEN_GREATER]       = {NULL,     binary, PREC_COMPARISON},
  [TOKEN_GREATER_EQUAL] = {NULL,     binary, PREC_COMPARISON},
  [TOKEN_LESS]          = {NULL,     binary, PREC_COMPARISON},
  [TOKEN_LESS_EQUAL]    = {NULL,     binary, PREC_COMPARISON},
  [TOKEN_IDENTIFIER]    = {variable, NULL,   PREC_NONE},
  [TOKEN_STRING]        = {string,   NULL,   PREC_NONE},
  [TOKEN_NUMBER]        = {number,   NULL,   PREC_NONE},
  [TOKEN_AND]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_CLASS]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_CONST]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_ELSE]          = {NULL,     NULL,   PREC_NONE},
  [TOKEN_FALSE]         = {literal,  NULL,   PREC_NONE},
  [TOKEN_FOR]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_FUN]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_IF]            = {NULL,     NULL,   PREC_NONE},
  [TOKEN_NIL]           = {literal,  NULL,   PREC_NONE},
  [TOKEN_OR]            = {NULL,     NULL,   PREC_NONE},
  [TOKEN_PRINT]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_RETURN]        = {NULL,     NULL,   PREC_NONE},
  [TOKEN_SUPER]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_THIS]          = {NULL,     NULL,   PREC_NONE},
  [TOKEN_TRUE]          = {literal,  NULL,   PREC_NONE},
  [TOKEN_VAR]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_WHILE]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_ERROR]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_EOF]           = {NULL,     NULL,   PREC_NONE},
};

static ParseRule* getRule(TokenType type) {
  return &rules[type];
}

static void parsePrecedence(Precedence precedence) {
  advance();
  ParseFn prefixRule = getRule(parser.previous.type)->prefix;
  if (prefixRule == NULL) {
    error("Expect expression.");
    return;
  }

  bool canAssign = precedence <= PREC_ASSIGNMENT;
  prefixRule(canAssign);

  while (precedence <= getRule(parser.current.type)->precedence) {
    advance();
    ParseFn infixRule = getRule(parser.previous.type)->infix;
    infixRule(canAssign);
  }

  if (canAssign && match(TOKEN_EQUAL)) {
    error("Invalid assignment target.");
  }
}

static void expression(void) {
  parsePrecedence(PREC_ASSIGNMENT);
}

static void block(void) {
  while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
    declaration();
  }

  consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void synchronize(void) {
  parser.panicMode = false;

  while (parser.current.type != TOKEN_EOF) {
    if (parser.previous.type == TOKEN_SEMICOLON) return;
    switch (parser.current.type) {
      case TOKEN_CLASS:
      case TOKEN_FUN:
      case TOKEN_VAR:
      case TOKEN_CONST:
      case TOKEN_FOR:
      case TOKEN_IF:
      case TOKEN_WHILE:
      case TOKEN_PRINT:
      case TOKEN_RETURN:
        return;

      default:
        ; // Do nothing.
    }

    advance();
  }
}

static void expressionStatement(void) {
  expression();
  consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
  emitByte(OP_POP);
}

static void printStatement(void) {
  expression();
  consume(TOKEN_SEMICOLON, "Expect ';' after value.");
  emitByte(OP_PRINT);
}

static void varDeclaration(bool isConst) {
  uint8_t global = parseVariable("Expect variable name.", isConst);

  if (isConst) {
    consume(TOKEN_EQUAL, "Expect '=' after const variable name.");
    expression();
  } else if (match(TOKEN_EQUAL)) {
    expression();
  } else {
    emitByte(OP_NIL);
  }
  consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

  defineVariable(global);
}

static void statement(void) {
  if (match(TOKEN_PRINT)) {
    printStatement();
  } else if (match(TOKEN_LEFT_BRACE)) {
    beginScope();
    block();
    endScope();
  } else {
    expressionStatement();
  }
}

static void declaration(void) {
  if (match(TOKEN_VAR)) {
    varDeclaration(false);
  } else if (match(TOKEN_CONST)) {
    varDeclaration(true);
  } else {
    statement();
  }

  if (parser.panicMode) synchronize();
}

bool compile(const char* source, Chunk* chunk) {
  initScanner(source);
  Compiler compiler;
  initCompiler(&compiler);
  compilingChunk = chunk;

  parser.hadError = false;
  parser.panicMode = false;

  advance();
  while (!match(TOKEN_EOF)) {
    declaration();
  }

  endCompiler();
  bool success = !parser.hadError;
  freeCompiler(&compiler);
  return success;
}
