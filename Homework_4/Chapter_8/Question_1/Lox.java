// LARRY LA - CS 4080 - HW 4

/* 
The code below for Ch.8 challenge Question 1 (see Lines 65, 81-118)

The REPL no longer supports entering a single expression and automatically printing its result value. 
That's a drag. Add support to the REPL to let users type in both statements and expressions. 
If they enter a statement, execute it. If they enter an expression, evaluate it and display the result value.

SOLUTION:
- Line 65: Modified runPrompt() to call new runRepl() instead of run()
- Lines 81-118: Added runRepl() method that intelligently handles both expressions and statements
  * First tries parsing input as statements
  * If it's a single expression statement, evaluates and prints the result  
  * If parsing fails, tries parsing as bare expression and prints result
  * Falls back to normal statement execution

Also requires parseExpression() method in Parser.java and public evaluate()/stringify() in Interpreter.java.

EXAMPLE INPUT/OUTPUT:
> 2 + 3
5
> print "Hello World";
Hello World

NOTE: Running from here won't work, if you would like to run the code, please use the Lox.java in the book directory.
*/

package com.craftinginterpreters.lox;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.nio.charset.Charset;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.List;

public class Lox {
  private static final Interpreter interpreter = new Interpreter();
  static boolean hadError = false;
  static boolean hadRuntimeError = false;

  public static void main(String[] args) throws IOException {
    if (args.length > 1) {
      System.out.println("Usage: jlox [script]");
      System.exit(64);
    } else if (args.length == 1) {
      runFile(args[0]);
    } else {
      runPrompt();
    }
  }

  private static void runFile(String path) throws IOException {
    byte[] bytes = Files.readAllBytes(Paths.get(path));
    run(new String(bytes, Charset.defaultCharset()));

    if (hadError) System.exit(65);
    if (hadRuntimeError) System.exit(70);
  }

  private static void runPrompt() throws IOException {
    InputStreamReader input = new InputStreamReader(System.in);
    BufferedReader reader = new BufferedReader(input);

    for (;;) { 
      System.out.print("> ");
      String line = reader.readLine();
      if (line == null) break;
      runRepl(line);
      hadError = false;
    }
  }

  private static void run(String source) {
    Scanner scanner = new Scanner(source);
    List<Token> tokens = scanner.scanTokens();
    Parser parser = new Parser(tokens);
    List<Stmt> statements = parser.parse();

    if (hadError) return;

    interpreter.interpret(statements);
  }

  private static void runRepl(String source) {
    Scanner scanner = new Scanner(source);
    List<Token> tokens = scanner.scanTokens();
    Parser parser = new Parser(tokens);
    
    // First try to parse as statements
    List<Stmt> statements = parser.parse();
    
    // If parsing succeeded and we have exactly one statement, 
    // check if it's an expression statement that could be evaluated and printed
    if (!hadError && statements.size() == 1 && statements.get(0) instanceof Stmt.Expression) {
      Stmt.Expression exprStmt = (Stmt.Expression) statements.get(0);
      Object value = interpreter.evaluate(exprStmt.expression);
      System.out.println(interpreter.stringify(value));
      return;
    }
    
    // If we had parsing errors, try parsing as a bare expression
    if (hadError) {
      hadError = false; // Reset error state
      scanner = new Scanner(source);
      tokens = scanner.scanTokens();
      parser = new Parser(tokens);
      
      try {
        Expr expression = parser.parseExpression();
        if (!hadError) {
          Object value = interpreter.evaluate(expression);
          System.out.println(interpreter.stringify(value));
          return;
        }
      } catch (Exception e) {
        // Fall back to normal statement execution
        hadError = false;
        run(source);
        return;
      }
    }
    
    // Normal statement execution
    if (!hadError) {
      interpreter.interpret(statements);
    }
  }

  static void error(int line, String message) {
    report(line, "", message);
  }

  private static void report(int line, String where,
                             String message) {
    System.err.println(
        "[line " + line + "] Error" + where + ": " + message);
    hadError = true;
  }

  static void error(Token token, String message) {
    if (token.type == TokenType.EOF) {
      report(token.line, " at end", message);
    } else {
      report(token.line, " at '" + token.lexeme + "'", message);
    }
  }

  static void runtimeError(RuntimeError error) {
    System.err.println(error.getMessage() + 
        "\n[line " + error.token.line + "]");
    hadRuntimeError = true;
  }
}