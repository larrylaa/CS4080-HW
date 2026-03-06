// LARRY LA
// CS 4080
// HW 6 - Chapter 13, Question 3
// This file adds F_STRING token type to support f-string interpolation.
// F-strings use syntax like f"Hello {name}" where {expression} gets evaluated.

package com.craftinginterpreters.lox;

enum TokenType {
    // Single-character tokens.
    LEFT_PAREN, RIGHT_PAREN, LEFT_BRACE, RIGHT_BRACE,
    COMMA, DOT, MINUS, PLUS, QUESTION, COLON, SEMICOLON, SLASH, STAR,

    // One or two character tokens.
    BANG, BANG_EQUAL,
    EQUAL, EQUAL_EQUAL,
    GREATER, GREATER_EQUAL,
    LESS, LESS_EQUAL,

    // Literals.
    IDENTIFIER, STRING, F_STRING, NUMBER,

    // Keywords.
    AND, BREAK, CLASS, ELSE, FALSE, FUN, FOR, IF, INNER, NIL, OR,
    PRINT, RETURN, THIS, TRUE, VAR, WHILE,

    EOF
}
