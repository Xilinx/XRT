/**
 * Copyright (C) 2016-2017 Xilinx, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

#ifndef _CPU_PIPES_H_
#define _CPU_PIPES_H_

#include <assert.h>
#include <pthread.h>
#include <queue>
#include <cstring>
#include <cstdlib>

//#define PIPE_VERBOSE 1
#ifdef PIPE_VERBOSE
#include <stdio.h>
#endif

//#define EXPORT_PIPE_SYMBOLS 1

#ifdef EXPORT_PIPE_SYMBOLS
#define EXPORT
#else
#define EXPORT static
#endif

#ifdef __GNUC__
# define MAYBE_UNUSED __attribute__((unused))
#endif



extern "C" {

typedef struct _cpu_pipe_reserve_id_t {
  std::size_t head;
  std::size_t tail;
  std::size_t next;
  unsigned size;
  unsigned ref;
} cpu_pipe_reserve_id_t;

typedef struct _cpu_pipe_t {
  pthread_mutex_t rd_lock;
  pthread_mutex_t wr_lock;

  std::size_t pkt_size;
  std::size_t pipe_size;

  std::size_t head;
  std::size_t tail;

  std::deque<cpu_pipe_reserve_id_t*> rd_rids;
  std::deque<cpu_pipe_reserve_id_t*> wr_rids;

  char buf[0];
} cpu_pipe_t;


/*
 * 6.13.16.2 - work-item builtins, non-reservation, non-locking
 */

MAYBE_UNUSED
EXPORT int
cpu_write_pipe_nolock(void *v, void *e)
{
  cpu_pipe_t *p = (cpu_pipe_t*)v;

#ifdef PIPE_VERBOSE
  printf("cpu_write_pipe_nolock %p %p\n", v, e);
#endif

  std::size_t head = p->head;
  std::size_t next = (head + p->pkt_size) % p->pipe_size;

  while (next == p->tail);

  std::memcpy(&p->buf[head], e, p->pkt_size);
  p->head = next;

  return 0;
}

MAYBE_UNUSED
EXPORT int
cpu_write_pipe_nb_nolock(void *v, void *e)
{
  cpu_pipe_t *p = (cpu_pipe_t*)v;

#ifdef PIPE_VERBOSE
  printf("cpu_write_pipe_nb_nolock %p %p\n", v, e);
#endif

  std::size_t head = p->head;
  std::size_t next = (head + p->pkt_size) % p->pipe_size;

  if (next == p->tail) {
    return -1;
  }

  std::memcpy(&p->buf[head], e, p->pkt_size);
  p->head = next;
  return 0;
}

MAYBE_UNUSED
EXPORT int
cpu_read_pipe_nolock(void *v, void *e)
{
  cpu_pipe_t *p = (cpu_pipe_t*)v;

#ifdef PIPE_VERBOSE
  printf("cpu_read_pipe_nolock %p %p\n", v, e);
#endif

  std::size_t tail = p->tail;

  while (p->head == tail);

  std::memcpy(e, &p->buf[tail], p->pkt_size);
  p->tail = (tail + p->pkt_size) % p->pipe_size;
  return 0;
}

MAYBE_UNUSED
EXPORT int
cpu_read_pipe_nb_nolock(void *v, void *e)
{
  cpu_pipe_t *p = (cpu_pipe_t*)v;

#ifdef PIPE_VERBOSE
  printf("cpu_read_pipe_nb_nolock %p %p\n", v, e);
#endif

  std::size_t tail = p->tail;

  if (p->head == tail) {
    return -1;
  }

  std::memcpy(e, &p->buf[tail], p->pkt_size);
  p->tail = (tail + p->pkt_size) % p->pipe_size;

  return 0;
}


MAYBE_UNUSED
EXPORT int
cpu_peek_pipe_nb_nolock(void *v, void *e)
{
  cpu_pipe_t *p = (cpu_pipe_t*)v;

#ifdef PIPE_VERBOSE
  printf("cpu_peek_pipe_nb_nolock %p %p\n", v, e);
#endif

  std::size_t tail = p->tail;

  if (p->head == tail) {
    return -1;
  }
  std::memcpy(e, &p->buf[tail], p->pkt_size);
  return 0;
}


/*
 * 6.13.16.2 - work-item builtins, non-reservation, locking
 */

MAYBE_UNUSED
EXPORT int
cpu_write_pipe(void *v, void *e)
{
  cpu_pipe_t *p = (cpu_pipe_t*)v;
  pthread_mutex_lock(&p->wr_lock);
  int ret = cpu_write_pipe_nolock(v, e);
  pthread_mutex_unlock(&p->wr_lock);
  return ret;
}

