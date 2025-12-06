#ifndef GB_CPU_H
#define GB_CPU_H

struct inst;

struct inst fetch(struct gb_state*);
void execute(struct gb_state*, struct inst);

#endif // GB_CPU_H
