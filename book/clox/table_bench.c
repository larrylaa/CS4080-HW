#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include "table.h"
#include "value.h"
#include "vm.h"

VM vm;

void printObject(Value value) {
  (void)value;
}

static uint64_t splitmix64(uint64_t* state) {
  uint64_t z = (*state += 0x9e3779b97f4a7c15ULL);
  z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
  z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
  return z ^ (z >> 31);
}

static int64_t nowNs(void) {
  return (int64_t)((double)clock() * (1000000000.0 / CLOCKS_PER_SEC));
}

static uint64_t benchSequential(int n) {
  Table table;
  initTable(&table);
  uint64_t sum = 0;

  for (int i = 0; i < n; i++) {
    tableSet(&table, NUMBER_VAL((double)i), NUMBER_VAL((double)(i * 2)));
  }

  for (int i = 0; i < n; i++) {
    Value out;
    if (tableGet(&table, NUMBER_VAL((double)i), &out)) {
      sum += (uint64_t)AS_NUMBER(out);
    }
  }

  freeTable(&table);
  return sum;
}

static uint64_t benchRandom(int n) {
  Table table;
  initTable(&table);
  uint64_t sum = 0;
  uint64_t state = 123456789ULL;

  for (int i = 0; i < n; i++) {
    uint64_t r = splitmix64(&state) & ((1ULL << 52) - 1);
    tableSet(&table, NUMBER_VAL((double)r), NUMBER_VAL((double)i));
  }

  state = 123456789ULL;
  for (int i = 0; i < n; i++) {
    uint64_t r = splitmix64(&state) & ((1ULL << 52) - 1);
    Value out;
    if (tableGet(&table, NUMBER_VAL((double)r), &out)) {
      sum += (uint64_t)AS_NUMBER(out);
    }
  }

  freeTable(&table);
  return sum;
}

static uint64_t benchDeleteChurn(int n, int rounds) {
  Table table;
  initTable(&table);
  uint64_t sum = 0;

  for (int i = 0; i < n; i++) {
    tableSet(&table, NUMBER_VAL((double)i), NUMBER_VAL((double)i));
  }

  for (int r = 0; r < rounds; r++) {
    for (int i = 0; i < n; i++) {
      tableDelete(&table, NUMBER_VAL((double)i));
    }
    for (int i = 0; i < n; i++) {
      tableSet(&table, NUMBER_VAL((double)i), NUMBER_VAL((double)(i + r)));
    }
  }

  for (int i = 0; i < n; i++) {
    Value out;
    if (tableGet(&table, NUMBER_VAL((double)i), &out)) {
      sum += (uint64_t)AS_NUMBER(out);
    }
  }

  freeTable(&table);
  return sum;
}

static uint64_t benchMissHeavy(int n, int missesPerKey) {
  Table table;
  initTable(&table);
  uint64_t sum = 0;

  for (int i = 0; i < n; i++) {
    tableSet(&table, NUMBER_VAL((double)i), NUMBER_VAL((double)(i + 1)));
  }

  int misses = n * missesPerKey;
  for (int i = 0; i < misses; i++) {
    Value out;
    if (!tableGet(&table, NUMBER_VAL((double)(i + n * 4)), &out)) {
      sum++;
    }
  }

  freeTable(&table);
  return sum;
}

static uint64_t benchTinyKeyspace(int ops) {
  Table table;
  initTable(&table);
  uint64_t sum = 0;

  for (int i = 0; i < ops; i++) {
    tableSet(&table, BOOL_VAL(true), NUMBER_VAL((double)i));
    tableSet(&table, BOOL_VAL(false), NUMBER_VAL((double)(i + 1)));
    tableSet(&table, NIL_VAL, NUMBER_VAL((double)(i + 2)));

    Value out;
    if (tableGet(&table, BOOL_VAL(true), &out)) sum += (uint64_t)AS_NUMBER(out);
    if (tableGet(&table, BOOL_VAL(false), &out)) sum += (uint64_t)AS_NUMBER(out);
    if (tableGet(&table, NIL_VAL, &out)) sum += (uint64_t)AS_NUMBER(out);
  }

  freeTable(&table);
  return sum;
}

static void runCase(const char* name, uint64_t (*fn)(void), int repeats) {
  int64_t best = INT64_MAX;
  int64_t total = 0;
  uint64_t checksum = 0;

  for (int i = 0; i < repeats; i++) {
    int64_t start = nowNs();
    checksum ^= fn();
    int64_t elapsed = nowNs() - start;
    if (elapsed < best) best = elapsed;
    total += elapsed;
  }

  printf("%-24s best=%8.3f ms  avg=%8.3f ms  checksum=%llu\n",
         name,
         best / 1000000.0,
         (double)total / repeats / 1000000.0,
         (unsigned long long)checksum);
}

static uint64_t caseSequential(void) { return benchSequential(40000); }
static uint64_t caseRandom(void) { return benchRandom(40000); }
static uint64_t caseDeleteChurn(void) { return benchDeleteChurn(25000, 6); }
static uint64_t caseMissHeavy(void) { return benchMissHeavy(30000, 8); }
static uint64_t caseTinyKeyspace(void) { return benchTinyKeyspace(500000); }

int main(void) {
  printf("Hash Table Benchmark Cases\n");
  printf("1) Sequential numbers: set N + get N\n");
  printf("2) Random numbers: set N + get N\n");
  printf("3) Delete churn: set N + (delete N, set N)*rounds + get N\n");
  printf("4) Miss-heavy: set N + (N*8 failed gets)\n");
  printf("5) Tiny keyspace: repeated set/get on true/false/nil\n");
  printf("\nResults\n");

  runCase("sequential_numbers", caseSequential, 5);
  runCase("random_numbers", caseRandom, 5);
  runCase("delete_churn", caseDeleteChurn, 5);
  runCase("miss_heavy", caseMissHeavy, 5);
  runCase("tiny_keyspace", caseTinyKeyspace, 5);
  return 0;
}