MAYBE_UNUSED
EXPORT int
cpu_write_pipe_nb(void *v, void *e)
{
  cpu_pipe_t *p = (cpu_pipe_t*)v;
  pthread_mutex_lock(&p->wr_lock);
  int ret = cpu_write_pipe_nb_nolock(v, e);
  pthread_mutex_unlock(&p->wr_lock);
  return ret;
}

MAYBE_UNUSED
EXPORT int
cpu_read_pipe(void *v, void *e)
{
  cpu_pipe_t *p = (cpu_pipe_t*)v;
  pthread_mutex_lock(&p->rd_lock);
  int ret = cpu_read_pipe_nolock(v, e);
  pthread_mutex_unlock(&p->rd_lock);
  return ret;
}

MAYBE_UNUSED
EXPORT int
cpu_read_pipe_nb(void *v, void *e)
{
  cpu_pipe_t *p = (cpu_pipe_t*)v;
  pthread_mutex_lock(&p->rd_lock);
  int ret = cpu_read_pipe_nb_nolock(v, e);
  pthread_mutex_unlock(&p->rd_lock);
  return ret;
}

MAYBE_UNUSED
EXPORT int
cpu_peek_pipe_nb(void *v, void *e)
{
  cpu_pipe_t *p = (cpu_pipe_t*)v;
  pthread_mutex_lock(&p->rd_lock);
  int ret = cpu_peek_pipe_nb_nolock(v, e);
  pthread_mutex_unlock(&p->rd_lock);
  return ret;
}

/*
 * 6.13.16.2 - work-item builtins, reservation, locking
 */

MAYBE_UNUSED
EXPORT void *
cpu_reserve_read_pipe(void *v, unsigned n)
{
  cpu_pipe_t *p = (cpu_pipe_t*)v;
#ifdef PIPE_VERBOSE
  printf("cpu_reserve_read_pipe %p %d\n", v, n);
#endif
  if (!v) return 0;

  pthread_mutex_lock(&p->rd_lock);

  std::size_t tail;
  if (p->rd_rids.size()) {
    cpu_pipe_reserve_id_t *id = p->rd_rids.back();
    tail = id->tail;
  }
  else {
    tail = p->tail;
  }

  int space = p->head - tail;
  if (space < 0)
    space += p->pipe_size;

  cpu_pipe_reserve_id_t *rid = 0;
  if ((int)n <= space) {
    rid = (cpu_pipe_reserve_id_t*)malloc(sizeof(cpu_pipe_reserve_id_t));
    if (rid) {
      // success
      rid->tail = tail;
      rid->next = (tail + (p->pkt_size*n)) % p->pipe_size;
      rid->size = n*p->pkt_size;
      rid->ref = 1;
      p->rd_rids.push_back(rid);
    }
  }

  pthread_mutex_unlock(&p->rd_lock);
  return rid;
}

MAYBE_UNUSED
EXPORT void
cpu_commit_read_pipe(void *v, void *r)
{
  cpu_pipe_t *p = (cpu_pipe_t*)v;
  cpu_pipe_reserve_id_t *rid = (cpu_pipe_reserve_id_t *)r;
#ifdef PIPE_VERBOSE
  printf("cpu_commit_read_pipe %p %p\n", v, r);
#endif

  pthread_mutex_lock(&p->rd_lock);

  rid->ref--;
  assert(rid->ref == 0 && "bad commit on read pipe");

  while (p->rd_rids.size() && !p->rd_rids.front()->ref) {
    cpu_pipe_reserve_id_t *front = p->rd_rids.front();
    p->tail = front->next;
    p->rd_rids.pop_front();
    free(front);
  }

  pthread_mutex_unlock(&p->rd_lock);
}

MAYBE_UNUSED
EXPORT int
cpu_read_pipe_reserve(void *v, void *r, unsigned idx, void *e)
{
  cpu_pipe_t *p = (cpu_pipe_t*)v;
  cpu_pipe_reserve_id_t *rid = (cpu_pipe_reserve_id_t *)r;
#ifdef PIPE_VERBOSE
  printf("cpu_read_pipe_reserve %p %p %d %p\n", v, r, idx, e);
#endif
  if (!p || !rid)
    return -1;

  std::size_t offset = idx*p->pkt_size;
  if (offset > (rid->size+p->pkt_size))
    return -1;

  offset = (rid->tail + offset) % p->pipe_size;
  std::memcpy(e, &p->buf[offset], p->pkt_size);

  return 0;
}

