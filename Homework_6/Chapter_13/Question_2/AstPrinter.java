// LARRY LA
// CS 4080
// HW 6 - Chapter 13, Question 2
// This file implements AstPrinter with inner keyword support for BETA-style method resolution.

package com.craftinginterpreters.lox;

class AstPrinter implements Expr.Visitor<String> {
    String print(Expr expr) {
        return expr.accept(this);
    }

    @Override
    public String visitBinaryExpr(Expr.Binary expr) {
        return parenthesize(expr.operator.lexeme, expr.left, expr.right);
    }

    @Override
    public String visitTernaryExpr(Expr.Ternary expr) {
        return parenthesize("?:", expr.condition, expr.thenBranch, expr.elseBranch);
    }

    @Override
    public String visitGroupingExpr(Expr.Grouping expr) {
        return parenthesize("group", expr.expression);
    }

    @Override
    public String visitLiteralExpr(Expr.Literal expr) {
        if (expr.value == null) return "nil";
        return expr.value.toString();
    }

    @Override
    public String visitUnaryExpr(Expr.Unary expr) {
        return parenthesize(expr.operator.lexeme, expr.right);
    }

    @Override
    public String visitAssignExpr(Expr.Assign expr) {
        return parenthesize("=", new Expr.Variable(expr.name), expr.value);
    }

    @Override
    public String visitVariableExpr(Expr.Variable expr) {
        return expr.name.lexeme;
    }

    @Override
    public String visitCallExpr(Expr.Call expr) {
        return parenthesize("call " + expr.callee.accept(this), 
            expr.arguments.toArray(new Expr[0]));
    }

    @Override
    public String visitGetExpr(Expr.Get expr) {
        return parenthesize("." + expr.name.lexeme, expr.object);
    }

    @Override
    public String visitSetExpr(Expr.Set expr) {
        return parenthesize("=" + expr.name.lexeme, expr.object, expr.value);
    }

    @Override
    public String visitInnerExpr(Expr.Inner expr) {
        return "inner." + expr.method.lexeme;
    }

    @Override
    public String visitThisExpr(Expr.This expr) {
        return "this";
    }

    @Override
    public String visitFunctionExpr(Expr.Function expr) {
        StringBuilder builder = new StringBuilder();
        builder.append("(fun (");
        for (int i = 0; i < expr.params.size(); i++) {
            if (i != 0) builder.append(" ");
            builder.append(expr.params.get(i).lexeme);
        }
        builder.append(") ... )");
        return builder.toString();
    }

    @Override
    public String visitLogicalExpr(Expr.Logical expr) {
        return parenthesize(expr.operator.lexeme, expr.left, expr.right);
    }

    private String parenthesize(String name, Expr... exprs) {
        StringBuilder builder = new StringBuilder();

        builder.append("(").append(name);
        for (Expr expr : exprs) {
            builder.append(" ");
            builder.append(expr.accept(this));
        }
        builder.append(")");

        return builder.toString();
    }
}