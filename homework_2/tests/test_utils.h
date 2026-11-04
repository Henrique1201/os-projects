/*
 * test_utils.h - Micro framework dos testes automatizados.
 *
 * Cada binario de teste deve ser formado por UM unico arquivo .c que
 * inclui este cabecalho (as variaveis globais abaixo tem uma unica definicao
 * por binario).
 *
 * Uso tipico:
 *     #include "test_utils.h"
 *     static void teste_exemplo(void) {
 *         TEST_CHECK(1 == 1, "exemplo");
 *     }
 *     int main(void) {
 *         TEST_RUN(teste_exemplo);
 *         return TEST_SUMMARY();
 *     }
 */
#ifndef TEST_UTILS_H
#define TEST_UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "memory.h"

static int test_checks;
static int test_failures;

#define TEST_CHECK(condition, ...)                                             \
    do {                                                                       \
        test_checks++;                                                         \
        if (!(condition)) {                                                    \
            test_failures++;                                                   \
            fprintf(stderr, "  [FALHA] %s:%d: ", __FILE__, __LINE__);          \
            fprintf(stderr, __VA_ARGS__);                                      \
            fputc('\n', stderr);                                               \
        }                                                                      \
    } while (0)

#define TEST_CHECK_EQ_INT(actual, expected, ...)                               \
    do {                                                                       \
        long long test_a = (long long)(actual);                                \
        long long test_e = (long long)(expected);                              \
        test_checks++;                                                         \
        if (test_a != test_e) {                                                \
            test_failures++;                                                   \
            fprintf(stderr, "  [FALHA] %s:%d: esperado %lld, obtido %lld - ",   \
                    __FILE__, __LINE__, test_e, test_a);                        \
            fprintf(stderr, __VA_ARGS__);                                      \
            fputc('\n', stderr);                                               \
        }                                                                      \
    } while (0)

#define TEST_CHECK_EQ_DOUBLE(actual, expected, tolerance, ...)                 \
    do {                                                                       \
        double test_a = (double)(actual);                                      \
        double test_e = (double)(expected);                                    \
        double test_d = test_a - test_e;                                       \
        test_checks++;                                                         \
        if (test_d < 0) { test_d = -test_d; }                                  \
        if (test_d > (double)(tolerance)) {                                    \
            test_failures++;                                                   \
            fprintf(stderr, "  [FALHA] %s:%d: esperado %.4f, obtido %.4f - ",  \
                    __FILE__, __LINE__, test_e, test_a);                        \
            fprintf(stderr, __VA_ARGS__);                                      \
            fputc('\n', stderr);                                               \
        }                                                                      \
    } while (0)

#define TEST_CHECK_EQ_STR(actual, expected, ...)                               \
    do {                                                                       \
        const char *test_a = (actual);                                         \
        const char *test_e = (expected);                                       \
        test_checks++;                                                         \
        if (test_a == NULL || strcmp(test_a, test_e) != 0) {                   \
            test_failures++;                                                   \
            fprintf(stderr, "  [FALHA] %s:%d: esperado \"%s\", obtido \"%s\" - ", \
                    __FILE__, __LINE__, test_e, test_a ? test_a : "(null)");     \
            fprintf(stderr, __VA_ARGS__);                                      \
            fputc('\n', stderr);                                               \
        }                                                                      \
    } while (0)

#define TEST_CHECK_MEMORY_VALID(memory, ...)                                   \
    do {                                                                       \
        char test_msg[256];                                                    \
        test_checks++;                                                         \
        if (!memory_validate_ex((memory), test_msg, sizeof(test_msg))) {      \
            test_failures++;                                                   \
            fprintf(stderr, "  [FALHA] %s:%d: memoria invalida (%s) - ",       \
                    __FILE__, __LINE__, test_msg);                             \
            fprintf(stderr, __VA_ARGS__);                                      \
            fputc('\n', stderr);                                               \
        }                                                                      \
    } while (0)

#define TEST_RUN(function)                                                     \
    do {                                                                       \
        printf("  Executando %s\n", #function);                                \
        function();                                                            \
    } while (0)

#define TEST_SUMMARY() (test_failures == 0 ? 0 : 1)

#endif /* TEST_UTILS_H */