MAYBE_UNUSED
EXPORT void *
cpu_reserve_write_pipe(void *v, unsigned n)
{
  cpu_pipe_t *p = (cpu_pipe_t*)v;
#ifdef PIPE_VERBOSE
  printf("cpu_reserve_write_pipe %p %d\n", v, n);
#endif
  if (!v) return 0;

  pthread_mutex_lock(&p->wr_lock);

  std::size_t head;
  if (p->wr_rids.size()) {
    cpu_pipe_reserve_id_t *id = p->wr_rids.back();
    head = id->head;
  }
  else {
    head = p->head;
  }

  int next = (head + p->pkt_size) % p->pipe_size;
  int space = p->tail - next;
  if (space < 0)
    space += p->pipe_size;

  cpu_pipe_reserve_id_t *rid = 0;
  if ((int)n <= space) {
    rid = (cpu_pipe_reserve_id_t*)malloc(sizeof(cpu_pipe_reserve_id_t));
    if (rid) {
      // success
      rid->head = head;
      rid->next = (head + (p->pkt_size*n)) % p->pipe_size;
      rid->size = n*p->pkt_size;
      rid->ref = 1;
      p->wr_rids.push_back(rid);
    }
  }

  pthread_mutex_unlock(&p->wr_lock);
  return rid;
}

MAYBE_UNUSED
EXPORT void
cpu_commit_write_pipe(void *v, void *r)
{
  cpu_pipe_t *p = (cpu_pipe_t*)v;
  cpu_pipe_reserve_id_t *rid = (cpu_pipe_reserve_id_t *)r;
#ifdef PIPE_VERBOSE
  printf("cpu_commit_write_pipe %p %p\n", v, r);
#endif
  pthread_mutex_lock(&p->wr_lock);

  rid->ref--;
  assert(rid->ref == 0 && "bad commit on write pipe");

  while (p->wr_rids.size() && !p->wr_rids.front()->ref) {
    cpu_pipe_reserve_id_t *front = p->wr_rids.front();
    p->head = front->next;
    p->wr_rids.pop_front();
    free(front);
  }

  pthread_mutex_unlock(&p->wr_lock);
}

MAYBE_UNUSED
EXPORT int
cpu_write_pipe_reserve(void *v, void *r, unsigned idx, void *e)
{
  cpu_pipe_t *p = (cpu_pipe_t*)v;
  cpu_pipe_reserve_id_t *rid = (cpu_pipe_reserve_id_t *)r;
#ifdef PIPE_VERBOSE
  printf("cpu_write_pipe_reserve %p %p %d %d\n", v, r, idx, *(int*)e);
#endif
  if (!p || !rid)
    return -1;

  std::size_t offset = idx*p->pkt_size;
  if (offset > (rid->size+p->pkt_size))
    return -1;

  offset = (rid->head + offset) % p->pipe_size;
  std::memcpy(&p->buf[offset], e, p->pkt_size);

  return 0;
}


/*
 * 6.13.16.3 work-group builtins
 */

MAYBE_UNUSED
EXPORT void *
cpu_work_group_reserve_read_pipe(void *v, unsigned n)
{
  return cpu_reserve_read_pipe(v,n);
}

MAYBE_UNUSED
EXPORT void *
cpu_work_group_reserve_write_pipe(void *v, unsigned n)
{
  return cpu_reserve_write_pipe(v,n);
}

MAYBE_UNUSED
EXPORT void
cpu_work_group_commit_read_pipe(void *v, void *r)
{
  cpu_commit_read_pipe(v,r);
}

MAYBE_UNUSED
EXPORT void
cpu_work_group_commit_write_pipe(void *v, void *r)
{
  cpu_commit_write_pipe(v,r);
}

/*
 * 6.13.16.4 pipe query functions
 */

MAYBE_UNUSED
EXPORT unsigned int
cpu_get_pipe_num_packets(void *v)
{
  cpu_pipe_t *p = (cpu_pipe_t*)v;

  pthread_mutex_lock(&p->rd_lock);
  std::size_t head = p->head;
  std::size_t tail;
  if (p->rd_rids.size()) {
    cpu_pipe_reserve_id_t *id = p->rd_rids.back();
    tail = id->tail;
  }
  else {
    tail = p->tail;
  }
  pthread_mutex_unlock(&p->rd_lock);

  int space = head - tail;
  if (space < 0)
    space += p->pipe_size;

  return (unsigned int)(space / p->pkt_size);
}

MAYBE_UNUSED
EXPORT unsigned int
cpu_get_pipe_max_packets(void *v)
{
  cpu_pipe_t *p = (cpu_pipe_t*)v;
  return (p->pipe_size / p->pkt_size) - 8;
}

}
#endif // _CPU_PIPES_H_
