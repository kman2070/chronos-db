#ifndef ELECTION_H
#define ELECTION_H

int try_become_active(const char *lock_path);
void release_active(int lock_fd);

#endif
