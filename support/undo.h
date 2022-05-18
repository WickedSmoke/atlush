#ifndef UNDO_H
#define UNDO_H
/*
  Undo Stack
  Copyright (C) 2022  Karl Robillard

  This software can be redistributed and/or modified under the terms of
  the GNU General Public License (see undo.c).
*/

#include <stdint.h>

typedef union {
    struct {
        uint16_t code;
        uint8_t  skipNext;
        uint8_t  skipPrev;
    } op;
    uint8_t  b[4];
    int16_t  s[2];
    uint16_t h[2];
    int32_t  i;
    uint32_t u;
    float    f;
}
UndoValue;

typedef struct {
    UndoValue* stack;
    uint32_t used;
    uint32_t avail;
    uint32_t pos;
    uint32_t byteLimit;
}
UndoStack;

enum UndoResult {
    Undo_AtEnd,
    Undo_Advanced,
    Undo_AdvancedFromStart = 2,
    Undo_AdvancedToEnd     = 4
};

#define Undo_Term   0

#ifdef __cplusplus
extern "C" {
#endif

int  undo_init(UndoStack*, uint32_t byteLimit);
void undo_free(UndoStack*);
void undo_clear(UndoStack*);
void undo_record(UndoStack*, uint16_t opcode, const UndoValue* data, int values);
int  undo_stepBack(UndoStack*, const UndoValue** step);
int  undo_stepForward(UndoStack*, const UndoValue** step);

#ifdef __cplusplus
}
#endif

#endif  // UNDO_H
