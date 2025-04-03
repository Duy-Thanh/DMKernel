#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "../../include/lang/parser.h"

// List of keywords
static const char *keywords[] = {
    "if", "else", "while", "for", "function", "return",
    "break", "continue", "import", "true", "false", "null",
    "let", "const", "var", "matrix", "vector", "int",
    "float", "string", "bool", "void", NULL
};

// Check if a string is a keyword
static bool is_keyword(const char *str) {
    for (int i = 0; keywords[i] != NULL; i++) {
        if (strcmp(str, keywords[i]) == 0) {
            return true;
        }
    }
    return false;
}

// Initialize lexer
dm_error_t dm_lexer_init(dm_context_t *ctx, dm_lexer_t *lexer, const char *source, size_t source_len) {
    if (ctx == NULL || lexer == NULL || source == NULL) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    lexer->ctx = ctx;
    lexer->source = source;
    lexer->source_len = source_len;
    lexer->position = 0;
    lexer->line = 1;
    lexer->column = 1;
    
    // Initialize current token
    lexer->current.type = DM_TOKEN_EOF;
    lexer->current.text = NULL;
    lexer->current.length = 0;
    lexer->current.line = 1;
    lexer->current.column = 1;
    
    return DM_SUCCESS;
}

// Skip whitespace and comments
static void skip_whitespace_and_comments(dm_lexer_t *lexer) {
    while (lexer->position < lexer->source_len) {
        char c = lexer->source[lexer->position];
        
        if (isspace(c)) {
            // Update line and column for newlines
            if (c == '\n') {
                lexer->line++;
                lexer->column = 1;
            } else {
                lexer->column++;
            }
            
            lexer->position++;
        } else if (c == '/' && lexer->position + 1 < lexer->source_len) {
            if (lexer->source[lexer->position + 1] == '/') {
                // Line comment, skip until end of line
                lexer->position += 2;
                lexer->column += 2;
                
                while (lexer->position < lexer->source_len && 
                      lexer->source[lexer->position] != '\n') {
                    lexer->position++;
                    lexer->column++;
                }
            } else if (lexer->source[lexer->position + 1] == '*') {
                // Block comment, skip until */
                lexer->position += 2;
                lexer->column += 2;
                
                while (lexer->position + 1 < lexer->source_len && 
                      !(lexer->source[lexer->position] == '*' && 
                        lexer->source[lexer->position + 1] == '/')) {
                    
                    if (lexer->source[lexer->position] == '\n') {
                        lexer->line++;
                        lexer->column = 1;
                    } else {
                        lexer->column++;
                    }
                    
                    lexer->position++;
                }
                
                if (lexer->position + 1 < lexer->source_len) {
                    // Skip the */
                    lexer->position += 2;
                    lexer->column += 2;
                }
            } else {
                // Not a comment, just a division operator
                break;
            }
        } else {
            // Not whitespace or comment
            break;
        }
    }
}

