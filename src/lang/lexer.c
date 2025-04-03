#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "../../include/lang/parser.h"
#include "../../include/core/debug.h"

// Override debug macros locally to disable them
#undef DM_DEBUG_ERROR_PRINT
#undef DM_DEBUG_WARN_PRINT
#undef DM_DEBUG_INFO_PRINT
#undef DM_DEBUG_VERBOSE_PRINT
#define DM_DEBUG_ERROR_PRINT(...)
#define DM_DEBUG_WARN_PRINT(...)
#define DM_DEBUG_INFO_PRINT(...)
#define DM_DEBUG_VERBOSE_PRINT(...)

// Keywords (alphabetically sorted)
static const char *KEYWORDS[] = {
    "break", "case", "class", "const", "continue", "default",
    "else", "export", "extends", "false", "for", "function",
    "if", "import", "let", "null", "return", "static", "super",
    "switch", "this", "true", "var", "while"
};

static const size_t KEYWORD_COUNT = sizeof(KEYWORDS) / sizeof(KEYWORDS[0]);

// Initialize lexer
dm_error_t dm_lexer_init(dm_context_t *ctx, dm_lexer_t *lexer, const char *source, size_t source_len) {
    if (ctx == NULL || lexer == NULL || source == NULL) {
        DM_DEBUG_ERROR_PRINT("Invalid arguments to dm_lexer_init\n");
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    DM_DEBUG_INFO_PRINT("Initializing lexer with source length: %zu\n", source_len);
    
    // Initialize lexer
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
    
    DM_DEBUG_INFO_PRINT("Lexer initialized successfully.\n");
    return DM_SUCCESS;
}

// Skip whitespace and comments
static void skip_whitespace_and_comments(dm_lexer_t *lexer) {
    bool skipped_something;
    
    do {
        skipped_something = false;
        
        // Skip whitespace
        while (lexer->position < lexer->source_len && 
               (isspace(lexer->source[lexer->position]) || lexer->source[lexer->position] == '\r')) {
            
            if (lexer->source[lexer->position] == '\n') {
                lexer->line++;
                lexer->column = 1;
            } else {
                lexer->column++;
            }
            
            lexer->position++;
            skipped_something = true;
        }
        
        // Skip single-line comments
        if (lexer->position + 1 < lexer->source_len && 
            lexer->source[lexer->position] == '/' && lexer->source[lexer->position + 1] == '/') {
            
            lexer->position += 2;  // Skip '//'
            lexer->column += 2;
            
            while (lexer->position < lexer->source_len && lexer->source[lexer->position] != '\n') {
                lexer->position++;
                lexer->column++;
            }
            
            skipped_something = true;
        }
        
        // Skip multi-line comments
        if (lexer->position + 1 < lexer->source_len && 
            lexer->source[lexer->position] == '/' && lexer->source[lexer->position + 1] == '*') {
            
            lexer->position += 2;  // Skip '/*'
            lexer->column += 2;
            
            while (lexer->position + 1 < lexer->source_len && 
                   !(lexer->source[lexer->position] == '*' && lexer->source[lexer->position + 1] == '/')) {
                
                if (lexer->source[lexer->position] == '\n') {
                    lexer->line++;
                    lexer->column = 1;
                } else {
                    lexer->column++;
                }
                
                lexer->position++;
            }
            
            if (lexer->position + 1 < lexer->source_len) {
                lexer->position += 2;  // Skip '*/'
                lexer->column += 2;
                skipped_something = true;
            }
        }
    } while (skipped_something);
}

// Scan the next token
dm_error_t dm_lexer_next_token(dm_lexer_t *lexer, dm_token_t *token) {
    if (lexer == NULL || token == NULL) {
        DM_DEBUG_ERROR_PRINT("Invalid arguments to dm_lexer_next_token\n");
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    // Skip whitespace and comments first
    skip_whitespace_and_comments(lexer);
    
    // Check if we're at the end of the file
    if (lexer->position >= lexer->source_len) {
        token->type = DM_TOKEN_EOF;
        token->text = NULL;
        token->length = 0;
        token->line = lexer->line;
        token->column = lexer->column;
        
        DM_DEBUG_VERBOSE_PRINT("TOKEN: EOF\n");
        return DM_SUCCESS;
    }
    
    token->line = lexer->line;
    token->column = lexer->column;
    
    // Get the current character
    char c = lexer->source[lexer->position];
    
    // Scan based on the first character
    if (isalpha(c) || c == '_') {
        // Identifier or keyword
        size_t start = lexer->position;
        
        while (lexer->position < lexer->source_len && 
               (isalnum(lexer->source[lexer->position]) || lexer->source[lexer->position] == '_')) {
            lexer->position++;
            lexer->column++;
        }
        
        token->text = (char*)&lexer->source[start];
        token->length = lexer->position - start;
        
        // Check if it's a keyword
        bool is_keyword = false;
        for (size_t i = 0; i < KEYWORD_COUNT; i++) {
            if (token->length == strlen(KEYWORDS[i]) &&
                strncmp(token->text, KEYWORDS[i], token->length) == 0) {
                token->type = DM_TOKEN_KEYWORD;
                is_keyword = true;
                break;
            }
        }
        
        if (!is_keyword) {
            token->type = DM_TOKEN_IDENTIFIER;
        }
        
        DM_DEBUG_VERBOSE_PRINT("TOKEN: %s '%.*s'\n", 
                token->type == DM_TOKEN_KEYWORD ? "KEYWORD" : "IDENTIFIER", 
                (int)token->length, token->text);
        return DM_SUCCESS;
    }
    else if (isdigit(c)) {
        // Number
        size_t start = lexer->position;
        bool has_decimal = false;
        
        while (lexer->position < lexer->source_len && 
               (isdigit(lexer->source[lexer->position]) || 
                lexer->source[lexer->position] == '.')) {
            
            if (lexer->source[lexer->position] == '.') {
                if (has_decimal) {
                    break;  // Can't have two decimal points
                }
                has_decimal = true;
            }
            
            lexer->position++;
            lexer->column++;
        }
        
        token->type = DM_TOKEN_NUMBER;
        token->text = (char*)&lexer->source[start];
        token->length = lexer->position - start;
        
        DM_DEBUG_VERBOSE_PRINT("TOKEN: NUMBER '%.*s'\n", (int)token->length, token->text);
        return DM_SUCCESS;
    }
    else if (c == '"' || c == '\'') {
        // String
        char quote = c;
        size_t start = lexer->position;
        
        // Skip the opening quote
        lexer->position++;
        lexer->column++;
        
        // Find the closing quote
        while (lexer->position < lexer->source_len && 
               lexer->source[lexer->position] != quote) {
            
            // Handle escape sequences
            if (lexer->source[lexer->position] == '\\' && 
                lexer->position + 1 < lexer->source_len) {
                lexer->position++;
                lexer->column++;
            }
            
            lexer->position++;
            lexer->column++;
        }
        
        // Check if we found the closing quote
        if (lexer->position >= lexer->source_len) {
            // Unterminated string
            DM_DEBUG_ERROR_PRINT("Syntax error: Unterminated string starting at line %zu, column %zu\n", 
                               token->line, token->column);
            return DM_ERROR_SYNTAX_ERROR;
        }
        
        // Skip the closing quote
        lexer->position++;
        lexer->column++;
        
        token->type = DM_TOKEN_STRING;
        token->text = (char*)&lexer->source[start];
        token->length = lexer->position - start;
        
        DM_DEBUG_VERBOSE_PRINT("TOKEN: STRING '%.*s'\n", (int)token->length, token->text);
        return DM_SUCCESS;
    }
    else if (strchr("+-*/%=<>!&|^~", c)) {
        // Operator
        size_t start = lexer->position;
        
        // Handle multi-character operators
        if (lexer->position + 1 < lexer->source_len) {
            char next = lexer->source[lexer->position + 1];
            
            if ((c == '=' && next == '=') || 
                (c == '!' && next == '=') ||
                (c == '<' && next == '=') ||
                (c == '>' && next == '=') ||
                (c == '&' && next == '&') ||
                (c == '|' && next == '|')) {
                
                lexer->position += 2;
                lexer->column += 2;
                
                token->type = DM_TOKEN_OPERATOR;
                token->text = (char*)&lexer->source[start];
                token->length = 2;
                
                DM_DEBUG_VERBOSE_PRINT("TOKEN: OPERATOR '%.*s'\n", (int)token->length, token->text);
                return DM_SUCCESS;
            }
        }
        
        // Single-character operator
        lexer->position++;
        lexer->column++;
        
        token->type = DM_TOKEN_OPERATOR;
        token->text = (char*)&lexer->source[start];
        token->length = 1;
        
        DM_DEBUG_VERBOSE_PRINT("TOKEN: OPERATOR '%.*s'\n", (int)token->length, token->text);
        return DM_SUCCESS;
    }
    else if (strchr("()[]{};,.", c)) {
        // Symbol
        token->type = DM_TOKEN_SYMBOL;
        token->text = (char*)&lexer->source[lexer->position];
        token->length = 1;
        
        lexer->position++;
        lexer->column++;
        
        DM_DEBUG_VERBOSE_PRINT("TOKEN: SYMBOL '%.*s'\n", (int)token->length, token->text);
        return DM_SUCCESS;
    }
    else {
        // Unknown character
        DM_DEBUG_ERROR_PRINT("Syntax error: Unknown character '%c' at line %zu, column %zu\n", 
                           c, lexer->line, lexer->column);
        return DM_ERROR_SYNTAX_ERROR;
    }
} 