// Scan the next token
dm_error_t dm_lexer_next_token(dm_lexer_t *lexer, dm_token_t *token) {
    if (lexer == NULL || token == NULL) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    // Skip whitespace and comments
    skip_whitespace_and_comments(lexer);
    
    // Check for end of file
    if (lexer->position >= lexer->source_len) {
        token->type = DM_TOKEN_EOF;
        token->text = NULL;
        token->length = 0;
        token->line = lexer->line;
        token->column = lexer->column;
        return DM_SUCCESS;
    }
    
    // Save token start position
    size_t token_start = lexer->position;
    size_t token_line = lexer->line;
    size_t token_column = lexer->column;
    
    // Get current character
    char c = lexer->source[lexer->position];
    
    if (isalpha(c) || c == '_') {
        // Identifier or keyword
        lexer->position++;
        lexer->column++;
        
        while (lexer->position < lexer->source_len) {
            c = lexer->source[lexer->position];
            if (isalnum(c) || c == '_') {
                lexer->position++;
                lexer->column++;
            } else {
                break;
            }
        }
        
        size_t length = lexer->position - token_start;
        char *text = (char *)lexer->source + token_start;
        
        // Check if it's a keyword
        if (is_keyword(text)) {
            token->type = DM_TOKEN_KEYWORD;
        } else {
            token->type = DM_TOKEN_IDENTIFIER;
        }
        
        token->text = text;
        token->length = length;
    } else if (isdigit(c) || (c == '.' && lexer->position + 1 < lexer->source_len && isdigit(lexer->source[lexer->position + 1]))) {
        // Number
        bool has_dot = (c == '.');
        lexer->position++;
        lexer->column++;
        
        while (lexer->position < lexer->source_len) {
            c = lexer->source[lexer->position];
            
            if (isdigit(c)) {
                lexer->position++;
                lexer->column++;
            } else if (c == '.' && !has_dot) {
                has_dot = true;
                lexer->position++;
                lexer->column++;
            } else if ((c == 'e' || c == 'E') && lexer->position + 1 < lexer->source_len) {
                // Exponent
                char next = lexer->source[lexer->position + 1];
                if (isdigit(next) || next == '+' || next == '-') {
                    lexer->position += 2;
                    lexer->column += 2;
                    
                    // Skip digits after exponent
                    while (lexer->position < lexer->source_len && isdigit(lexer->source[lexer->position])) {
                        lexer->position++;
                        lexer->column++;
                    }
                } else {
                    break;
                }
            } else {
                break;
            }
        }
        
        token->type = DM_TOKEN_NUMBER;
        token->text = (char *)lexer->source + token_start;
        token->length = lexer->position - token_start;
    } else if (c == '"' || c == '\'') {
        // String
        char quote = c;
        lexer->position++;
        lexer->column++;
        
        token_start = lexer->position; // Skip the quote
        
        while (lexer->position < lexer->source_len) {
            c = lexer->source[lexer->position];
            
            if (c == quote) {
                break;
            } else if (c == '\\' && lexer->position + 1 < lexer->source_len) {
                // Escape sequence
                lexer->position += 2;
                lexer->column += 2;
            } else {
                if (c == '\n') {
                    lexer->line++;
                    lexer->column = 1;
                } else {
                    lexer->column++;
                }
                
                lexer->position++;
            }
        }
        
        token->type = DM_TOKEN_STRING;
        token->text = (char *)lexer->source + token_start;
        token->length = lexer->position - token_start;
        
        // Skip closing quote
        if (lexer->position < lexer->source_len) {
            lexer->position++;
            lexer->column++;
        }
    } else if (strchr("+-*/%=<>!&|^~?:.,;()[]{}", c) != NULL) {
        // Operator or symbol
        lexer->position++;
        lexer->column++;
        
        // Check for two-character operators
        if (lexer->position < lexer->source_len) {
            char next = lexer->source[lexer->position];
            
            // Two-character operators
            if ((c == '+' && next == '+') ||
                (c == '-' && next == '-') ||
                (c == '=' && next == '=') ||
                (c == '!' && next == '=') ||
                (c == '<' && next == '=') ||
                (c == '>' && next == '=') ||
                (c == '&' && next == '&') ||
                (c == '|' && next == '|') ||
                (c == '+' && next == '=') ||
                (c == '-' && next == '=') ||
                (c == '*' && next == '=') ||
                (c == '/' && next == '=') ||
                (c == '%' && next == '=') ||
                (c == '&' && next == '=') ||
                (c == '|' && next == '=') ||
                (c == '^' && next == '=') ||
                (c == '>' && next == '>') ||
                (c == '<' && next == '<')) {
                
                lexer->position++;
                lexer->column++;
                
                // Check for three-character operators
                if (lexer->position < lexer->source_len) {
                    char next2 = lexer->source[lexer->position];
                    
                    if (((c == '>' && next == '>' && next2 == '=') ||
                         (c == '<' && next == '<' && next2 == '=') ||
                         (c == '=' && next == '=' && next2 == '='))) {
                        
                        lexer->position++;
                        lexer->column++;
                    }
                }
            }
        }
        
        if (strchr("()[]{};,.", c) != NULL) {
            token->type = DM_TOKEN_SYMBOL;
        } else {
            token->type = DM_TOKEN_OPERATOR;
        }
        
        token->text = (char *)lexer->source + token_start;
        token->length = lexer->position - token_start;
    } else {
        // Unknown character
        lexer->position++;
        lexer->column++;
        
        token->type = DM_TOKEN_SYMBOL; // Treat as symbol for error handling
        token->text = (char *)lexer->source + token_start;
        token->length = 1;
    }
    
    token->line = token_line;
    token->column = token_column;
    
    // Update current token
    lexer->current = *token;
    
    return DM_SUCCESS;
} 